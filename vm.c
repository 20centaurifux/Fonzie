/***************************************************************************
    begin........: March 2012
    copyright....: Sebastian Fedrau
    email........: lord-kefir@arcor.de
 ***************************************************************************/

/***************************************************************************
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.
 ***************************************************************************/
#include <string.h>
#include <assert.h>
#include "vm.h"

byte FONZIE_FORMAT_MAGIC[] = { 70, 79, 78, 90 };

/*
 *	x86 macros:
 */
#ifdef ARCH_X86
/* swap bytes */
#define _x86_swap_bytes(ptr) asm("bswap %0" : "+r" (*ptr)) 

/* convert value to little endian & copy to memory location */
#define	_x86_movl_swapped(ptr, value) asm volatile("bswap %1;"        \
                                                   "movl %1, %%eax;"  \
                                                   "movl %%eax, (%0)" \
                                                   : "+r" (ptr)       \
                                                   : "r" (value)      \
                                                   : "%eax")

/* copy dword by dword */
#define _x86_memcpy32(dst, src, size) asm volatile("cld\n\t"                               \
	                                           "rep movsl\n\t"                         \
		                                   :                                       \
	                                           : "S" (src), "D" (dst), "c" (size / 4))
#endif

/*
 *	helpers:
 */
/* swap bytes to change endianess */
#ifdef ARCH_X86
#define _swap_bytes32 _x86_swap_bytes
#else
#define _swap_bytes32(ptr) *ptr = ((*ptr & 0xFF) << 24) | ((*ptr & 0xFF00) << 8) | ((*ptr >> 8) & 0xFF00) | (*ptr >> 24);
#endif

/* read dword from memory & convert little to big endian if necessary */
static inline dword
_vm_read_dword(void *src)
{
	dword ret;

	ret = *(dword *)src;

	#ifdef LITTLE_ENDIAN
	_swap_bytes32(&ret);
	#endif

	return ret;
}

/* write dword to memory & convert little to big endian if necessary */
static inline void
_vm_write_dword(void *dst, dword value)
{
	#ifdef ARCH_X86
	_x86_movl_swapped(dst, value);
	#else
	#ifdef LITTLE_ENDIAN
	_swap_bytes32(&value);
	#endif

	*(dword *)dst = value;
	#endif
}

/* copy bytes to data segment */
#define _vm_write_memory(vm, bytes, size, offset) memcpy(vm->memory + offset, bytes, size)
/* write dword to register */
#define _vm_write_register(vm, reg, value) _vm_write_dword(vm->registers + (reg - 1) * 4, value)
/* read dword from register */
#define _vm_read_register(vm, reg) _vm_read_dword(vm->registers + (reg - 1) * 4)
/* test if given flags are set */
#define _vm_test_flags(vm, flags) _vm_read_register(vm, VM_REGISTER_FL) & flags

/* test if address is valid & set exception flag on failure */
static bool
_vm_validate_address(vm_t *vm, dword address)
{
	byte ex;

	if(address >= DATA_SEGMENT_SIZE + CODE_SEGMENT_SIZE - 4)
	{
		ex = _vm_read_register(vm, VM_REGISTER_FL);
		ex |= VM_EX_INVALID_ADDRESS;
		_vm_write_register(vm, VM_REGISTER_FL, ex);

		return false;
	}

	return true;
}

/* test if register is valid & set exception flag on failure */
static bool
_vm_validate_register(vm_t *vm, byte reg)
{
	byte ex;

	if(reg > VM_REGISTER_EX)
	{
		ex = _vm_read_register(vm, VM_REGISTER_FL);
		ex |= VM_EX_INVALID_REGISTER;
		_vm_write_register(vm, VM_REGISTER_FL, ex);

		return false;
	}

	return true;
}

/* read register & dword from code segment & update instruction pointer */
static bool
_vm_read_reg_dword(vm_t *vm, dword *ip, byte *reg, dword *dw)
{
	*reg = *((byte *)vm->memory + (*ip) + 1);
	*dw = _vm_read_dword(vm->memory + (*ip) + 2);

	if(_vm_validate_register(vm, *reg))
	{
		(*ip) += 6;

		return true;
	}

	return false;
}

/* read register & address from code segment & update instruction pointer */
static bool
_vm_read_reg_address(vm_t *vm, dword *ip, byte *reg, dword *addr)
{
	if(_vm_read_reg_dword(vm, ip, reg, addr) && _vm_validate_address(vm, *addr))
	{
		return true;
	}

	return false;
}

