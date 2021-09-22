#include "ysim.h"

#include "errors.h"

/************************** Utility Routines ****************************/

/** Return nybble from op (pos 0: least-significant; pos 1:
 *  most-significant)
 */
static Byte
get_nybble(Byte op, int pos) {
  return (op >> (pos * 4)) & 0xF;
}

/************************** Condition Codes ****************************/

/** Conditions used in instructions */
typedef enum {
  ALWAYS_COND, LE_COND, LT_COND, EQ_COND, NE_COND, GE_COND, GT_COND
} Condition;

/** accessing condition code flags */
static inline bool get_cc_flag(Byte cc, unsigned flagBitIndex) {
  return !!(cc & (1 << flagBitIndex));
}
static inline bool get_zf(Byte cc) { return get_cc_flag(cc, ZF_CC); }
static inline bool get_sf(Byte cc) { return get_cc_flag(cc, SF_CC); }
static inline bool get_of(Byte cc) { return get_cc_flag(cc, OF_CC); }

/** Return true iff the condition specified in the least-significant
 *  nybble of op holds in y86.  Encoding of Figure 3.15 of Bryant's
 *  CompSys3e.
 */
bool
check_cc(const Y86 *y86, Byte op)
{
  bool ret = false;
  Condition condition = get_nybble(op, 0);
  Byte cc = read_cc_y86(y86);
  switch (condition) {
  case ALWAYS_COND:
    ret = true;
    break;
  case LE_COND:
    ret = (get_sf(cc) ^ get_of(cc)) | get_zf(cc);
    break;
  case LT_COND:
    ret = (get_sf(cc) ^ get_of(cc));
    break;
  case EQ_COND:
    ret = get_zf(cc);
    break;
  case NE_COND:
    ret = !get_zf(cc);
    break;
  case GE_COND:
    ret = !(get_sf(cc) ^ get_of(cc)); 
    break;
  case GT_COND:
    ret = (!(get_sf(cc) ^ get_of(cc)) & !get_zf(cc)); 
    break;
  default: {
    Address pc = read_pc_y86(y86);
    fatal("%08lx: bad condition code %d\n", pc, condition);
    break;
    }
  }
  return ret;
}

/** return true iff word has its sign bit set */
static inline bool
isLt0(Word word) {
  return (word & (1UL << (sizeof(Word)*CHAR_BIT - 1))) != 0;
}

/** Set condition codes for addition operation with operands opA, opB
 *  and result with result == opA + opB.
 */
static void
set_add_arith_cc(Y86 *y86, Word opA, Word opB, Word result)
{
	Byte cc = 0;
	if(result == 0) cc |= 4; //set ZF if result is 0
	if(isLt0(result)) cc |= 2; //set SF if result is negative
	if(isLt0(opA) && isLt0(opB)){ //if opA and opB are both negative...
		if(!isLt0(result)) cc |= 1; //set OF if result is positive
	}
	if(!isLt0(opA) && !isLt0(opB)){ //if opA and opB are both positive...
		if(isLt0(result)) cc |= 1; //set OF if result is negative
	}
	//printf("CC is %d\n", cc);
	write_cc_y86(y86, cc);
}

/** Set condition codes for subtraction operation with operands opA, opB
 *  and result with result == opA - opB.
 */
static void
set_sub_arith_cc(Y86 *y86, Word opA, Word opB, Word result)
{
  Byte cc = 0;
  if(result == 0) cc |= 4; //set ZF if result is 0
  if(isLt0(result)) cc |= 2; //set SF if result is negative
  if(!isLt0(opA) && isLt0(opB)){ //if opA is positive and opB is negative...
	  if(isLt0(result)) cc |= 1; //set OF is result is negative
  }
  if(isLt0(opA) && !isLt0(opB)){ //if opA is negative and opB is positive...
	  if(!isLt0(result)) cc |= 1; //set OF if result is positive
  }
  //printf("CC is %d\n", cc);
  write_cc_y86(y86, cc);
}

static void
set_logic_op_cc(Y86 *y86, Word result)
{
  Byte cc = 0;
  if(result == 0) cc |= 4; //set ZF if result is 0
  if(isLt0(result)) cc |= 2; //set SF if result is negative
  //printf("CC is %d\n", cc);
  write_cc_y86(y86, cc);
}

/**************************** Operations *******************************/

