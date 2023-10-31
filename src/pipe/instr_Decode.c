#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err_handler.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "forward.h"
#include "machine.h"
#include "hw_elts.h"

#define SP_NUM 31
#define XZR_NUM 32

extern machine_t guest;
extern mem_status_t dmem_status;

extern int64_t W_wval;

static comb_logic_t 
generate_DXMW_control(opcode_t op,
                      d_ctl_sigs_t *D_sigs, x_ctl_sigs_t *X_sigs, m_ctl_sigs_t *M_sigs, w_ctl_sigs_t *W_sigs) {
	//x sigs
	//val b sel
	if (op == OP_ADDS_RR || op == OP_SUBS_RR || op == OP_CMP_RR || op == OP_MVN || op == OP_ORR_RR || op == OP_EOR_RR || op == OP_ANDS_RR || op == OP_TST_RR) {
		X_sigs -> valb_sel = 1;
	}
	else {
		X_sigs -> valb_sel = 0;
	}
	//set cc
	if (op == OP_ADDS_RR || op == OP_SUBS_RR || op == OP_CMP_RR || op == OP_ANDS_RR || op == OP_TST_RR) {
		X_sigs -> set_CC = 1;
	}
	else {
		X_sigs -> set_CC = 0;
	}
	//m sigs
	//dmem read
	if (op == OP_LDUR) {
		M_sigs -> dmem_read = 1;
	}
	else {
		M_sigs -> dmem_read = 0;
	}
	//dmem write
	if (op == OP_STUR) {
		M_sigs -> dmem_write = 1;
	}
	else {
		M_sigs -> dmem_write = 0;
	}
	//w sigs
	//dst sel
	if (op == OP_BL) {
		W_sigs -> dst_sel = 1;
	}
	else {
		W_sigs -> dst_sel = 0;
	}
	//w val sel		
	if (op == OP_LDUR) {
		W_sigs -> wval_sel = 1;
	}
	else {
		W_sigs -> wval_sel = 0;
	}
	//w enable
	if (op == OP_STUR || op == OP_CMP_RR || op == OP_TST_RR || op == OP_B || op == OP_B_COND || op == OP_RET || op == OP_NOP || op == OP_HLT || op == OP_ERROR) {
		W_sigs -> w_enable = 0;
	}
	else {
		W_sigs -> w_enable = 1;
	}	
	return;
}

/*
 * Logic for extracting the immediate value for M-, I-, and RI-format instructions.
 * STUDENT TO-DO:
 * Extract the immediate value and write it to *imm.
 */

static comb_logic_t 
extract_immval(uint32_t insnbits, opcode_t op, int64_t *imm) {
	switch (op) {
		//M format
		case OP_LDUR:
		case OP_STUR:
			*imm = bitfield_s64(insnbits, 12, 9);
			break;
		//RI format
		case OP_ADD_RI:
		case OP_SUB_RI:
		case OP_UBFM:
		case OP_ASR:
			*imm = bitfield_u32(insnbits, 10, 12);
			break;
		case OP_LSL:
			*imm = 64 - bitfield_u32(insnbits, 16, 6);
			break;
		case OP_LSR:
			*imm = bitfield_u32(insnbits, 16, 6);
			break;
		//I1 format
		case OP_MOVK:
		case OP_MOVZ:
			*imm = bitfield_u32(insnbits, 5, 16);
			break;
		//I2 format
		case OP_ADRP:
			 *imm = ((bitfield_s64(insnbits, 29, 2) << 12) | (bitfield_s64(insnbits, 5, 19) << 14));
			break;
		default:
			*imm = 0;
			break;
	}
	return;
}


static comb_logic_t
decide_alu_op(opcode_t op, alu_op_t *ALU_op) {
    switch(op) {
		case OP_LDUR:
		case OP_STUR:
		case OP_ADRP:
		case OP_ADD_RI:
		case OP_ADDS_RR:
			*ALU_op = PLUS_OP;
			break;
		case OP_MOVK:
		case OP_MOVZ:
			*ALU_op = MOV_OP;
			break;	
		case OP_SUB_RI:
		case OP_SUBS_RR:
		case OP_CMP_RR:
			*ALU_op = MINUS_OP;
			break;
		case OP_MVN:
			*ALU_op = NEG_OP;
			break;
		case OP_ORR_RR:
			*ALU_op = OR_OP;
			break;
		case OP_EOR_RR:
			*ALU_op = EOR_OP;
			break;
		case OP_ANDS_RR:
		case OP_TST_RR:
			*ALU_op = AND_OP;
			break;
		case OP_LSL:
		case OP_UBFM:
			*ALU_op = LSL_OP;
			break;
		case OP_LSR:
			*ALU_op = LSR_OP;
			break;
		case OP_ASR:
			*ALU_op = ASR_OP;
			break;
		case OP_ERROR:
		default:
			*ALU_op = PASS_A_OP;
			break;
	}
	return;
}

comb_logic_t 
copy_m_ctl_sigs(m_ctl_sigs_t *dest, m_ctl_sigs_t *src) {
    dest->dmem_read = src->dmem_read;
	dest->dmem_write = src->dmem_write;
	return;
}

comb_logic_t 
copy_w_ctl_sigs(w_ctl_sigs_t *dest, w_ctl_sigs_t *src) {
   	dest->dst_sel = src->dst_sel;
   	dest->wval_sel = src->wval_sel;
   	dest->w_enable = src->w_enable;
	return;
}