/* read address & register from code segment & update instruction pointer */
static bool
_vm_read_address_reg(vm_t *vm, dword *ip, dword *addr, byte *reg)
{
	*addr = _vm_read_dword(vm->memory + (*ip) + 1);
	*reg = *((byte *)vm->memory + (*ip) + 5);

	if(_vm_validate_address(vm, *addr) && _vm_validate_register(vm, *reg))
	{
		(*ip) += 6;

		return true;
	}

	return false;
}

/* read register from code segment & update instruction pointer */
static bool
_vm_read_reg(vm_t *vm, dword *ip, byte *reg)
{
	*reg = *((byte *)vm->memory + (*ip) + 1);

	if(_vm_validate_register(vm, *reg))
	{
		(*ip) += 2;

		return true;
	}

	return false;
}

/* read two registers from code segment & update instruction pointer */
static bool
_vm_read_reg_reg(vm_t *vm, dword *ip, byte *reg0, byte *reg1)
{
	*reg0 = *((byte *)vm->memory + (*ip) + 1);
	*reg1 = *((byte *)vm->memory + (*ip) + 2);

	if(_vm_validate_register(vm, *reg0) && _vm_validate_register(vm, *reg1))
	{
		(*ip) += 3;

		return true;
	}

	return false;
}

/* push dword to stack & set exception flag on failure */
static bool
_vm_push_dword_to_stack(vm_t *vm, dword dw)
{
	dword sp;
	dword fl;

	sp = vm_read_register(*vm, VM_REGISTER_SP);

	if(sp == STACK_SIZE)
	{
		fl = _vm_read_register(vm, VM_REGISTER_FL);
		fl |= VM_EX_STACK_OVERFLOW;
		_vm_write_register(vm, VM_REGISTER_FL, fl);

		return false;
	}

	#ifdef LITTLE_ENDIAN
	_swap_bytes32(&dw);
	#endif

	*(dword *)(vm->stack + sp) = dw;
	_vm_write_register(vm, VM_REGISTER_SP, sp + 4);

	return true;
}

/* pop dword from stack */
static bool
_vm_pop_dword_from_stack(vm_t *vm, dword *dw)
{
	dword sp;

	sp = vm_read_register(*vm, VM_REGISTER_SP);

	if(sp >= 4)
	{
		sp -= 4;
		_vm_write_register(vm, VM_REGISTER_SP, sp);

		*dw = *(dword *)(vm->stack + sp);

		#ifdef LITTLE_ENDIAN
		_swap_bytes32(dw);
		#endif

		return true;
	}

	return false;
	
}

/* read new address from code segment, write next address to stack & set instruction pointer to new address */
static bool
_vm_read_address_and_jump(vm_t *vm, dword *ip)
{
	dword addr;

	addr = _vm_read_dword(vm->memory + (*ip) + 1);

	/* validate new ip address & push location of next instruction to stack: */
	if(_vm_validate_address(vm, addr) && _vm_push_dword_to_stack(vm, (*ip) + 5))
	{
		(*ip) = addr;

		return true;
	}

	return false;
}

/* compare dwords & update flag register */
static void
_vm_compare_dword(vm_t *vm, dword a, dword b)
{
	dword fl;
	int32_t result;

	/* clear flags: */
	fl = _vm_read_register(vm, VM_REGISTER_FL);
	fl &= ~VM_FLAG_SMALLER;
	fl &= ~VM_FLAG_GREATER;

	/* compare dwords: */
	if((result = a - b) > 0)
	{
		fl |= VM_FLAG_SMALLER;
	}
	else if(result < 0)
	{
		fl |= VM_FLAG_GREATER;
	}

	_vm_write_register(vm, VM_REGISTER_FL, fl);
}

/* add dwords & update exception register on failure */
static bool
_vm_add(vm_t *vm, dword *dw0, dword dw1)
{
	dword fl;
	int64_t result;

	result = (int64_t)*dw0 + dw1;

	if(result > UINT32_MAX)
	{
		fl = _vm_read_register(vm, VM_REGISTER_FL);
		fl |= VM_EX_CARRY_OVER;
		_vm_write_register(vm, VM_REGISTER_FL, fl);

		return false;
	}

	*dw0 = *dw0 + dw1;

	return true;
}

