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
#include <arpa/inet.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

int loglevel = LOG_INFO;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void asmlog_open(const char *ident, int option, int facility)
{
	openlog(ident, option, facility);
}

void asmlog(int level, const char* format, ...)
{
	va_list ap;
	FILE* os;

	if (level <= LOG_ERR) {
		os = stderr;
	} else {
		os = stdout;
	}

	va_start(ap, format);
	vsyslog(level, format, ap);
	if (level <= loglevel) {
		fprintf(os,"asmdll: ");
		vfprintf(os, format, ap);
		fprintf(os,"\n");
		fflush(os);
	}
	va_end(ap);
}

void asmlog_close(void)
{
	closelog();
}
