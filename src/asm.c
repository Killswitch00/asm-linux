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
 * beuseful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Arma Server Monitor for Linux; if not, see
 * <http://www.gnu.org/licenses/>.
 */
#include "asm.h"
#include "client.h"
#include "server.h"

#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

char*  prog_name;
char** args;
int    argsc;

char*  log_name;
size_t log_name_len;
int    log_interval;
FILE*  log_file;

char*  pid_name;
size_t pid_name_len;

int    server = 0;
int    client = 1;
int    max_clients  = 1;
int    sysv_daemon  = 0;
int    instance_set = 0; // 0...3, which set of 4 instances should be reported by the client?

char*  host;
int    port = 24000;

void usage(const char* prog_name)
{
	fprintf(stderr, "\nUsage: %s [-s|-c] [-n <max #clients>] [-h host] [-p port] [-l logfile] [-t <log interval>]\n", prog_name);
}

/*
 *  -s      run ASM as server
 *  -c      run ASM as client (default)
 *  -n      allow n clients to connect (default: 1 client)
 *  -h      host address to bind to (server) or connect to (client) (default: localhost )
 *  -p      port to listen or to connect to (default: 24000)
 *  -l      prefix for and activation of client-side logfile (default: ./asm.log)
 *  -t      interval for logging, in seconds (default: 1)
 *  -i      PID file
 *  -o      When running as a client, Which set of four instances shall be reported? range 0..3, (default: 0)
 *
 *  -d      Do a SysV style daemonization when running as a server. (Default: don't - run as a systemd service)
 */
int main(int argc, char** argv)
{
	int option, flag, usage_error, c_seen, s_seen, status = EXIT_SUCCESS;

	c_seen = 0;
	s_seen = 0;
	usage_error = 0;

	if ((prog_name = strdup(basename(argv[0]))) == NULL) {
		perror("malloc");
		status = EXIT_FAILURE;
		goto cleanup;
	}
	args  = argv;
	argsc = argc;

	log_name_len = pid_name_len = (size_t)sysconf(_PC_PATH_MAX);
	if ((log_name = (char *)calloc(log_name_len, 1)) == NULL) {
		perror("calloc");
		status = EXIT_FAILURE;
		goto cleanup;
	}
	if ((pid_name = (char *)calloc(pid_name_len, 1)) == NULL) {
		perror("calloc");
		status = EXIT_FAILURE;
		goto cleanup;
	}
	if ((host = (char *)calloc(INET6_ADDRSTRLEN, 1)) == NULL) {
		perror("calloc");
		status = EXIT_FAILURE;
		goto cleanup;
	}

	while (usage_error == 0 && (option = getopt(argc, argv, "ch:i:l::n:o:p:st:")) != -1) {
		switch (option) {
			case 'c':
				server = 0;
				client = 1;
				c_seen = 1;
				if (s_seen == 1) usage_error = 1;
				break;
			case 'd':
				sysv_daemon = 1;
				break;
			case 'h':
				strncpy(host, optarg, INET6_ADDRSTRLEN);
				host[INET6_ADDRSTRLEN - 1] = '\0';
				break;
			case 'i':
				strncpy(pid_name, optarg, pid_name_len);
				pid_name[pid_name_len - 1] = '\0';
				break;
			case 'l':
				if (log_interval == 0) log_interval = 1;
				if (optarg != NULL) {
					strncpy(log_name, optarg, log_name_len);
				}
				log_name[log_name_len - 1] = '\0';
				break;
			case 'n':
				if (isdigit(*optarg)) {
					max_clients = atoi(optarg);
					if (max_clients <= 1) {
						max_clients = 1;
					}
				} else {
					usage_error = 1;
				}
				break;
			case 'o':
				if (isdigit(*optarg)) {
					instance_set = 1 + atoi(optarg);
					if (instance_set < 1 || instance_set > 4) {
						usage_error = 1;
					}
				} else {
					usage_error = 1;
				}
				break;
			case 'p':
				if (isdigit(*optarg)) {
					port = atoi(optarg);
					if (port <= 1) {
						port = 24000;
					}
				} else {
					usage_error = 1;
				}
				break;
			case 's':
				client = 0;
				server = 1;
				s_seen = 1;
				if (c_seen) usage_error = 1;
				break;
			case 't':
				if (isdigit(*optarg)) {
					log_interval = atoi(optarg);
					if (log_interval <= 1)
					{
						log_interval = 1;
					}
				} else {
					usage_error = 1;
				}
				break;
			default:
				usage_error = 1;
		}
	}

	if (usage_error == 1) {
			usage(prog_name);
			status = EXIT_FAILURE;
			goto cleanup;
	}

	if (client) {
		if (*host == '\0') {
			strcpy(host, "localhost");
		}

		if (*log_name == '\0') {
			strcpy(log_name, "./asm.log");
		}
	}

	printf("client:      %d\n", client);
	printf("server:      %d\n", server);
	printf("max clients: %d\n", max_clients);
	printf("host:        %s\n", host);
	printf("port:        %d\n", port);
	printf("log file:    %s\n", log_name);
	printf("interval:    %d\n", log_interval);

	if (server) {
		// Work as a service
		status = asmserver();
	} else {
		if (log_interval > 0) {
			log_file = fopen(log_name, "a+");
			if (log_file == NULL) {
				perror("Could not open log file");
				status = EXIT_FAILURE;
				goto cleanup;
			}
		}

		// Client: connect to server and receive stats
		status = asmclient(instance_set);
	}

cleanup:
	if (log_file)  fclose(log_file);
	if (host)      free(host);
	if (pid_name)  free(pid_name);
	if (log_name)  free(log_name);
	if (prog_name) free(prog_name);

	return status;
}
