#include <stdbool.h>
#include "forward.h"

comb_logic_t forward_reg(uint8_t D_src1, // src1 from which decode is reading ***USED***
                        uint8_t D_src2, // src2 from whcih decode is reading ***USED***
                        uint8_t X_dst, // the destination to which X is trying to write ***USED***
                        uint8_t M_dst, // the destination to which M is trying to write ***USED***
                        uint8_t W_dst, // the destination to which W is trying to write ***USED***
                        uint64_t X_val_ex, // the value that X has computed (could be memory address or value to be written) ***USED***
                        uint64_t M_val_ex, // the value that X calculated (just copied in M register) ***USED***
                        uint64_t M_val_mem, // this is the value returned from memory (if reading from memory) ***USED***
                        uint64_t W_val_ex, // the value that X calculated (just copied in W register) ***USED***
                        uint64_t W_val_mem, // this is the value returned from memory (just copied in W register) ***USED***
                        bool M_wval_sel, // signal to choose val_ex or val_mem (1 means LDUR or val_mem and 0 means val_ex) ***USED***
                        bool W_wval_sel, // signal to choose val_ex or val_mem (1 means LDUR or val_mem and 0 means val_ex) ***USED***
                        bool X_w_enable, // signal to tell decode that writing will or will not occur (just copied in X register) ***USED***
                        bool M_w_enable, // signal to tell decode that writing will or will not occur (just copied in M register) ***USED***
                        bool W_w_enable, // signal to tell decode that writing will or will not occur (just copied in W register) ***USED***
                        uint64_t *val_a, // ***USED***
                        uint64_t *val_b) { // ***USED***

    // bools to check the updated status
    bool updated_a = false,
         updated_b = false;

    if(X_w_enable){
        if(D_src1 == X_dst){
            *val_a = X_val_ex;
            updated_a = true;
        }
        if(D_src2 == X_dst){
            *val_b = X_val_ex;
            updated_b = true;
        }
    }
    if(M_w_enable){
        if(D_src1 == M_dst && !updated_a){
            *val_a = M_wval_sel ? M_val_mem : M_val_ex;
            updated_a = true;
        }
        if(D_src2 == M_dst && !updated_b){
            *val_b = M_wval_sel ? M_val_mem : M_val_ex;
            updated_b = true;
        }
    }
    if(W_w_enable){
        if(D_src1 == W_dst && !updated_a){
            *val_a = W_wval_sel ? W_val_mem : W_val_ex;
            updated_a = true;
        }
        if(D_src2 == W_dst && !updated_b){
            *val_b = W_wval_sel ? W_val_mem : W_val_ex;
            updated_b = true;
        }
    }
}