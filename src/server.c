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
#include "server.h"
#include "settings.h"
#include "util.h"
#include "gettickcount.h"

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

extern char*  prog_name;
extern char** args;
extern int    argsc;

extern char*  pid_name;
extern int    pid_name_len;

extern int    port;
extern int    max_clients;
extern int    sysv_daemon;
extern int    running;

static int    pid_name_created = 0;
static int    connected_clients = 0;

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
	int firstload = 0;

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
		// First load of the extension - resize the shared memory for our needs
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
		shm_unlink("/ASM_MapFile");
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
		shm_unlink("/ASM_MapFile");
		close(filemap_fd);
	}
}

//
// Return:
//  1 If <pid> is running and is named <progname>
//  0 if <pid> is not running or if <pid> is running,
//    but is not named <progname>
// -1 if an error occured
//
int already_running(pid_t pid, char *progname)
{
	int    rv = 0;
	FILE*  f = NULL;
	char   procpath[20];
	char*  cmdline = NULL;
	size_t cmdlen = 0;

	memset(procpath, 0, sizeof(procpath));
	snprintf(procpath, sizeof(procpath), "/proc/%d/cmdline", pid);

	f = fopen(procpath, "r");
	if (f == NULL) {
		if (errno != ENOENT) {
			asmlog_error("Could not open \"%s\": %s", procpath, strerror(errno));
			rv = -1;
		}
		memset(procpath, 0, sizeof(procpath));
	} else {
		if (getline(&cmdline, &cmdlen, f)) {
			if (!strncmp(progname, basename(cmdline), strlen(progname))) {
				rv = 1;
			}
			free(cmdline);
		}
		fclose(f);
	}

	return rv;
}


