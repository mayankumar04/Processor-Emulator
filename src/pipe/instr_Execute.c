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

extern bool X_condval;

extern comb_logic_t copy_m_ctl_sigs(m_ctl_sigs_t *, m_ctl_sigs_t *);
extern comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *, w_ctl_sigs_t *);

comb_logic_t execute_instr(x_instr_impl_t *in, m_instr_impl_t *out) {
    if(in->W_sigs.w_enable == 1) {
        out->dst = in->dst;
    }
    else {
        out->dst = 32;
    }
    copy_m_ctl_sigs(&(out->M_sigs), &(in->M_sigs));
    copy_w_ctl_sigs(&(out->W_sigs), &(in->W_sigs));
    out->val_b = in->val_b;
    out->op = in->op;
    out->print_op = in->print_op;
    out->seq_succ_PC = in->seq_succ_PC;
    out->status = in->status;
    if (in->op == OP_BL) {
        in->val_a = in->seq_succ_PC;
    }
    if (in->ALU_op == ERROR_OP){
        return;
    }
    if(in->X_sigs.valb_sel){
        alu(in->val_a, in->val_b, in->val_hw, in->ALU_op, in->X_sigs.set_CC, in->cond, &(out->val_ex), &X_condval);
    }
    else{
        alu(in->val_a, in->val_imm, in->val_hw, in->ALU_op, in->X_sigs.set_CC, in->cond, &(out->val_ex), &X_condval);
    }
    out->cond_holds = X_condval;
    return;
}