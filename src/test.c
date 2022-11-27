/*
 * Copyright 2014 Killswitch
 *
 * This file is part of Arma Server Monitor for Linux.
 *
 * Arma Server Monitor for Linux is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Arma Server Monitor for Linux is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Arma Server Monitor for Linux; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "asm.h"

#define SLEEP 60

typedef void (*callextension)(char *output, int outputSize, const char *function);

int main(void)
{
	char output[OUTPUTSIZE];
	int instance = -1;
	char* error;
	void* handle = NULL;
	callextension RVExtension = NULL;


	dlerror();
	handle = dlopen("./asmdll.so", RTLD_LAZY);

	if (!handle) {
		handle = dlopen("asmdll.so", RTLD_LAZY);
	}

	if (!handle) {
		fprintf(stderr, "Could not open the ASM library: %s\n", dlerror());
		return -1;
	}

	dlerror();
	RVExtension = (callextension)dlsym(handle, "RVExtension");
	if ((error = dlerror()) != 0L)
	{
		perror(error);
		goto close;
	}

	if (RVExtension == NULL)
	{
		fprintf(stderr, "Could not load function\n");
		goto close;
	}

	(*RVExtension)(output, OUTPUTSIZE, "version");
	printf("Library version = %s\n", output);


	// 9: INIT, "ASMdll" callExtension format ["9:%1", profileName]
	(*RVExtension)(output, OUTPUTSIZE, "9:test");
	printf("init: %s\n", output);

	(*RVExtension)(output, OUTPUTSIZE, "id");
	printf("Instance id = %s\n", output);
	instance = atoi(output);

	// 0: FPS update, "ASMdll" RVExtension format ["0:%1:%2", round (diag_fps*1000),round (diag_fpsmin*1000)];
	(*RVExtension)(output, OUTPUTSIZE, "0:50:25");

	// 1: CPS update, "ASMdll" RVExtension format [""1:%1"", _c];
	(*RVExtension)(output, OUTPUTSIZE, "1:10");

	// 2: GEN update:
	// "_Players = {(alive _x)&&(isPlayer _x)} count _units;" \n
        // "_locAIs = {(alive _x)&&(local _x)} count _units;" \n
        // "_remAIs = ({alive _x} count _units) - _Players - _locAIs;" \n"
	// ""ASMdll"" RVExtension format [""2:%1:%2:%3"", _Players, _locAIs, _remAIs];"
	(*RVExtension)(output, OUTPUTSIZE, "2:8:100:50");
	printf("%s\n", output);

	// 3: MISSION update, "ASMdll" RVExtension format ["3:%1",  missionName];
	(*RVExtension)(output, OUTPUTSIZE, "3:asmtest");

	// 4: OCC0 update, "ASMdll" RVExtension format ["4:%1", _oc0];
	(*RVExtension)(output, OUTPUTSIZE, "4:4");

	// 5: OCC1 update, "ASMdll" RVExtension format ["5:%1", _oc1];
	(*RVExtension)(output, OUTPUTSIZE, "5:5");

	// 6: OCC2 update, "ASMdll" RVExtension format ["6:%1", _oc2];
	(*RVExtension)(output, OUTPUTSIZE, "6:6");

	// Simulate a long-running A3 server instance
	printf("Instance %d: sleeping for %d seconds...\n", instance, SLEEP);
	sleep(SLEEP);
	printf("Instance %d: sleep done.\n", instance);

close:
	dlclose(handle);

	printf("All done\n");
	return 0;
}
