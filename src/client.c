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
#include "config.h"
#include "util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUFSIZE (MAX_ARMA_INSTANCES * sizeof(struct ARMA_SERVER_INFO))

extern char* host;
extern int port;

extern int log_interval;
extern FILE* log_file;

int asmclient(int instance_set)
{
	int instance, count, server, rv, running = 1;
	char buf[BUFSIZE];
	char* bp = NULL;
	struct addrinfo hints;
	struct addrinfo *serverinfo = NULL, *p = NULL;
	char portnum[6] = {0, 0, 0, 0, 0, 0}, s[INET6_ADDRSTRLEN];
	char request[4] = {0, 0, 0, 0};
	struct ARMA_SERVER_INFO *asi = 0;

	asmlog_info(PACKAGE_STRING);

	memset(buf, 0, sizeof(buf));
	memset(&hints, 0, sizeof(hints));
	memset(s, 0, sizeof(s));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(portnum, 6, "%d", port);

	asmlog_info("Connecting to %s:%d", host, port);

	if ((rv = getaddrinfo(*host == '\0' ? "localhost" : host, portnum, &hints, &serverinfo)) !=0 ) {
		asmlog_error("asmclient: getaddrinfo, %s", gai_strerror(errno));
		return EXIT_FAILURE;
	}

	for (p = serverinfo; p != NULL; p = p->ai_next) {
		if ((server = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			asmlog_error("asmclient: socket, %s", strerror(errno));
			continue;
		}

		if (connect(server, p->ai_addr, p->ai_addrlen) == -1) {
			close(server);
			if (errno != ECONNREFUSED) {
				asmlog_error("asmclient: connect, %s (%d)", strerror(errno), errno);
			}
			continue;
		}
		break;
	}

	if (p == NULL) {
		asmlog_error("Could not connect to %s:%d", host, port);
		return EXIT_FAILURE;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));

	freeaddrinfo(serverinfo);

	while (running) {
		// Send four-byte zero reqest
		int remaining = (int)sizeof(request);
		do {
			rv = send(server, (void *)request, remaining, 0);
			if (rv == -1) {
				if (errno != EINTR) {
					asmlog_error("asmclient: send, %s", strerror(errno));
				}
				break;
			}
			remaining -= rv;
		} while (remaining > 0);


		// Receive ASI stats
		rv = recv(server, buf, BUFSIZE, 0);
		if (rv == 0) {
			// Server closed the connection
			running = 0;
			continue;
		}
		if (rv == -1) {
			if (errno != EINTR) {
				asmlog_error("asmclient: recv, %s", strerror(errno));
			}
			continue;
		}


		// Unless the -o option was used to pick which four servers should
		// be displayed, show all of them.
		count = instance_set == 0 ? MAX_ARMA_INSTANCES : 4;
		if (instance_set == 0) {
			bp = buf;
		} else {
			bp = buf + (4 * (instance_set - 1));
		}
		asmlog_info("Displaying stats for %d instances...", count);
		// FIXME: consider that the received data for inactive instances
		//        is only the PID field.
		for (instance = 0; instance < count; instance++) {
			asi = (struct ARMA_SERVER_INFO*)(bp + (instance * sizeof(struct ARMA_SERVER_INFO)));
			asmlog_info("============================ server %2d", instance + 1);
			asmlog_info("PID = %zd", asi->PID);
			asmlog_info("OC0 = %zd", asi->OBJ_COUNT_0);
			asmlog_info("OC1 = %zd", asi->OBJ_COUNT_1);
			asmlog_info("OC2 = %zd", asi->OBJ_COUNT_2);
			asmlog_info("PLC = %zd", asi->PLAYER_COUNT);
			asmlog_info("AIL = %zd", asi->AI_LOC_COUNT);
			asmlog_info("AIR = %zd", asi->AI_REM_COUNT);
			asmlog_info("FPS = %zd", asi->SERVER_FPS);
			asmlog_info("MIN = %zd", asi->SERVER_FPSMIN);
			asmlog_info("CPS = %zd", asi->FSM_CE_FREQ);
			asmlog_info("MEM = %zd", asi->MEM / (1024*1024));
			asmlog_info("NTI = %zd", asi->NET_RECV);
			asmlog_info("NTO = %zd", asi->NET_SEND);
			asmlog_info("DIR = %zd", asi->DISC_READ);
			asmlog_info("TICK = %zd", asi->TICK_COUNT);
			asmlog_info("MISSION = \"%s\"", asi->MISSION);
			asmlog_info("PROFILE = \"%s\"", asi->PROFILE);

			// instance|TimeStamp|FPS|CPS|PL#|AIL|AIR|OC0|OC1|OC2
			if (log_file) {
				fprintf(log_file, "%d|%zd|%u|%u|%u|%u|%u|%u|%u|%u\n",
					instance, time(NULL),
					asi->SERVER_FPS, asi->FSM_CE_FREQ,
					asi->PLAYER_COUNT, asi->AI_LOC_COUNT, asi->AI_REM_COUNT,
					asi->OBJ_COUNT_0, asi->OBJ_COUNT_1, asi->OBJ_COUNT_2);
			}
		}
		// Debug: run only one time
		running = 0;

		(void)sleep(log_interval);
	}

	return EXIT_SUCCESS;
}