comb_logic_t
extract_regs(uint32_t insnbits, opcode_t op, 
             uint8_t *src1, uint8_t *src2, uint8_t *dst) {
    switch (op)
    {
        case OP_LDUR:
            *src1 = (insnbits >> 5) & 0x1F;
            *src2 = XZR_NUM;
            *dst = (insnbits & 0x1F);
            break;
        case OP_STUR:
			*src1 = (insnbits >> 5) & 0x1F;
            *src2 = (insnbits & 0x1F);
            *dst = (insnbits & 0x1F);
			break;
        //RI format
        case OP_LSL:
        case OP_LSR:
        case OP_ADD_RI:
        case OP_SUB_RI:
        case OP_UBFM:
        case OP_ASR:
            *src1 = (insnbits >> 5) & 0x1F;
            *src2 = XZR_NUM;
            *dst = (insnbits & 0x1F);
            break;
        //I1 format
        case OP_MOVK:
			*src1 = (insnbits & 0x1F);
            *src2 = XZR_NUM;
            *dst = (insnbits & 0x1F);
            break;
        case OP_MOVZ:
        //I2 format
        case OP_ADRP:
            *src1 = XZR_NUM;
			*src2 = XZR_NUM;
            *dst = (insnbits & 0x1F);
            break;
        //RR format
        case OP_CMP_RR:
        case OP_TST_RR:
            *src1 = (insnbits >> 5) & 0x1F;
            *src2 = (insnbits >> 16) & 0x1F;
            *dst = XZR_NUM;
            break;
        case OP_MVN:
            *src1 = XZR_NUM;
            *src2 = (insnbits >> 16) & 0x1F;
            *dst = insnbits & 0x1F;
            break;
        case OP_ADDS_RR:
        case OP_SUBS_RR:
        case OP_ORR_RR:
        case OP_EOR_RR:
        case OP_ANDS_RR:
            *src1 = (insnbits >> 5) & 0x1F;
            *src2 = (insnbits >> 16) & 0x1F;
            *dst = insnbits & 0x1F;
            break;
        //B format
        case OP_RET:
            *src1 = (insnbits >> 5) & 0x1F;
            *src2 = XZR_NUM;
			*dst = X_out->dst;
            break;
        case OP_B:
		case OP_BL:
			*src1 = XZR_NUM;
            *src2 = XZR_NUM;
			*dst = 0x1E;
			break;
        case OP_B_COND:
			*src1 = XZR_NUM;
            *src2 = XZR_NUM;
			*dst = X_out->dst;
			break;
		//S format
        case OP_NOP:
			*src1 = XZR_NUM;
            *src2 = XZR_NUM;
			*dst = insnbits & 0x1F;
			break;
        case OP_HLT:
            *src1 = XZR_NUM;
            *src2 = XZR_NUM;
			*dst = 0;
            break;
        default:
            *dst = XZR_NUM;
            *src2 = XZR_NUM;
            *src1 = XZR_NUM;
            break;
	}
    return;
}

comb_logic_t decode_instr(d_instr_impl_t *in, x_instr_impl_t *out) {
	//pass op code
	out -> op = in -> op;
	out -> seq_succ_PC = in -> seq_succ_PC;
	//
	generate_DXMW_control(in -> op, NULL, &(out->X_sigs), &(out->M_sigs), &(out->W_sigs));
	decide_alu_op(in -> op, &(out->ALU_op));
	//cond
	if (in -> op == OP_B_COND) {
		out -> cond = in->insnbits & 0xF;
	}
	else {
		out->cond = X_out->cond;
	}
	//reg file 
	uint8_t src1 = 0;
	uint8_t src2 = 0;
	uint8_t dstt = 0;
	extract_regs(in->insnbits, in->op, &src1, &src2, &dstt);
	//fix registers
	if ((in->op != OP_ADD_RI && in->op != OP_SUB_RI) && dstt == 31)
		dstt = XZR_NUM;
	if ((in->op != OP_LDUR && in->op != OP_STUR && in->op != OP_ADD_RI && in->op !=OP_SUB_RI) && src1 == 31)
		src1 = XZR_NUM;
	if ((in->op == OP_ADDS_RR || in->op == OP_SUBS_RR || in->op == OP_CMP_RR || in->op == OP_MVN || in->op == OP_ORR_RR || in->op == OP_EOR_RR || in->op == OP_ANDS_RR || in->op == OP_TST_RR) && src2 == 31){
		src2 = XZR_NUM;
	}
	regfile(src1, src2, W_out->dst, W_wval, W_out->W_sigs.w_enable, &(out->val_a), &(out->val_b));

	if(in->op == OP_ADRP){
        out->val_a = in->seq_succ_PC;
    }
	out->dst = dstt;
	//imm
	extract_immval(in->insnbits, in->op, &(out->val_imm));
	//hw val
	if (in->op == OP_MOVZ || in->op == OP_MOVK || in->op == OP_ADRP) {
		out->val_hw = (bitfield_u32(in->insnbits, 21, 2)<<4);
	}
	else {
		out->val_hw = 0;
	}

	out -> status = in -> status;
	out ->print_op = in->print_op;
	if(X_out->op == OP_LDUR)
		X_out->W_sigs.w_enable = false;

	forward_reg(src1, src2, M_in->dst, M_out->dst, W_out->dst, M_in->val_ex,
				W_in->val_ex, W_in->val_mem, W_out->val_ex, W_out->val_mem,
				W_in->W_sigs.wval_sel, W_out->W_sigs.wval_sel, M_in->W_sigs.w_enable,
				W_in->W_sigs.w_enable, W_out->W_sigs.w_enable, &(out->val_a), &(out->val_b));
	if(X_out->op == OP_LDUR)
		X_out->W_sigs.w_enable = true;
}