// Create a PID file when the program runs as a daemon
int PID_start()
{
	int fd;
	FILE* pf;
	uid_t euid;

	euid = geteuid();
	// pick reasonable default PID file locations
	if (*pid_name == '\0') {
		if (euid == 0) {
			snprintf(pid_name, pid_name_len, "/var/run/%s.pid", prog_name);
		} else {
			// Breaks for non-root user whose home dir is / (eg a non-login system user)
			//snprintf(pid_name, pid_name_len, "./%s.pid", prog_name);
			return 0;
		}
	}
	asmlog_debug("PID_start: prog_name = %s, pid_name = \"%s\"", prog_name, pid_name);


	fd = open(pid_name, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (fd == -1) {
		asmlog_error("Cannot open PID file: \"%s\"", pid_name);
		return 1;
	}

	// Block und lock
	if (flock(fd, LOCK_EX) == -1) {
		asmlog_error("Cannot lock PID file: \"%s\"", pid_name);
		close(fd);
		return 1;
	}

	pf = fdopen(fd, "r+");
	if (pf == NULL) {
		asmlog_error("Cannot open PID file: \"%s\"", pid_name);
		flock(fd, LOCK_UN);
		close(fd);
		return 1;
	} else {
		pid_t pid, oldpid;

		pid = getpid();
		if (fscanf(pf, "%5d", &oldpid) == 1) {
			/*  TODO: ok, so who is this PID then?
			 *  If a process with that PID does not exist, carry on.
			 *  If it's a running process that is not this daemon, carry on.
			 *  If it's another instance of this daemon, GTFO.
			 */
			if (already_running(pid, prog_name) == 1) {
				asmlog_error("An instance of %s is already running", prog_name);
				flock(fd, LOCK_UN);
				fclose(pf);
				return 1;
			}
		}
		rewind(pf);
		fprintf(pf, "%lu", (unsigned long int)pid);
		flock(fd, LOCK_UN);
		fclose(pf);
		pid_name_created = 1;
		asmlog_info("PID %d written to %s", pid, pid_name);
	}
	return 0;
}

// Remove the PID file when the daemon stops
void PID_stop()
{
	if (pid_name_created && pid_name) {
		if (unlink(pid_name)) {
			asmlog_error("Could not remove PID file \"%s\"", pid_name);
		} else {
			asmlog_info("%s was removed", pid_name);
		}
		memset(pid_name, 0, pid_name_len);
		pid_name_created = 0;
	}
}


/* SysV style daemonizing
 *
 *  1 Close all open file descriptors except stdin, stdout, stderr (i.e. the
 *    first three file descriptors 0, 1, 2). This ensures that no accidentally
 *    passed file descriptor stays around in the daemon process. On Linux,
 *    this is best implemented by iterating through /proc/self/fd, with a
 *    fallback of iterating from file descriptor 3 to the value returned by
 *    getrlimit() for RLIMIT_NOFILE.
 *
 *  2 Reset all signal handlers to their default. This is best done by
 *    iterating through the available signals up to the limit of _NSIG and
 *    resetting them to SIG_DFL.
 *
 *  3 Reset the signal mask using sigprocmask().
 *
 *  4 Sanitize the environment block, removing or resetting environment
 *    variables that might negatively impact daemon runtime.
 *
 *  5 Call fork(), to create a background process.
 *
 *  6 In the child, call setsid() to detach from any terminal and create
 *    an independent session.
 *
 *  7 In the child, call fork() again, to ensure that the daemon can never
 *    re-acquire a terminal again.
 *
 *  8 Call exit() in the first child, so that only the second child (the actual
 *    daemon process) stays around. This ensures that the daemon process is
 *    re-parented to init/PID 1, as all daemons should be.
 *
 *  9 In the daemon process, connect /dev/null to standard input, output,
 *    and error.
 *
 * 10 In the daemon process, reset the umask to 0, so that the file modes
 *    passed to open(), mkdir() and suchlike directly control the access
 *    mode of the created files and directories.
 *
 * 11 In the daemon process, change the current directory to the root directory
 *    (/), in order to avoid that the daemon involuntarily blocks mount points
 *    from being unmounted.
 *
 * 12 In the daemon process, write the daemon PID (as returned by getpid()) to a
 *    PID file, for example /run/foobar.pid (for a hypothetical daemon "foobar")
 *    to ensure that the daemon cannot be started more than once. This must be
 *    implemented in race-free fashion so that the PID file is only updated when
 *    it is verified at the same time that the PID previously stored in the PID
 *    file no longer exists or belongs to a foreign process. Commonly, some kind
 *    of file locking is employed to implement this logic.
 *
 * 13 In the daemon process, drop privileges, if possible and applicable.
 *
 * 14 From the daemon process, notify the original process started that
 *    initialization is complete. This can be implemented via an unnamed pipe or
 *    similar communication channel that is created before the first fork() and
 *    hence available in both the original and the daemon process.
 *
 * 15 Call exit() in the original process. The process that invoked the daemon must
 *    be able to rely on that this exit() happens after initialization is complete
 *    and all external communication channels are established and accessible.
 */
void daemonize()
{
	int i, fd, status;
	int pf1[2]; // Connects parent with child 1

	struct dirent** dirlist;
	sigset_t sigset;
	pid_t pid, daemon_pid = 0;

	// 1. Close all open file descriptors except stdin, stdout, stderr
	i = scandir("/proc/self/fd", &dirlist, 0, alphasort);
	if (i < 0) {
		asmlog_error("daemonize: scandir");
		_exit(errno);
	}
	while (i--) {
		if (isdigit(dirlist[i]->d_name[0])) {
			fd = (int)strtod(dirlist[i]->d_name, 0);
			if (fd > STDERR_FILENO) close(fd);
		}
		free(dirlist[i]);
	}

	// 2 Reset all signal handlers to their default
	for (i = 1; i < NSIG; i++) {
		signal(i, SIG_DFL);
	}

	// 3 reset the signal mask
	sigprocmask(SIG_BLOCK, NULL, &sigset);		// get
	sigprocmask(SIG_UNBLOCK, &sigset, NULL);	// reset

	// 4 TODO: santitize the environment block

	// 5 Call fork()
	if (pipe(pf1) != 0) {
		asmlog_error("daemonize: 5, pipe");
		exit(errno);
	}

	if ((pid = fork()) < 0) {
		close(pf1[0]);
		close(pf1[1]);
		asmlog_error("daemonize: 5, fork");
		exit(errno);
	};

	if (pid == 0) {
		int fd0, fd1, fd2;
		int pf2[2]; // Connects child 1 with child 2 (the actual daemon)


		close(pf1[0]); // Close the reader end of the pipe

		// 6 Child process Mk.I - call setsid()
		if (setsid() < 0) {
			close(pf1[1]);
			asmlog_error("daemonize: 6, setsid");
			_exit(errno);
		}

		if (pipe(pf2) != 0) { // Connects child 1 with child 2 (the actual daemon)
			asmlog_error("daemonize: 6, pipe");
			_exit(errno);
		}

		// 7 Fork again
		if ((pid = fork()) < 0) {
			close(pf1[1]);
			close(pf2[0]);
			close(pf2[1]);
			asmlog_error("daemonize: 7, fork");
			_exit(errno);
		}

		// 8 Call exit() in the first child
		if (pid > 0) {
			status = 0;
			close(pf2[1]);
			if (read(pf2[0], &daemon_pid, sizeof(pid_t)) < 0) {
				asmlog_error("daemonize: 8, read");
				status = errno;
			} else {
				if (write(pf1[1], &daemon_pid, sizeof(pid_t)) < 0) {
					asmlog_error("daemonize: 8, write");
					status = errno;
				}
			}
			close(pf2[0]);
			close(pf1[1]);
			_exit(status);
		}

		close(pf1[1]);
		close(pf2[0]);

		// 9 Close the stdin, stdout and stderr file descriptors and then
		//   connect them to /dev/null
		close(STDERR_FILENO);
		close(STDOUT_FILENO);
		close(STDIN_FILENO);
		fd0 = open("/dev/null", O_RDWR);
		fd1 = dup(0);
		fd2 = dup(0);
		//asmlog_info("daemonize: new fd0 = %d", fd0);

		// 10 Clear the file creation mask
		umask(0);

		// 11 If we are running as a system service, change
		//    the current dir to the root directory
		if (geteuid() == 0 && chdir("/") < 0) {
			asmlog_error("daemonize: chdir");
			close(fd2);
			close(fd1);
			close(fd0);
			_exit(errno);
		}

		// 12 Write daemon PID to a PID file
		PID_start();

		// 13 Drop privileges, if needed
		if (geteuid() == 0) {
			// TODO: drop down a notch. Do NOT run as root
		}

		// 14 Notify the original process that we're done
		pid = getpid();
		if (write(pf2[1], &pid, sizeof(pid_t)) < 0) {
			asmlog_error("daemonize: 14, write");
			_exit(errno);
		}
		//asmlog_info("daemonize, child 2 init DONE: %d bytes written", i);
		close(pf2[1]);
	} else {
		// Parent. 15 Wait for the daemon child to finish initialization and then exit.
		close(pf1[1]);
		i = read(pf1[0], &daemon_pid, sizeof(pid_t));
		close(pf1[0]);
		wait(&status);
		asmlog_debug("daemonize: parent DONE, %d bytes read, daemon is %zd, first child returned %d", i, daemon_pid, status);
		_exit(status);
	}

	return;
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

	if (sysv_daemon) {
		asmlog_debug("asmserver(): daemonize begin");
		daemonize();
		asmlog_debug("asmserver(): daemonize done");
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

	PID_stop();
	close_shmem();
	asmlog_close();

	return EXIT_SUCCESS;
}