static void
op1(Y86 *y86, Byte op, Register regA, Register regB)
{
  enum {ADDL_FN, SUBL_FN, ANDL_FN, XORL_FN };
  Word opA = read_register_y86(y86, regA);
  Word opB = read_register_y86(y86, regB);
  //printf("OpA is %lx, OpB is %lx\n", opA, opB);
  switch(op){
	  case ADDL_FN:{
		  Word result = opA + opB;
		  set_add_arith_cc(y86, opA, opB, result);
		  write_register_y86(y86, regB, result);
		  break;
	  }
	  case SUBL_FN:{
	          Word result = opB - opA;
		  //printf("Result is %lx\n", result);
		  set_sub_arith_cc(y86, opB, opA, result);
		  write_register_y86(y86, regB, result);
		  break;
	  }
	  case ANDL_FN:{
		  Word result = opA & opB;
		  set_logic_op_cc(y86, result);
		  write_register_y86(y86, regB, result);
		  break;
          }
          case XORL_FN:{
		  Word result = opA ^ opB;
		  set_logic_op_cc(y86, result);
		  write_register_y86(y86, regB, result);
		  break;
	  }
	  default:
	  {
		  Address pc = read_pc_y86(y86);
		  fatal("%081lx: bad operation code %d\n", pc, op);
		  break;
	  }
  }
}

/*********************** Single Instruction Step ***********************/

typedef enum {
  HALT_CODE, NOP_CODE, CMOVxx_CODE, IRMOVQ_CODE, RMMOVQ_CODE, MRMOVQ_CODE,
  OP1_CODE, Jxx_CODE, CALL_CODE, RET_CODE,
  PUSHQ_CODE, POPQ_CODE } BaseOpCode;

/** Execute the next instruction of y86. Must change status of
 *  y86 to STATUS_HLT on halt, STATUS_ADR or STATUS_INS on
 *  bad address or instruction.
 */
