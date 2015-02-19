/***************************************************************************
    begin........: March 2012
    copyright....: Sebastian Fedrau
    email........: sebastian.fedrau@gmail.com
 ***************************************************************************/

/***************************************************************************
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License v3 as published
    by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License v3 for more details.
 ***************************************************************************/
#ifndef __VM_H__
#define __VM_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define DATA_SEGMENT_SIZE 512
#define CODE_SEGMENT_SIZE 2048
#define STACK_SIZE        256

typedef uint8_t byte;
typedef uint32_t dword;

extern byte DELVECCHIO_FORMAT_MAGIC[];

typedef enum
{
	VM_REGISTER_A0 = 1,
	VM_REGISTER_A1 = 2,
	VM_REGISTER_A2 = 3,
	VM_REGISTER_A3 = 4,
	VM_REGISTER_R  = 5,
	VM_REGISTER_IP = 6,
	VM_REGISTER_SP = 7,
	VM_REGISTER_FL = 8,
	VM_REGISTER_EX = 9
} VM_REGISTER;

typedef enum
{
	OP_CODE_HALT                = 0,
	OP_CODE_MOV_REG_REG         = 1,
	OP_CODE_MOV_REG_ADDR        = 2,
	OP_CODE_MOV_ADDR_REG        = 3,
	OP_CODE_MOV_REG_DWORD       = 4,
	OP_CODE_MOV_REG_ADDR_IN_REG = 5,
	OP_CODE_INC                 = 6,
	OP_CODE_DEC                 = 7,
	OP_CODE_SUB_REG_REG         = 8,
	OP_CODE_SUB_REG_ADDR        = 9,
	OP_CODE_SUB_REG_DWORD       = 10,
	OP_CODE_ADD_REG_REG         = 11,
	OP_CODE_ADD_REG_ADDR        = 12,
	OP_CODE_ADD_REG_DWORD       = 13,
	OP_CODE_MUL_REG_REG         = 14,
	OP_CODE_MUL_REG_ADDR        = 15,
	OP_CODE_MUL_REG_DWORD       = 16,
	OP_CODE_DIV_REG_REG         = 17,
	OP_CODE_DIV_REG_ADDR        = 18,
	OP_CODE_DIV_REG_DWORD       = 19,
	OP_CODE_AND_REG_REG         = 20,
	OP_CODE_AND_REG_ADDR        = 21,
	OP_CODE_AND_REG_DWORD       = 22,
	OP_CODE_OR_REG_REG          = 23,
	OP_CODE_OR_REG_ADDR         = 24,
	OP_CODE_OR_REG_DWORD        = 25,
	OP_CODE_MOD_REG_REG         = 26,
	OP_CODE_MOD_REG_ADDR        = 27,
	OP_CODE_MOD_REG_DWORD       = 28,
	OP_CODE_RND                 = 29,
	OP_CODE_RET                 = 30,
	OP_CODE_CMP_REG_ADDR        = 31,
	OP_CODE_CMP_REG_REG         = 32,
	OP_CODE_JE                  = 33,
	OP_CODE_JNE                 = 34,
	OP_CODE_JGE                 = 35,
	OP_CODE_JG                  = 36,
	OP_CODE_JLE                 = 37,
	OP_CODE_JL                  = 38,
	OP_CODE_CALL                = 39,
	OP_CODE_PUSH                = 40,
	OP_CODE_POP                 = 41,
	OP_CODE_MOVS_REG_STACK      = 42,
	OP_CODE_MOVS_STACK_REG      = 43
} VM_OPCODE;

typedef enum
{
	VM_FLAG_SMALLER = 1,
	VM_FLAG_GREATER = 2
} VM_FLAG;

typedef enum
{
	VM_EX_INVALID_ADDRESS       = 1,
	VM_EX_INVALID_REGISTER      = 2,
	VM_EX_STACK_OVERFLOW        = 4,
	VM_EX_CARRY_OVER            = 8,
	VM_EX_DIV_ZERO              = 16,
	VM_EX_INVALID_OP_CODE       = 32,
	VM_EX_INVALID_STACK_ADDRESS = 64
} VM_EX;

typedef enum
{
	VM_STATE_UNDEFINED  = -1,
	VM_STATE_OK         = 0,
	VM_STATE_EXCEPTION  = 1,
	VM_STATE_STOPPED    = 2
} VM_STATE;

typedef struct
{
 	byte registers[36];
	byte memory[DATA_SEGMENT_SIZE + CODE_SEGMENT_SIZE];
	dword stack[STACK_SIZE / 4];
} vm_t;

void vm_reset(vm_t *vm);
void vm_write_data(vm_t *vm, byte data[DATA_SEGMENT_SIZE], size_t size);
void vm_write_code(vm_t *vm, byte code[CODE_SEGMENT_SIZE], size_t size);
void vm_print_registers(vm_t vm, FILE *stream);
dword vm_read_register(vm_t vm, VM_REGISTER reg);
void vm_write_register(vm_t *vm, VM_REGISTER reg, dword value);
dword vm_read_dword(vm_t vm, dword address);
void vm_write_dword(vm_t *vm, dword address, dword value);
bool vm_load_delvecchio(vm_t *vm, FILE *fz);
VM_STATE vm_step(vm_t *vm);
#endif

