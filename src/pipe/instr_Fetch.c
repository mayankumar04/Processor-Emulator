#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err_handler.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include "hw_elts.h"

extern machine_t guest;
extern mem_status_t dmem_status;

extern uint64_t F_PC;
static comb_logic_t 
select_PC(uint64_t pred_PC,                                     
          opcode_t D_opcode, uint64_t val_a,                    
          opcode_t M_opcode, bool M_cond_val, uint64_t seq_succ,
          uint64_t *current_PC) {
	if(D_opcode == OP_RET && val_a == RET_FROM_MAIN_ADDR){
        *current_PC = 0;
        return;
    }
    *current_PC = pred_PC;
    if(M_opcode == OP_B_COND){
        if(M_cond_val == false){
            *current_PC = seq_succ;
            return;
        }
    }
    if(D_opcode == OP_RET){
        *current_PC = val_a;
        return;
    }
}

static comb_logic_t 
predict_PC(uint64_t current_PC, uint32_t insnbits, opcode_t op, 
           uint64_t *predicted_PC, uint64_t *seq_succ) {
    if (!current_PC) {
        return;
    }
	*seq_succ = current_PC + 4;
	if (op == OP_B || op == OP_BL){
		*predicted_PC = current_PC + 4*(bitfield_s64(insnbits, 0, 26));
	}
	else if (op == OP_B_COND) {
		*predicted_PC = current_PC + 4*(bitfield_s64(insnbits, 5, 19));
	}
	else {
		*predicted_PC = *seq_succ;
	}
	return;
}

static
void fix_instr_aliases(uint32_t insnbits, opcode_t *op) {
    if (*op == OP_UBFM){
        if(bitfield_u32(insnbits, 10, 6) != 0b111111 && bitfield_u32(insnbits, 10, 6) + 1 == bitfield_u32(insnbits, 16, 6)){
            *op = OP_LSL;
        }
        else if(bitfield_u32(insnbits, 10, 6) == 0b111111){
            *op = OP_LSR;
        }
    }
    if(*op == OP_ANDS_RR && bitfield_u32(insnbits, 0, 5) == 0b11111)
        *op = OP_TST_RR;
    else if(*op == OP_TST_RR && bitfield_u32(insnbits, 0, 5) != 0b11111)
        *op = OP_ANDS_RR;
    if(*op == OP_SUBS_RR && bitfield_u32(insnbits, 0, 5) == 0b11111)
        *op = OP_CMP_RR;
    else if(*op == OP_CMP_RR && bitfield_u32(insnbits, 0, 5) != 0b11111)
        *op = OP_SUBS_RR;
    return;
}

comb_logic_t fetch_instr(f_instr_impl_t *in, d_instr_impl_t *out) {
    printf("yo");
    bool imem_err = 0;
    uint64_t current_PC = 0;
    select_PC(in->pred_PC, X_out->op, X_out->val_a, M_out->op, M_out->cond_holds, M_out->seq_succ_PC, &current_PC);
    if (!current_PC) {
        out->insnbits = 0xD4400000U;
        out->op = OP_HLT;
        out->print_op = OP_HLT;
        imem_err = false;
    }
    else {
        imem(current_PC, &(out->insnbits), &imem_err);
        out->op = itable[bitfield_u32(out->insnbits, 21, 11)];
        fix_instr_aliases(out->insnbits, &(out->op));
        if(imem_err || out->op == OP_ERROR){
            out->status = STAT_INS;
            in->status = STAT_INS;
        }
        else 
        {
            in->status = D_out->status;
            out->status = D_out->status;
        }
        out->print_op = out->op;
        out->this_PC = current_PC;
        predict_PC(current_PC, out->insnbits, out->op, &F_PC, &(out->seq_succ_PC));
        if (out->op == OP_ADRP) {
            out->seq_succ_PC = (out->seq_succ_PC >> 12) << 12;
        }
    }
    if (out->op == OP_HLT) {
        in->status = STAT_HLT;
        out->status = STAT_HLT;
    }
    
    return;
}