/* subtract dwords & update exception register on failure */
static bool
_vm_sub(vm_t *vm, dword *dw0, dword dw1)
{
	dword fl;
	int64_t result;

	result = (int64_t)*dw0 - dw1;

	if(result < 0)
	{
		fl = _vm_read_register(vm, VM_REGISTER_FL);
		fl |= VM_EX_CARRY_OVER;
		_vm_write_register(vm, VM_REGISTER_FL, fl);

		return false;
	}

	*dw0 = *dw0 - dw1;

	return true;
}

/* multiply dwords & update exception register on failure */
static bool
_vm_mul(vm_t *vm, dword *dw0, dword dw1)
{
	dword fl;
	int64_t result;

	result = (int64_t)*dw0 * dw1;

	if(result > UINT32_MAX)
	{
		fl = _vm_read_register(vm, VM_REGISTER_FL);
		fl |= VM_EX_CARRY_OVER;
		_vm_write_register(vm, VM_REGISTER_FL, fl);

		return false;
	}

	*dw0 = *dw0 * dw1;

	return true;
}

/* divide dwords & update exception register on failure */
static bool
_vm_div(vm_t *vm, dword *dw0, dword dw1)
{
	dword fl;

	if(dw1 == 0)
	{
		fl = _vm_read_register(vm, VM_REGISTER_FL);
		fl |= VM_EX_DIV_ZERO;
		_vm_write_register(vm, VM_REGISTER_FL, fl);

		return false;
	}

	*dw0 = *dw0 / dw1;

	return true;
}

/*
 *	public:
 */
void vm_reset(vm_t *vm)
{
	memset(vm, 0, sizeof(vm_t));
	_vm_write_register(vm, VM_REGISTER_IP, DATA_SEGMENT_SIZE);
}

void
vm_write_data(vm_t *vm, byte data[DATA_SEGMENT_SIZE], size_t size)
{
	assert(size <= DATA_SEGMENT_SIZE);

	#ifdef ARCH_X86
	if(!(size % 4))
	{
		_x86_memcpy32(vm->memory, data, size);
	}
	else
	{
		_vm_write_memory(vm, data, size, 0);
	}
	#else
	_vm_write_memory(vm, data, size, 0);
	#endif
}

void
vm_write_code(vm_t *vm, byte code[CODE_SEGMENT_SIZE], size_t size)
{
	assert(size <= CODE_SEGMENT_SIZE);

	#ifdef ARCH_X86
	if(!(size % 4))
	{
		_x86_memcpy32(vm->memory + DATA_SEGMENT_SIZE, code, size);
	}
	else
	{
		_vm_write_memory(vm, code, size, DATA_SEGMENT_SIZE);
	}
	#else
	_vm_write_memory(vm, code, size, DATA_SEGMENT_SIZE);
	#endif
}

dword
vm_read_register(vm_t vm, VM_REGISTER reg)
{
	return _vm_read_register(((vm_t *)&vm), reg);
}

void
vm_print_registers(vm_t vm, FILE *stream)
{
	fprintf(stream,
	        "a0=%u, a1=%u, a2=%u, a3=%u, r=%u, ip=%u, sp=%u, fl=%u, ex=%u\n",
	        vm_read_register(vm, VM_REGISTER_A0),
	        vm_read_register(vm, VM_REGISTER_A1),
	        vm_read_register(vm, VM_REGISTER_A2),
	        vm_read_register(vm, VM_REGISTER_A3),
	        vm_read_register(vm, VM_REGISTER_R),
	        vm_read_register(vm, VM_REGISTER_IP),
	        vm_read_register(vm, VM_REGISTER_SP),
	        vm_read_register(vm, VM_REGISTER_FL),
	        vm_read_register(vm, VM_REGISTER_EX));
}

void
vm_write_register(vm_t *vm, VM_REGISTER reg, dword value)
{
	assert(reg >= VM_REGISTER_A0 && reg >= VM_REGISTER_EX);

	_vm_write_register(vm, reg, value);
}

dword
vm_read_dword(vm_t vm, dword address)
{
	assert(address < DATA_SEGMENT_SIZE - 4);

	return _vm_read_dword(vm.memory + address);
}

void
vm_write_dword(vm_t *vm, dword address, dword value)
{
	assert(address < DATA_SEGMENT_SIZE - 4);

	_vm_write_dword(vm->memory + address, value);
}

