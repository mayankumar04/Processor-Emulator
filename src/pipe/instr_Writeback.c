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

extern int64_t W_wval;

comb_logic_t wback_instr(w_instr_impl_t *in) {
    if(in->W_sigs.wval_sel)
        W_wval = in->val_mem;
    else
        W_wval = in->val_ex;
    return;
}