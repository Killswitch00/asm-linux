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
#ifndef ASM_H_
#define ASM_H_

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"

#define ASM_VERSION VERSION

#define SMALSTRINGSIZE 32
#define FUNCTIONSIZE 2048
#define OUTPUTSIZE 4096
#define PAGESIZE sysconf(_SC_PAGESIZE)
#define MAX_ARMA_INSTANCES 16
#define FILEMAPSIZE (size_t)(PAGESIZE * MAX_ARMA_INSTANCES)


struct ARMA_SERVER_INFO
{
	uint16_t	PID;
	uint16_t	OBJ_COUNT_0;
	uint16_t	OBJ_COUNT_1;
	uint16_t	OBJ_COUNT_2;
	uint16_t	PLAYER_COUNT;
	uint16_t	AI_LOC_COUNT;
	uint16_t	AI_REM_COUNT;
	uint16_t	SERVER_FPS;
	uint16_t	SERVER_FPSMIN;
	uint16_t	FSM_CE_FREQ;
	uint32_t	MEM;
	uint32_t	NET_RECV;
	uint32_t	NET_SEND;
	uint32_t	DISC_READ;
	uint32_t	TICK_COUNT;
	char		MISSION[SMALSTRINGSIZE];
	char		PROFILE[SMALSTRINGSIZE];
};

#endif /* ASM_H_ */
