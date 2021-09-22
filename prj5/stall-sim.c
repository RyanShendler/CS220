#include "stall-sim.h"

#include "y86-util.h"

#include "errors.h"
#include "memalloc.h"

#include <assert.h>

enum {
  MAX_DATA_BUBBLES = 3,  /** max # of bubbles due to data hazards */
  JUMP_BUBBLES = 2,      /** # of bubbles for cond jump op */
  RET_BUBBLES = 3,       /** # of bubbles for return op */
  MAX_REG_WRITE = 2      /** max # of registers written per clock cycle */
};

struct StallSimStruct {
  Y86 *y86;
  int clock;
  //int read[6];
  int write[6];
  int regClock;
  int stallTimer; //indicates how many stalls in a row
  bool stalling; //indicates if y86 was stalled last clock cycle
};


/********************** Allocation / Deallocation **********************/

/** Create a new pipeline stall simulator for y86. */
StallSim *
new_stall_sim(Y86 *y86)
{
  StallSim *sim = malloc(sizeof(struct StallSimStruct));
  sim->y86 = y86;
  sim->clock = 0;
  sim->stallTimer=0;
  sim->stalling=false;
  sim->regClock = 0;
  for(int i = 0; i < 6; i++){
	  //sim->read[i]=-1;
	  sim->write[i]=-1;
  }
  return sim;
}

/** Free all resources allocated by new_pipe_sim() in stallSim. */
void
free_stall_sim(StallSim *stallSim)
{
  free(stallSim);
}

/*void
insert_read(int reg1, int reg2, StallSim *stallSim){
	int regClock = stallSim->regClock;
	switch(regClock){
		case 0:
			stallSim->read[0] = reg1;
			stallSim->read[1] = reg2;
			break;
		case 1:
			stallSim->read[2] = reg1;
			stallSim->read[3] = reg2;
			break;
		case 2:
			stallSim->read[4] = reg1;
			stallSim->read[5] = reg2;
			break;
		default:
			break;
	}
}*/

void
insert_written(int reg1, int reg2, StallSim *stallSim){
	int regClock = stallSim->regClock;
	switch(regClock){
		case 0:
			stallSim->write[0] = reg1;
			stallSim->write[1] = reg2;
			break;
		case 1:
			stallSim->write[2] = reg1;
			stallSim->write[3] = reg2;
			break;
		case 2:
			stallSim->write[4] = reg1;
			stallSim->write[5] = reg2;
			break;
		default:
			break;
	}
	/*printf("write is: ");
	for(int i = 0; i<6; i++){
		printf("%d ", stallSim->write[i]);
	}
	printf("\n");*/
}

void
check_reg(Byte opcode, StallSim *stallSim){
	Address pc = read_pc_y86(stallSim->y86);
	int reg1 = -1;
	int reg2 = -1;
	switch(opcode){ //check register being written
		case 2: //rrmov and cmov
			reg1 = read_memory_byte_y86(stallSim->y86, pc+1);
			if(read_status_y86(stallSim->y86) != STATUS_AOK) return;
			reg1 = (reg1 & 0xF);
			break;
		case 3: //irmov
			reg1 = read_memory_byte_y86(stallSim->y86, pc+1);
			reg1 = (reg1 & 0xF);
			break;
		case 5: //mrmov
			reg1 = read_memory_byte_y86(stallSim->y86, pc+1);
			reg1 = ((reg1 >> 4) & 0xF);
			break;
		case 6: //op
			reg1 = read_memory_byte_y86(stallSim->y86, pc+1);
			reg1 = (reg1 & 0xF);
			break;
		case 8: //call
			reg1 = 4;
			break;
		/*case 9: //ret
			reg1 = 4;
			break;*/
		case 10: //push
			reg1 = 4;
			break;
		case 11: //pop
			reg1 = read_memory_byte_y86(stallSim->y86, pc+1);
			reg1 = ((reg1 >> 4) & 0xF);
			reg2 = 4;
			break;
		default:
			break;
	}
	insert_written(reg1, reg2, stallSim);
	if(stallSim->regClock != 2){
		stallSim->regClock++;
	} else {
		stallSim->regClock=0;
	}
}

Byte
read_opcode(Y86 *y86){
	Address pc = read_pc_y86(y86);
	Byte opcode = read_memory_byte_y86(y86, pc);
	if(read_status_y86(y86) != STATUS_AOK) return;
	opcode = ((opcode >> 4) & 0xF);
	return opcode;
}

