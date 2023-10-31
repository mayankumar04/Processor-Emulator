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

//tells whether to read or write
//in include/pipe/instr_pipeline.h
extern comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *, w_ctl_sigs_t *);

comb_logic_t memory_instr(m_instr_impl_t *in, w_instr_impl_t *out) {

	bool err = false;
	if (in->M_sigs.dmem_read == 1 || in->M_sigs.dmem_write == 1)
		dmem(in->val_ex, in->val_b, in->M_sigs.dmem_read, in->M_sigs.dmem_write, &(out->val_mem), &err);
	
	if (err)
		out->status = STAT_ADR;
	else
		out->status = in->status;

	out->op = in->op;
	out->print_op = in->print_op;
	copy_w_ctl_sigs(&(out->W_sigs), &(in->W_sigs));
	out->dst = in->dst;
	out->val_b = in->val_b;
	out->val_ex = in->val_ex;
	M_out->cond_holds = in->cond_holds;
	return;
}
