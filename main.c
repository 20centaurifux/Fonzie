/***************************************************************************
    begin........: March 2012
    copyright....: Sebastian Fedrau
    email........: lord-kefir@arcor.de
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>

#include "vm.h"

int
main(int argc, char *argv[])
{
	vm_t vm;
	FILE *fz = NULL;
	VM_STATE state = VM_STATE_UNDEFINED;

	/* check arguments */
	if(argc != 2)
	{
		fprintf(stderr, "Wrong number of arguments.\n");
		return -1;
	}

	/* open binary */
	if((fz = fopen(argv[1], "rb")))
	{
		/* reset machine */
		vm_reset(&vm);

		/* load binary */
		if(vm_load_delvecchio(&vm, fz))
		{
			/* execute machine code */
			do
			{
				state = vm_step(&vm);
				vm_print_registers(vm, stdout);
			}
			while(state == VM_STATE_OK);

			fprintf(stdout, "state=%d\n", state);
		}
		else
		{
			fprintf(stderr, "Couldn't load file, format seems to be invalid.\n");
		}

		/* close file */
		fclose(fz);
	}
	else
	{
		fprintf(stderr, "Couldn't open file (err=%d).\n", errno);
	}

	return 0;
}