//return true if data hazard detected, return false if no data hazards
bool
check_data_hazard(Byte opcode, StallSim *stallSim){
	int reg1 = -1; //registers read by current instruction
	int reg2 = -1;
	Address pc = read_pc_y86(stallSim->y86);
	switch(opcode){
		case 2: //rrmov and cmov
			reg1 = read_memory_byte_y86(stallSim->y86, pc+1);
			reg1 = ((reg1 >> 4) & 0xF);
			break;
		case 4: //rmmov
			reg1 = read_memory_byte_y86(stallSim->y86, pc+1);
			reg1 = ((reg1 >> 4) & 0xF);
			break;
		case 5: //mrmov
			reg1 = read_memory_byte_y86(stallSim->y86, pc+1);
			reg1 = (reg1 & 0xF);
			break;
		case 6: //op
			reg1 = read_memory_byte_y86(stallSim->y86, pc+1);
			reg2 = reg1;
			reg1 = ((reg1 >> 4) & 0xF);
			reg2 = (reg2 & 0xF);
			break;
		case 8: //call
			reg1 = 4;
			break;
		/*case 9: //ret
			break;*/
		case 10: //push
			reg1 = read_memory_byte_y86(stallSim->y86, pc+1);
			reg1 = ((reg1 >> 4) & 0xF);
			reg2 = 4;
			break;
		case 11: //pop
			reg1 = 4;
			break;
		default:
			break;
	}
	for(int i=0; i<6; i++){
		int written = stallSim->write[i];
		//printf("written is %d\n", written);
		if(written != -1){
			if(written == reg1){
				return true;
			} else if(written == reg2){
				return true;
			}
		}
	}
	return false;
}
/** Apply next pipeline clock to stallSim.  Return true if
 *  processor can proceed, false if pipeline is stalled.
 *
 * The pipeline will stall under the following circumstances:
 *
 * Exactly 4 clock cycles on startup to allow the pipeline to fill up.
 *
 * Exactly 2 clock cyclies after execution of a conditional jump.
 *
 * Exactly 3 clock cycles after execution of a return.
 *
 * Upto 3 clock cycles when attempting to read a register which was
 * written by any of upto 3 preceeding instructions.
 */
bool
clock_stall_sim(StallSim *stallSim)
{
  //Byte opcode = read_opcode(stallSim->y86);
  //printf("opcode is %d\n", opcode);
  //check_reg(opcode, stallSim); //checks which registers are being written by current instruction
  Byte opcode = -1;
  bool stall = true; //signals stall if needed
  int clock = stallSim->clock;
  if(clock < 4){ //stall on startup
	  stall = false;
  } else {
	  opcode = read_opcode(stallSim->y86);
	  //printf("opcode is %d\n", opcode);
	 // check_reg(opcode, stallSim);
  }
  if(opcode == Jxx_CODE){ //stall after conditional jump
	  if(stallSim->stalling == false){ 
		  stallSim->stallTimer = 2;
		  stallSim->stalling = true;
		  stall = false;
	  } else {
		  if(stallSim->stallTimer != 1){
			  stallSim->stallTimer--;
			  stallSim->stalling = true;
			  stall=false;
		  } else {
			  stallSim->stalling = false;
			  stallSim->stallTimer = 0;
		  }
	  }
  }
  else if(opcode == RET_CODE){ //stall after return
	  if(stallSim->stalling == false){
		  stallSim->stallTimer = 3;
		  stallSim->stalling = true;
		  stall = false;
	  } else {
		  if(stallSim->stallTimer != 1){
			  stallSim->stallTimer--;
			  stallSim->stalling = true;
			  stall=false;
		  } else {
			  stallSim->stalling = false;
			  stallSim->stallTimer = 0;
		  }
	  }
  }
  else if(check_data_hazard(opcode, stallSim)){
  	stall = false;
  }

  if(clock >= 4){
	  if(!check_data_hazard(opcode, stallSim)){
	  	check_reg(opcode, stallSim);
	  } else{
		  insert_written(-1, -1, stallSim);
		  if(stallSim->regClock != 2){
			  stallSim->regClock++;
		  } else{
			  stallSim->regClock=0;
		  }
	  }
  }
  clock++; //increment clock
  stallSim->clock = clock;
  //printf("stalling is %d\n", stallSim->stalling);
  return stall;
}