void
step_ysim(Y86 *y86)
{
  Address pc = read_pc_y86(y86);
  Byte opcode = read_memory_byte_y86(y86, pc);
  if(read_status_y86(y86) != STATUS_AOK) return;
  opcode = get_nybble(opcode, 1);
  switch(opcode){
	  case 0: //halt
		  write_status_y86(y86, STATUS_HLT);
		  break;
	  case 1: //nop
		  pc++;
		  write_pc_y86(y86, pc);
		  break;
          case 2: //cmov and rrmov
	  {
		  Byte cond = read_memory_byte_y86(y86, pc); //get condition code
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  cond = get_nybble(cond, 0);
		  if(check_cc(y86, cond)){ //excute move if condition is met
			  Byte regs = read_memory_byte_y86(y86, pc+1);
			  if(read_status_y86(y86) != STATUS_AOK) return;
			  Byte src = get_nybble(regs, 1); //get source register
			  Byte dest = get_nybble(regs, 0); //get destination register
			  Word val = read_register_y86(y86, src); //get value from source register
			  write_register_y86(y86, dest, val); //write src register value to dest register
		  }
		  pc = pc+1+sizeof(Byte); //increment pc
		  write_pc_y86(y86, pc);
		  break;
	  }
	  case 3: //irmovq
	  {
		  Byte reg = read_memory_byte_y86(y86, (pc+1)); //get register
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  reg = get_nybble(reg, 0); //get destination register
		  Word imm = read_memory_word_y86(y86, pc+1+sizeof(Byte)); //get source immediate
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  write_register_y86(y86, reg, imm); //write immediate ot destination register
		  pc = pc + 1 + sizeof(Byte) + sizeof(Word); //increment pc
		  write_pc_y86(y86, pc);
		  break;
	  }
	  case 4: //rmmovq
	  {
		  Byte regs = read_memory_byte_y86(y86, (pc+1)); //get registers
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  Byte src = get_nybble(regs, 1); //get source register
		  Byte dest = get_nybble(regs, 0); //get destination register
		  Word disp = read_memory_word_y86(y86, (pc+1+sizeof(Byte))); //get displacement
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  Word val = read_register_y86(y86, src); //get value from source register
		  Word d = read_register_y86(y86, dest); //get value from destination register
		  write_memory_word_y86(y86, d+disp, val); //write value to memory address d+disp
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  pc = pc + 1 + sizeof(Byte) + sizeof(Word); //increment pc
		  write_pc_y86(y86, pc);
		  break;
	  }
	  case 5: //mrmovq
	  {
		  Byte regs = read_memory_byte_y86(y86, (pc+1)); //get registers
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  Byte src = get_nybble(regs, 0); //get source register
		  Byte dest =  get_nybble(regs, 1); //get destination register
		  Word disp = read_memory_word_y86(y86, (pc+1+sizeof(Byte))); //get displacement
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  Word s = read_register_y86(y86, src); //get value from source register
		  Word val = read_memory_word_y86(y86, s+disp); //get value from memory
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  write_register_y86(y86, dest, val); //write value to dest register
		  pc = pc+1+sizeof(Byte)+sizeof(Word); //increment pc
		  write_pc_y86(y86, pc);
		  break;
	  }
	  case 6: //op
	  {
		  Byte op = read_memory_byte_y86(y86, pc); //get operation code
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  op = get_nybble(op, 0);
		  //printf("Opcode is %d\n", op);
		  Byte regs = read_memory_byte_y86(y86, pc+1); //get registers
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  Byte regA = get_nybble(regs, 1); //get register A
		  Byte regB = get_nybble(regs, 0); //get register B
		  op1(y86, op, regA, regB); //do operation
		  pc = pc+1+sizeof(Byte); //increment pc
		  write_pc_y86(y86, pc);
		  break;
	  }
	  case 7: //jmp
	  {
		  Byte cond = read_memory_byte_y86(y86, pc); //get condition code
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  cond = get_nybble(cond, 0);
		  if(check_cc(y86, cond)){
			  Word dest = read_memory_word_y86(y86, pc+1);
			  if(read_status_y86(y86) != STATUS_AOK) return;
			  write_pc_y86(y86, dest);
		  } else{
		  	pc = pc+1+sizeof(Word); //increment pc
		  	write_pc_y86(y86, pc);
		  }
     		  break;
	  }
	  case 8: //call
	  {
	  	Word dest = read_memory_word_y86(y86, pc+1); //get destination address
		if(read_status_y86(y86) != STATUS_AOK) return;
	  	Address ret_addr = pc+1+sizeof(Word); //get return address
		Word stack = read_register_y86(y86, 4); //get stack pointer
		stack = stack - sizeof(Address); //decrement stack pointer
		write_register_y86(y86, 4, stack); 
		write_memory_word_y86(y86, stack, ret_addr); //push return address to stack
		if(read_status_y86(y86) != STATUS_AOK) return;
		write_pc_y86(y86, dest); //jump to destination address
		break;
	  }
	  case 9: //ret
	  {
		Word stack = read_register_y86(y86, 4); //get stack pointer
		Word dest = read_memory_word_y86(y86, stack); //pop address from stack
		if(read_status_y86(y86) != STATUS_AOK) return;
		stack = stack + sizeof(Address); //increment stack pointer
		write_register_y86(y86, 4, stack);
		write_pc_y86(y86, dest);
		break;
	  }
	  case 10: //push
	  {
		  Byte reg = read_memory_byte_y86(y86, pc+1);
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  reg = get_nybble(reg, 1); //get source register
		  Word value = read_register_y86(y86, reg); //get value of register
		  Word stack = read_register_y86(y86, 4); //get stack pointer
		  stack = stack - sizeof(Word); //decrement stack pointer
		  write_register_y86(y86, 4, stack);
		  write_memory_word_y86(y86, stack, value); //push value to stack
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  pc = pc+1+sizeof(Byte); //pc
		  write_pc_y86(y86, pc);
		  break;
	  }
	  case 11: //pop
	  {
		  Byte reg = read_memory_byte_y86(y86, pc+1);
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  reg = get_nybble(reg, 1); //get destination register
		  Word stack = read_register_y86(y86, 4); //get stack pointer
		  Word value = read_memory_word_y86(y86, stack); //pop address from stack
		  if(read_status_y86(y86) != STATUS_AOK) return;
		  write_register_y86(y86, reg, value);
		  if(reg != 4){
		  	stack = stack+sizeof(Word); //increment stack pointer
		  	write_register_y86(y86, 4, stack);
		  }
		  pc = pc+1+sizeof(Byte); //increment pc
		  write_pc_y86(y86, pc);
		  break;
	  }
	  default:
		  write_status_y86(y86, STATUS_INS);
  }
}
