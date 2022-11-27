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

// For glibc 2.10 and earlier, this is needed for getline()
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netdb.h>

#include "asm.h"
#include "asmlog.h"
#include "config.h"
#include "server.h"
#include "settings.h"
#include "util.h"
#include "gettickcount.h"

extern char*  prog_name;
extern char** args;
extern int    argsc;

extern int    port;
extern int    max_clients;
extern int    running;

static int    connected_clients = 0;

static int    firstload = 0;
static int    filemap_fd;
static void*  filemap;
static long   pagesize;

/*
 * Initialize the shared memory area where the stats will be reported
 */
int init_shmem()
{
	mode_t orig_umask;
	struct stat filestat;

	pagesize = sysconf(_SC_PAGESIZE); // 4 KiB or 2 MiB

	orig_umask = umask(0);
	filemap_fd = shm_open("/ASM_MapFile", O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	(void)umask(orig_umask);

	if (filemap_fd < 0) {
		asmlog_error("Could not create shared memory object, %s", strerror(errno));
		return 1;
	}
	memset(&filestat, 0, sizeof(filestat));
	if (fstat(filemap_fd, &filestat) != 0) {
		asmlog_error("Could not fstat() the shared memory object, %s", strerror(errno));
		(void)shm_unlink("/ASM_MapFile");
		(void)close(filemap_fd);
		return 1;
	}
	if (filestat.st_size == 0) {
		if (ftruncate(filemap_fd, FILEMAPSIZE) != 0) {
			asmlog_error("Could not set shared memory object size, %s", strerror(errno));
			shm_unlink("/ASM_MapFile");
			close(filemap_fd);
			return 1;
		}
		firstload = 1;
	}

	filemap = mmap(NULL, FILEMAPSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, filemap_fd, 0);
	if (filemap == MAP_FAILED) {
		filemap = NULL;
		if (firstload == 1) {
			shm_unlink("/ASM_MapFile");
		}
		close(filemap_fd);
		asmlog_error("Could not memory map the object, %s", strerror(errno));
		return 1;
	}

	if (firstload == 1) {
		memset(filemap, 0, FILEMAPSIZE);
		asmlog_info("Shared memory initialized");
	}

	return 0;
}

void close_shmem(void)
{
	if (filemap != NULL) {
		munmap(filemap, FILEMAPSIZE);
	}
	if (filemap_fd > -1) {
		if (firstload == 1) {
			shm_unlink("/ASM_MapFile");
		}
		close(filemap_fd);
	}
}

// Handle child processes exiting
void handle_child(int s)
{
	(void)s;

    while (waitpid(-1, NULL, WNOHANG) > 0) {
		connected_clients--;
	}
}


/*
 * serialize the server info and send it down the tubes
 *
 * NOTE: it appears that the Windows ArmaServerMonitor uses data serialized
 *       in non-network byte order, ie in Intel x86 host byte order
 *       (aka Little endian). That means that this function is technically
 *       non-portable to non-x86 architectures, since we do not use htons()
 *       et al to serialize the data before it is sent.
 *       (OTOH, ArmA is strictly x86 anyhow)
 */
int send_asi(int clientfd)
{
	int status = 0;
	uint32_t DeadTimeOut;
	struct ARMA_SERVER_INFO *asi = NULL;
	int instance, remaining;
	unsigned char sendbuf[MAX_ARMA_INSTANCES * sizeof(struct ARMA_SERVER_INFO)];
	unsigned char* p = NULL;

	//asmlog_info("send_asi: ARMA_SERVER_INFO is %zd bytes.", sizeof(struct ARMA_SERVER_INFO));
	memset(sendbuf, 0, sizeof(sendbuf));
	p = sendbuf;
	DeadTimeOut = gettickcount() - 10000;
	for (instance = 0; instance < MAX_ARMA_INSTANCES; instance++) {
		asi = (struct ARMA_SERVER_INFO*)((unsigned char *)filemap + (instance * pagesize));

		// Serialize the ARMA_SERVER_INFO data
		if (asi->PID == 0 || asi->TICK_COUNT < DeadTimeOut) {
			// The slot is either unused or dead - just send a zero PID field.
			*((unsigned short *)p) = 0;                  p += sizeof(unsigned short); // 2
		} else {
			*((unsigned short *)p) = asi->PID;           p += sizeof(unsigned short); // 2
			*((unsigned short *)p) = asi->OBJ_COUNT_0;   p += sizeof(unsigned short); // 4
			*((unsigned short *)p) = asi->OBJ_COUNT_1;   p += sizeof(unsigned short); // 6
			*((unsigned short *)p) = asi->OBJ_COUNT_2;   p += sizeof(unsigned short); // 8
			*((unsigned short *)p) = asi->PLAYER_COUNT;  p += sizeof(unsigned short); // 10
			*((unsigned short *)p) = asi->AI_LOC_COUNT;  p += sizeof(unsigned short); // 12
			*((unsigned short *)p) = asi->AI_REM_COUNT;  p += sizeof(unsigned short); // 14
			*((unsigned short *)p) = asi->SERVER_FPS;    p += sizeof(unsigned short); // 16
			*((unsigned short *)p) = asi->SERVER_FPSMIN; p += sizeof(unsigned short); // 18
			*((unsigned short *)p) = asi->FSM_CE_FREQ;   p += sizeof(unsigned short); // 20
			*((unsigned int *)p)   = asi->MEM;           p += sizeof(unsigned int);   // 24
			*((unsigned int *)p)   = asi->NET_RECV;      p += sizeof(unsigned int);   // 28
			*((unsigned int *)p)   = asi->NET_SEND;      p += sizeof(unsigned int);   // 32
			*((unsigned int *)p)   = asi->DISC_READ;     p += sizeof(unsigned int);   // 36
			*((unsigned int *)p)   = asi->TICK_COUNT;    p += sizeof(unsigned int);   // 40
			memcpy(p, asi->MISSION, SMALSTRINGSIZE);     p += SMALSTRINGSIZE;         // 72
			memcpy(p, asi->PROFILE, SMALSTRINGSIZE);     p += SMALSTRINGSIZE;         // 104
			asi->MISSION[SMALSTRINGSIZE - 1] = 0;
			asi->PROFILE[SMALSTRINGSIZE - 1] = 0;
		}
	}
	remaining = p - sendbuf;
	//asmlog_debug("send_asi: %d sending %zd", instance + 1, remaining);
	do {
		int sent = send(clientfd, (void *)sendbuf, remaining, 0);
		if (sent == -1) {
			if (errno != EINTR) {
				asmlog_error("send_asi, send()");
				status = 1;
			}
			break;
		}
		remaining -= sent;
	} while (remaining > 0);

	return status;
}

// A port number is a 16-bit value whose max value is 65535,
// ie  5+1 chars if represented as a string
#define PORT_STRLEN 6
int asmserver()
{
	int rv, server, yes = 1;
	char portnum[PORT_STRLEN];
	struct addrinfo hints;
	struct addrinfo *address_list;
	struct addrinfo *p;
	struct sigaction sa;

	socklen_t size;
	struct sockaddr_storage client_addr;
	char s[INET6_ADDRSTRLEN];
	pid_t pid;

	asmlog_info(PACKAGE_STRING);

	// Open the shared memory area
	if (init_shmem()) {
		asmlog_error("Could not initalize the shared memory area");
		return EXIT_FAILURE;
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family   = AF_UNSPEC;   // IPv4 and IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags    = AI_PASSIVE;  // fill in my IP for me

	snprintf(portnum, PORT_STRLEN, "%d", port);

	if ((rv = getaddrinfo(NULL, portnum, &hints, &address_list)) != 0) {
	    asmlog_error("asmserver(): getaddrinfo, %s", gai_strerror(rv));
	    return EXIT_FAILURE;	// FIXME: cleanup
	}

	// Pick the first usable address found
	for (p = address_list; p != NULL; p = p->ai_next)
	{
		server = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (server == -1) {
			asmlog_error("asmserver(): socket, %s", strerror(errno));
			continue;
		}

		if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			close(server);
			asmlog_error("asmserver(): setsockopt");
			return EXIT_FAILURE;		// FIXME: cleanup
		}

		if (bind(server, p->ai_addr, p->ai_addrlen) == -1) {
			close(server);
			asmlog_error("asmserver(): bind");
			continue;
		}

		break;
	}

	if (p == NULL)
	{
		asmlog_error("server(): failed to bind");
	    return EXIT_FAILURE; // FIXME: cleanup
	}

	freeaddrinfo(address_list);

	if (listen(server, max_clients) == -1) {
		asmlog_error("listen");
		close(server);
	    return EXIT_FAILURE;
	}

	// Handle client connection handlers exiting
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = handle_child;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		asmlog_error("sigaction");
		close(server);
	    return EXIT_FAILURE;
	}

	// Wait for connections
	running = 1;
	size = sizeof(client_addr);

	while (running) {  // main accept() loop
		asmlog_info("Waiting for connections");

		int client = accept(server, (struct sockaddr *)&client_addr, &size);
		if (client == -1) {
			if (errno != EINTR) {
				asmlog_error("accept");
			}
			continue;
		}

		// Limit the number of clients that may connect to max_clients
		if (connected_clients >= max_clients) {
			close(client);
			continue;
		}

		inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);

		// TODO: use TCP Wrappers to let sysadmins accept or deny connections

		asmlog_info("Got connection from %s", s);

		if ((pid = fork()) < 0) {
			/* MURPHY */
			asmlog_error("fork, %s", strerror(errno));
			close(client);
			running = 0;
			continue;
		}

		connected_clients++;
		if (pid == 0) {
			/* CHILD */
			int i;
			char req[5] = {0};

			fd_set fds;

			close(server); // child doesn't need the server socket

			FD_ZERO(&fds);
			FD_SET(client, &fds);

			// Identify the child process using the client connection number
			snprintf(args[0], strlen(args[0]), "%s%d", prog_name, connected_clients);
			for (i = argsc - 1; i > 0; i--) {
				memset(args[i], 0, strlen(args[i]));
			}

			asmlog_info("Client %d connected", connected_clients);

			while (running) {
				asmlog_debug("Client %d recv() ...", connected_clients);
				rv = recv(client, req, 4, 0);

				if (rv == 0) {
					// The client closed the connection
					running = 0;
					continue;
				}

				if (rv == -1 && errno == EINTR) continue;

				// A zero "DWORD" (32-bits) is the magic word
				i = atoi(req);
				if (i == 0) {
					asmlog_debug("Client %d send_asi() ...", connected_clients);
					send_asi(client);
				} else {
					asmlog_error("Client %d received %08x (%02x %02x %02x %02x)",
							connected_clients, i, req[0], req[1], req[2], req[3]);
				}
			 	memset(req, 0xff, sizeof(req));
			 }

			close(client);
			asmlog_info("Client %d disconnected", connected_clients);
			asmlog_close();
			_exit(0);
		} else {
			/* PARENT */
			close(client);  // parent doesn't need the client socket
		}
	}

	asmlog_info("Server exiting");

	if (server > -1) {
		close(server);
	}

	close_shmem();
	asmlog_close();

	return EXIT_SUCCESS;
}

