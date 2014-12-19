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
	int i, instance, count, server, rv, running = 1;
	char buf[BUFSIZE];
	char* bp = NULL;
	struct addrinfo hints;
	struct addrinfo *serverinfo = NULL, *p = NULL;
	char portnum[6] = {0, 0, 0, 0, 0, 0}, s[INET6_ADDRSTRLEN];
	char request[4] = {0, 0, 0, 0};
	struct ARMA_SERVER_INFO *asi = 0;


	memset(buf, 0, sizeof(buf));
	memset(&hints, 0, sizeof(hints));
	memset(s, 0, sizeof(s));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(portnum, 6, "%d", port);

	printf("Connecting to %s:%s\n", host, portnum);

	if ((rv = getaddrinfo(*host == '\0' ? "localhost" : host, portnum, &hints, &serverinfo)) !=0 ) {
		perror("asmclient(): getaddrinfo");
		return EXIT_FAILURE;
	}

	for (p = serverinfo; p != NULL; p = p->ai_next) {
		if ((server = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("asmclient(): socket");
			continue;
		}

		if (connect(server, p->ai_addr, p->ai_addrlen) == -1) {
			close(server);
			if (errno != ECONNREFUSED) {
				fprintf(stderr, "asmclient: connect, %s (%d)\n", strerror(errno), errno);
			}
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "Could not connect to %s:%d\n", host, port);
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
					fprintf(stderr, "asmclient(): send(), %s\n", strerror(errno));
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
				fprintf(stderr, "asmclient():recv(), %s\n", strerror(errno));
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
		fprintf(stderr, "Displaying stats for %d instances...\n", count);
		// FIXME: consider that the received data for inactive instances
		//        is only the PID field.
		for (instance = 0; instance < count; instance++) {
			asi = (struct ARMA_SERVER_INFO*)(bp + (instance * sizeof(struct ARMA_SERVER_INFO)));
			printf("============================ server %2d\n", instance + 1);
			printf("PID = %zd\n", asi->PID);
			printf("OC0 = %zd\n", asi->OBJ_COUNT_0);
			printf("OC1 = %zd\n", asi->OBJ_COUNT_1);
			printf("OC2 = %zd\n", asi->OBJ_COUNT_2);
			printf("PLC = %zd\n", asi->PLAYER_COUNT);
			printf("AIL = %zd\n", asi->AI_LOC_COUNT);
			printf("AIR = %zd\n", asi->AI_REM_COUNT);
			printf("FPS = %zd\n", asi->SERVER_FPS);
			printf("MIN = %zd\n", asi->SERVER_FPSMIN);
			printf("CPS = %zd\n", asi->FSM_CE_FREQ);
			printf("MEM = %zd\n", asi->MEM / (1024*1024));
			printf("NTI = %zd\n", asi->NET_RECV);
			printf("NTO = %zd\n", asi->NET_SEND);
			printf("DIR = %zd\n", asi->DISC_READ);
			printf("TICK = %zd\n", asi->TICK_COUNT);
			printf("MISSION = \"%s\"\n", asi->MISSION);
			printf("PROFILE = \"%s\"\n", asi->PROFILE);

			// instance|TimeStamp|FPS|CPS|PL#|AIL|AIR|OC0|OC1|OC2
			if (log_file) {
				fprintf(log_file, "%d|%zd|%zd|%zd|%zd|%zd|%zd|%zd|%zd|%zd\n",
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

