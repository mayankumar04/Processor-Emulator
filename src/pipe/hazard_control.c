#include "machine.h"

extern machine_t guest;
extern mem_status_t dmem_status;

void pipe_control_stage(proc_stage_t stage, bool bubble, bool stall) {
    pipe_reg_t *pipe;
    switch(stage) {
        case S_FETCH: pipe = F_instr; break;
        case S_DECODE: pipe = D_instr; break;
        case S_EXECUTE: pipe = X_instr; break;
        case S_MEMORY: pipe = M_instr; break;
        case S_WBACK: pipe = W_instr; break;
        default: printf("Error: incorrect stage provided to pipe control.\n"); return;
    }
    if (bubble && stall) {
        printf("Error: cannot bubble and stall at the same time.\n");
        pipe->ctl = P_ERROR;
    }
    // If we were previously in an error state, stay there.
    if (pipe->ctl == P_ERROR) return;

    if (bubble) {
        pipe->ctl = P_BUBBLE;
    }
    else if (stall) {
        pipe->ctl = P_STALL;
    }
    else { 
        pipe->ctl = P_LOAD;
    }
}

// method not really needed but oh well
// just checks to see if it's the RET case
bool check_ret_hazard(opcode_t D_opcode) {
    return D_opcode == OP_RET;
}

// just checks to see if the branch was wrong or not
// true: branch prediction was wrong, false: prediction was right
bool check_mispred_branch_hazard(opcode_t X_opcode, bool X_condval) {
    return X_opcode == OP_B_COND && !X_condval;
}

// checks to see if load is happening back to back or not
// highkey this one seems off because parameter isn't used 
bool check_load_use_hazard(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2,
                           opcode_t X_opcode, uint8_t X_dst) {
    return X_opcode == OP_LDUR && (D_src1 == X_dst || D_src2 == X_dst);
}

comb_logic_t handle_hazards(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2, 
                            opcode_t X_opcode, uint8_t X_dst, bool X_condval) {

    bool cond = check_mispred_branch_hazard(X_opcode, X_condval),
         load = check_load_use_hazard(D_opcode, D_src1, D_src2, X_opcode, X_dst),
         ret = check_ret_hazard(D_opcode),
		 f_stall = F_out->status == STAT_HLT || F_out->status == STAT_INS,
		 x_stall = X_out->status == STAT_HLT || M_out->status == STAT_ADR || M_out->status == STAT_INS,
		 m_stall = M_out->status == STAT_HLT || M_out->status == STAT_ADR || M_out->status == STAT_INS,
		 w_stall = W_out->status == STAT_HLT || W_out->status == STAT_ADR || W_out->status == STAT_INS;
	
	//default
	pipe_control_stage(S_FETCH, false, f_stall);
	pipe_control_stage(S_DECODE, false, false);
	pipe_control_stage(S_EXECUTE, false, false);
	pipe_control_stage(S_MEMORY, false, false);
	pipe_control_stage(S_WBACK, false, false);
	
	if(load){
		pipe_control_stage(S_FETCH, false, true); // stall
		pipe_control_stage(S_DECODE, false, true); // stall
		pipe_control_stage(S_EXECUTE, true, false); // bubble
	}
	else if(cond){
		pipe_control_stage(S_DECODE, true, false); // bubble
		pipe_control_stage(S_EXECUTE, true, false); // bubble
	}
	else if(ret){
		pipe_control_stage(S_FETCH, false, true); //stall
		pipe_control_stage(S_DECODE, true, false); // bubble
	}

	if(dmem_status == IN_FLIGHT){
		pipe_control_stage(S_FETCH, false, true);
		pipe_control_stage(S_DECODE, false, true);
		pipe_control_stage(S_EXECUTE, false, true);
		pipe_control_stage(S_MEMORY, false, true);
		pipe_control_stage(S_WBACK, false, true);
	}

	//exceptions
	if (x_stall){
		pipe_control_stage(S_FETCH, false, true);
		pipe_control_stage(S_DECODE, false, true);
		pipe_control_stage(S_EXECUTE, false, true);
	}
	if (m_stall) {
		pipe_control_stage(S_FETCH, false, true);
		pipe_control_stage(S_DECODE, false, true);
		pipe_control_stage(S_EXECUTE, false, true);
		pipe_control_stage(S_MEMORY, false, true);
	}
	if (w_stall){
		pipe_control_stage(S_FETCH, false, true);
		pipe_control_stage(S_DECODE, false, true);
		pipe_control_stage(S_EXECUTE, false, true);
		pipe_control_stage(S_MEMORY, false, true);
		pipe_control_stage(S_WBACK, false, true);
	}
}