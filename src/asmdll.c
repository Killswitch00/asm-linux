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

#include "asm.h"
#include "asmlog.h"
#include "settings.h"
#include "asmdll.h"
#include "gettickcount.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static long pagesize;
static int FileMapHandle = -1;
static void* FileMap;
static uint32_t InstanceID;

static struct ARMA_SERVER_INFO *ArmaServerInfo = NULL;
static struct stat filestat;
static struct timespec T0;

void __attribute ((constructor)) libasm_open(void)
{
	int firstload = 0;
	char *debug = getenv("ASM_DEBUG");

	if (debug && !strcmp(debug, "1")) {
		asmlog_enable_debug();
	}
	asmlog_stdout("asmdll");

	asmlog_info(PACKAGE_STRING);

	pagesize = sysconf(_SC_PAGESIZE);
	FileMapHandle = shm_open("/ASM_MapFile", O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

	if (FileMapHandle < 0) {
		asmlog_error("Could not create shared memory object: %s", strerror(errno));
		return;
	}
	memset(&filestat, 0, sizeof(filestat));
	if (fstat(FileMapHandle, &filestat) != 0) {
		asmlog_error("Could not fstat() the shared memory object: %s", strerror(errno));
		shm_unlink("/ASM_MapFile");
		close(FileMapHandle);
		return;
	}
	if (filestat.st_size == 0) {
		// First load of the extension - resize the shared memory for our needs
		if (ftruncate(FileMapHandle, FILEMAPSIZE) != 0) {
			asmlog_error("Could not set shared memory object size: %s", strerror(errno));
			shm_unlink("/ASM_MapFile");
			close(FileMapHandle);
			return;
		}
		firstload = 1;
	}

	FileMap = mmap(NULL, FILEMAPSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, FileMapHandle, 0);
	if (FileMap == MAP_FAILED) {
		FileMap = NULL;
		shm_unlink("/ASM_MapFile");
		close(FileMapHandle);
		asmlog_error("Could not memory map the object: %s", strerror(errno));
		return;
	}

	if (firstload) {
		asmlog_debug("first load - clearing shared memory area.");
		memset(FileMap, 0, FILEMAPSIZE);
	} else {
		asmlog_debug("using existing shared memory area");
	}

	memset(&T0, 0, sizeof(T0));
	clock_gettime(CLOCK_MONOTONIC, &T0);

	read_settings(); // ASM.ini

	asmlog_debug("extension loaded");
}


void __attribute ((destructor)) libasm_close(void)
{
	if (ArmaServerInfo != NULL) {
		memset(ArmaServerInfo, 0, pagesize);
		msync(ArmaServerInfo, pagesize, MS_ASYNC|MS_INVALIDATE);
	}
	if (FileMap != NULL) {
		munmap(FileMap, FILEMAPSIZE);
	}
	if (FileMapHandle > -1) {
		shm_unlink("/ASM_MapFile");
		close(FileMapHandle);
	}
	asmlog_debug("extension unloaded");
	asmlog_close();
}


void RVExtension(char *output, int outputSize, const char *function)
{
	char *stopstring;
	size_t funlen;

	if (output == NULL || outputSize <= 0 || function == NULL) return;
	funlen = strnlen(function, FUNCTIONSIZE + 2);	// "<n>:<OCCx string>"
	if (funlen == 0) return;
	if (funlen == FUNCTIONSIZE + 2) return;

	*output = '\0';

	//asmlog_debug("RVExtension(%p, %d, \"%s\")", output, outputSize, function);
	if (!isdigit(*function)) {
		do {
			// Get version
			if (strncasecmp(function, "version", sizeof("version")) == 0) {
				snprintf(output, outputSize, "%s", ASM_VERSION); // return the RV extension version
				break;
			}
			// (Debug) Get instance id
			if (strncasecmp(function, "id", sizeof("id")) == 0) {
				snprintf(output, outputSize, "%u", InstanceID);
				break;
			}
		} while (0);
		output[outputSize - 1] = '\0';
		return;
	} else {
		// function is supposed to be <digit>:<data>
		if (function[1] != ':' || funlen < 3) {
			return;
		}
	}

	if (!FileMap) {
	   asmlog_error("no FileMap");
	   return;
	}

	switch (*function) {
		case '0': // FPS update
			if (ArmaServerInfo != NULL) {
					unsigned FPS,FPSMIN;

					FPS = strtol(&function[2], &stopstring, 10);
					FPSMIN = strtol(&stopstring[1], &stopstring, 10);
					ArmaServerInfo->SERVER_FPS    =	FPS;
					ArmaServerInfo->SERVER_FPSMIN =	FPSMIN;
					ArmaServerInfo->TICK_COUNT    = gettickcount();

					asmlog_debug("0: FPS update");
			}
			break;

		case '1': // CPS update
			if (ArmaServerInfo != NULL) {
					struct timespec T1;
					double tnsec;
					unsigned conditionNo;

					memset(&T1, 0, sizeof(T1));
					clock_gettime(CLOCK_MONOTONIC, &T1);
					tnsec = (1e9 * (T1.tv_sec - T0.tv_sec) + (T1.tv_nsec - T0.tv_nsec)) / 1e9;
					conditionNo = strtol(&function[2], &stopstring, 10);
					ArmaServerInfo->FSM_CE_FREQ = floor(conditionNo * 1000 / tnsec + 0.5);

					T0 = T1;
					asmlog_debug("1: CPS update");
			}
			break;

		case '2': // GEN update
			if (ArmaServerInfo != NULL) {
				unsigned players, ail, air;
				FILE* f = NULL;
				long rss = 0L;

				players = strtol(&function[2],   &stopstring, 10);
				ail		= strtol(&stopstring[1], &stopstring, 10);
				air		= strtol(&stopstring[1], &stopstring, 10);
				ArmaServerInfo->PLAYER_COUNT = players;
				ArmaServerInfo->AI_LOC_COUNT = ail;
				ArmaServerInfo->AI_REM_COUNT = air;

				// ASMdll.dll for Windows gets the "Commit Charge" value here,
				// the total memory that the memory manager has committed
				// for a running process. (unit: bytes)
				if ((f = fopen("/proc/self/statm", "r")) != NULL) {
					// The second number in statm is the size of the in-memory
					// working set (RSS). TODO: is this the value we want?
					if (fscanf(f, "%*s%8ld", &rss) != 1) {
						rss = 0L;
					}
					fclose(f);
				}
				ArmaServerInfo->MEM = rss * pagesize;
				asmlog_debug("2: GEN update");
			}
			break;

		case '3': // MISSION update
			if (ArmaServerInfo != NULL) {
				memset(ArmaServerInfo->MISSION, 0, SMALSTRINGSIZE);
				strncpy(ArmaServerInfo->MISSION, &function[2], SMALSTRINGSIZE);
				ArmaServerInfo->MISSION[SMALSTRINGSIZE-1] = 0;
				asmlog_debug("3: MISSION update");
			}
			break;

		case '4': // OBJ_COUNT_0 update
			if (ArmaServerInfo != NULL) {
				unsigned obj;
				obj = strtol(&function[2], &stopstring, 10);
				ArmaServerInfo->OBJ_COUNT_0 = obj;
				asmlog_debug("4: OBJ_COUNT_0 update");
			}
			break;

		case '5': // OBJ_COUNT_1 update
			if (ArmaServerInfo != NULL) {
				unsigned obj;
				obj = strtol(&function[2], &stopstring, 10);
				ArmaServerInfo->OBJ_COUNT_1 = obj;
				asmlog_debug("5: OBJ_COUNT_1 update");
			}
			break;

		case '6': // OBJ_COUNT_2 update
			if (ArmaServerInfo != NULL) {
				unsigned obj;
				obj = strtol(&function[2], &stopstring, 10);
				ArmaServerInfo->OBJ_COUNT_2 = obj;
				asmlog_debug("6: OBJ_COUNT_2 update");
			}
			break;

		case '9': // init
			if (ArmaServerInfo == NULL) {
				if (enableProfilePrefixSlotSelection > 0 && isdigit(function[2])) {
					asmlog_debug("selecting slot based on profileName...");
					// Select the instance based on the leading digit in the server's profile name
					errno = 0;
					InstanceID = strtol(&function[2], &stopstring, 10);
					if (errno == 0 && InstanceID < MAX_ARMA_INSTANCES) {
						ArmaServerInfo = (struct ARMA_SERVER_INFO*)((unsigned char *)FileMap + (InstanceID * pagesize));
					}
				} else {
					asmlog_debug("finding available slot");
					// Find a free server info slot or re-use one if it hasn't been updated in the last 10 seconds
					uint32_t DeadTimeout = gettickcount() - 10000;
					for (InstanceID = 0 ; InstanceID < MAX_ARMA_INSTANCES ; InstanceID++) {
						ArmaServerInfo = (struct ARMA_SERVER_INFO*)((unsigned char *)FileMap + (InstanceID * pagesize));
						if ((ArmaServerInfo->PID == 0) || (ArmaServerInfo->TICK_COUNT < DeadTimeout)) break;
					}
				}
				if (ArmaServerInfo != NULL && InstanceID < MAX_ARMA_INSTANCES) {
					ArmaServerInfo->MEM = 0;
					ArmaServerInfo->TICK_COUNT = gettickcount();
					ArmaServerInfo->PID = getpid();
					memset(ArmaServerInfo->PROFILE, 0, sizeof(ArmaServerInfo->PROFILE));
					strncpy(ArmaServerInfo->PROFILE, &function[2], sizeof(ArmaServerInfo->PROFILE));
					ArmaServerInfo->PROFILE[sizeof(ArmaServerInfo->PROFILE) - 1] = '\0';
					snprintf(output, outputSize, "_ASM_OPT=[%s,%s,%s,\"%s\",\"%s\",\"%s\"];", OCI0, OCI1, OCI2, OCC0, OCC1, OCC2);
				} else {
					ArmaServerInfo = NULL;
					asmlog_error("init failed - no available slots.");
					snprintf(output, outputSize, "_ASM_OPT=[0,0,0,\"\",\"\",\"\"];");
				}
				output[outputSize - 1] = '\0';
			} else {
				ArmaServerInfo->MEM = 0;
			}
			break;

		default: // 7,8 are not implemented
			return;
	}
	if (ArmaServerInfo != NULL) {
		if (msync(ArmaServerInfo, pagesize, MS_ASYNC|MS_INVALIDATE) != 0) {
			asmlog_error("msync(): %s", strerror(errno));
		}
	}
	return;
}
