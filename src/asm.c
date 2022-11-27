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
#include <errno.h>
#include <netinet/in.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "asm.h"
#include "asmlog.h"
#include "client.h"
#include "server.h"

#define LOG_PREFIX_DEFAULT "ASMlog"

char*  prog_name;
char** args;
int    argsc;

char*  log_prefix;
int    log_interval;

pid_t  pid;
char*  pid_name;
size_t pid_name_len;

int    server = 0;
int    client = 1;
int    max_clients  = 1;
int    sysv_daemon  = 0;
int    systemd      = 0;
int    instance_set = 0; // 0...3, which set of 4 instances should be reported by the client?

char*  host;
int    port = 24000;

volatile sig_atomic_t running = 0;
int    once = 1;  // Client: display stats once, not continuously. TODO: add option

void usage(const char* prog_name)
{
	fprintf(stderr, "\nUsage: %s [-s|-c] [-n <max #clients>] [-h host] [-p port] [-l logfile] [-t <log interval>]\n", prog_name);
}

// Handle a few termination signals
void handle_signal(int s)
{
	if (running == 0) return;

	switch (s)
	{
		case SIGHUP:
			/* TODO: re-read config.
			 * Could be used to re-initalize the service if the need arises.
			 */
			break;
		case SIGINT:
			running = 0;
			break;
		case SIGTERM:
			running = 0;
			break;
		default:
			break;
	}
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
 *  -d      Enable debug-level log messages
 *  -b      (server) Run in the background as a SysV style daemon.
 *  -y      (server) Run as a systemd service, logging to stdout
 *
 */
int main(int argc, char** argv)
{
	int option;
	int b_seen = 0;
	int c_seen = 0;
	int s_seen = 0;
	int y_seen = 0;
	int usage_error = 0;
	int status = EXIT_SUCCESS;
	struct sigaction sa;

	prog_name = strdup(basename(argv[0]));
	if (prog_name == NULL) {
		perror("malloc");
		status = EXIT_FAILURE;
		goto cleanup;
	}
	args  = argv;
	argsc = argc;

	pid_name_len = (size_t)sysconf(_PC_PATH_MAX);
	pid_name = (char *)calloc(pid_name_len, 1);
	if (pid_name == NULL) {
		perror("calloc");
		status = EXIT_FAILURE;
		goto cleanup;
	}

	host = (char *)calloc(INET6_ADDRSTRLEN, 1);
	if (host == NULL) {
		perror("calloc");
		status = EXIT_FAILURE;
		goto cleanup;
	}

	while (usage_error == 0 && (option = getopt(argc, argv, "bcdh:i:l::n:o:p:st:y")) != -1) {
		switch (option) {
			case 'b':
				sysv_daemon = 1;
				b_seen = 1;
				if (y_seen == 1) usage_error = 1;
				break;
			case 'c':
				server = 0;
				client = 1;
				c_seen = 1;
				if (s_seen == 1) usage_error = 1;
				break;
			case 'd':
				asmlog_enable_debug();
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
				log_prefix = strdup(optarg);
				if (log_interval == 0) log_interval = 1;
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
			case 'y':
				systemd = 1;
				if (b_seen == 1) usage_error = 1;
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

	if (log_prefix == NULL) {
		log_prefix = strdup(LOG_PREFIX_DEFAULT);
	}

	if (client) {
		if (*host == '\0') {
			strcpy(host, "localhost");
		}
	}
#ifdef SHOW_OPTIONS
	printf("client:      %d\n", client);
	printf("server:      %d\n", server);
	printf("max clients: %d\n", max_clients);
	printf("host:        %s\n", host);
	printf("port:        %d\n", port);
	printf("log prefix:  %s\n", log_prefix);
	printf("interval:    %d\n", log_interval);
#endif

	// Handle HUP, kill and CTRL-C
	pid = getpid();
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = handle_signal;
	sa.sa_flags = 0;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	if (server) {
		if (sysv_daemon) {
			asmlog_syslog(prog_name);
		} else if (systemd) {
			asmlog_systemd();
		} else {
			asmlog_console();
		}
		// Work as a service
		status = asmserver();
	} else {
		asmlog_console();

		// Client: connect to server and receive stats
		status = asmclient(instance_set);
	}

cleanup:
	free(host);
	free(pid_name);
	free(prog_name);
	free(log_prefix);

	return status;
}