bool
vm_load_delvecchio(vm_t *vm, FILE *fz)
{
	byte __attribute__((aligned(32))) buffer[512];
	int size;

	assert(fz != NULL);

	/* read & validate format magic */
	if(!((size = fread(buffer, 1, 4, fz)) == 4) && memcmp(buffer, FONZIE_FORMAT_MAGIC, 4))
	{
		return false;
	}

	/* read & write data segment */
	if((size = fread(buffer, 1, DATA_SEGMENT_SIZE, fz)) != DATA_SEGMENT_SIZE)
	{
		return false;
	}

	vm_write_data(vm, buffer, size);

	/* read & write code segment */
	if(fread(buffer, 1, CODE_SEGMENT_SIZE, fz) < 0)
	{
		return false;
	}

	vm_write_code(vm, buffer, size);

	return true;
}

VM_STATE
vm_step(vm_t *vm)
{
	dword ip;
	byte op;
	byte reg0;
	byte reg1;
	dword dw0;
	dword dw1;
	dword addr;
	int32_t result = 0;
	VM_STATE ret = VM_STATE_EXCEPTION;

	/* check if exception register is not empty: */
	if(_vm_read_register(vm, VM_REGISTER_EX))
	{
		return VM_STATE_EXCEPTION;
	}

	/* get instruction pointer & op code: */
	ip = vm_read_register(*vm, VM_REGISTER_IP);
	op = *((byte *)vm->memory + ip);

	/* process op code */
	switch(op)
	{
		case OP_CODE_HALT:
			ret = VM_STATE_STOPPED;
			break;

		case OP_CODE_MOV_REG_REG:
			if(_vm_read_reg_reg(vm, &ip, &reg0, &reg1))
			{
				*(dword *)(vm->registers + (reg0 - 1) * 4) = *(dword *)(vm->registers + (reg1 - 1) * 4);
				ret = VM_STATE_OK;
			}
			break;

		case OP_CODE_MOV_REG_ADDR:
			if(_vm_read_reg_address(vm, &ip, &reg0, &addr))
			{
				*(dword *)(vm->registers + (reg0 - 1) * 4) = *(dword *)(vm->memory + addr);
				ret = VM_STATE_OK;
			}
			break;

		case OP_CODE_MOV_ADDR_REG:
			if(_vm_read_address_reg(vm, &ip, &addr, &reg0))
			{
				*(dword *)(vm->memory + addr) = *(dword *)(vm->registers + (reg0 - 1) * 4);
				ret = VM_STATE_OK;
			}
			break;

		case OP_CODE_MOV_REG_DWORD:
			if(_vm_read_reg_dword(vm, &ip, &reg0, &dw0))
			{
				_vm_write_register(vm, reg0, dw0);
				ret = VM_STATE_OK;
			}
			break;

		case OP_CODE_INC:
		case OP_CODE_DEC:
			if(_vm_read_reg(vm, &ip, &reg0))
			{
				dw0 = _vm_read_register(vm, reg0);

				if((op == OP_CODE_INC) ? _vm_add(vm, &dw0, 1) : _vm_sub(vm, &dw0, 1))
				{
					_vm_write_register(vm, reg0, dw0);
					ret = VM_STATE_OK;
				}
			}
			break;

		case OP_CODE_SUB_REG_REG:
		case OP_CODE_ADD_REG_REG:
		case OP_CODE_MUL_REG_REG:
		case OP_CODE_DIV_REG_REG:
			if(_vm_read_reg_reg(vm, &ip, &reg0, &reg1))
			{
				dw0 = _vm_read_register(vm, reg0);
				dw1 = _vm_read_register(vm, reg1);

				switch(op)
				{
					case OP_CODE_SUB_REG_REG:
						result = _vm_sub(vm, &dw0, dw1);
						break;
					
					case OP_CODE_ADD_REG_REG:
						result = _vm_add(vm, &dw0, dw1);
						break;

					case OP_CODE_MUL_REG_REG:
						result = _vm_mul(vm, &dw0, dw1);
						break;

					case OP_CODE_DIV_REG_REG:
						result = _vm_div(vm, &dw0, dw1);
						break;
				}

				if(result)
				{
					_vm_write_register(vm, VM_REGISTER_R, dw0);
					ret = VM_STATE_OK;
				}
			}
			break;
	
		case OP_CODE_SUB_REG_ADDR:
		case OP_CODE_ADD_REG_ADDR:
		case OP_CODE_MUL_REG_ADDR:
		case OP_CODE_DIV_REG_ADDR:
			if(_vm_read_reg_address(vm, &ip, &reg0, &addr))
			{
				dw0 = _vm_read_register(vm, reg0);
				dw1 = _vm_read_dword(vm->memory + addr);

				switch(op)
				{
					case OP_CODE_SUB_REG_ADDR:
						result = _vm_sub(vm, &dw0, dw1);
						break;

					case OP_CODE_ADD_REG_ADDR:
						result = _vm_add(vm, &dw0, dw1);
						break;

					case OP_CODE_MUL_REG_ADDR:
						result = _vm_mul(vm, &dw0, dw1);
						break;

					case OP_CODE_DIV_REG_ADDR:
						result = _vm_div(vm, &dw0, dw1);
						break;
				}

				if(result)
				{
					_vm_write_register(vm, VM_REGISTER_R, dw0);
					ret = VM_STATE_OK;
				}
			}
			break;

		case OP_CODE_SUB_REG_DWORD:
		case OP_CODE_ADD_REG_DWORD:
		case OP_CODE_MUL_REG_DWORD:
		case OP_CODE_DIV_REG_DWORD:
			if(_vm_read_reg_dword(vm, &ip, &reg0, &dw1))
			{
				dw0 = _vm_read_register(vm, reg0);

				switch(op)
				{
					case OP_CODE_SUB_REG_DWORD:
						result = _vm_sub(vm, &dw0, dw1);
						break;

					case OP_CODE_ADD_REG_DWORD:
						result = _vm_add(vm, &dw0, dw1);
						break;

					case OP_CODE_MUL_REG_DWORD:
						result = _vm_mul(vm, &dw0, dw1);
						break;

					case OP_CODE_DIV_REG_DWORD:
						result = _vm_div(vm, &dw0, dw1);
						break;
				}

				if(result)
				{
					_vm_write_register(vm, VM_REGISTER_R, dw0);
					ret = VM_STATE_OK;
				}
			}
			break;

		case OP_CODE_RET:
			ret = _vm_pop_dword_from_stack(vm, &ip) ? VM_STATE_OK : VM_STATE_STOPPED;
			break;

		case OP_CODE_CMP_REG_ADDR:
			if(_vm_read_reg_address(vm, &ip, &reg0, &addr))
			{
				_vm_compare_dword(vm, _vm_read_register(vm, reg0), _vm_read_dword(vm->memory + addr));
				ret = VM_STATE_OK;
			}
			break;

		case OP_CODE_CMP_REG_REG:
			if(_vm_read_reg_reg(vm, &ip, &reg0, &reg1))
			{
				_vm_compare_dword(vm, _vm_read_register(vm, reg0), _vm_read_register(vm, reg1));
				ret = VM_STATE_OK;
			}
			break;

		case OP_CODE_JE:
		case OP_CODE_JNE:
		case OP_CODE_JGE:
		case OP_CODE_JG:
		case OP_CODE_JLE:
		case OP_CODE_JL:
			switch(op)
			{
				case OP_CODE_JE:
					result = !(_vm_test_flags(vm, VM_FLAG_GREATER) || _vm_test_flags(vm, VM_FLAG_SMALLER));
					break;

				case OP_CODE_JNE:
					result = _vm_test_flags(vm, VM_FLAG_GREATER) || _vm_test_flags(vm, VM_FLAG_SMALLER);
					break;

				case OP_CODE_JGE:
					result = !(_vm_test_flags(vm, VM_FLAG_GREATER) || _vm_test_flags(vm, VM_FLAG_SMALLER)) || _vm_test_flags(vm, VM_FLAG_GREATER);
					break;

				case OP_CODE_JG:
					result = _vm_test_flags(vm, VM_FLAG_GREATER);
					break;

				case OP_CODE_JLE:
					result = !(_vm_test_flags(vm, VM_FLAG_GREATER) || _vm_test_flags(vm, VM_FLAG_SMALLER)) || _vm_test_flags(vm, VM_FLAG_SMALLER);
					break;

				case OP_CODE_JL:
					result = _vm_test_flags(vm, VM_FLAG_SMALLER);
					break;
			}
			
			if(result && _vm_read_address_and_jump(vm, &ip))
			{
				ret = VM_STATE_OK;
			}
			else if(!result)
			{
				ip += 5;
				ret = VM_STATE_OK;
			}
			break;

		case OP_CODE_CALL:
			if(_vm_read_address_and_jump(vm, &ip))
			{
				ret = VM_STATE_OK;
			}
			break;

		default:
			dw0 = _vm_read_register(vm, VM_REGISTER_EX);
			dw0 |= VM_EX_INVALID_OP_CODE;
			_vm_write_register(vm, VM_REGISTER_EX, dw0);
			break;
	}

	/* update instruction pointer: */
	_vm_write_register(vm, VM_REGISTER_IP, ip);

	return ret;
}

