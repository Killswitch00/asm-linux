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
#include <syslog.h>

extern int loglevel;


void *get_in_addr(struct sockaddr *);

void asmlog_open(const char *ident, int option, int facility);
void asmlog(int level, const char *format, ...);
void asmlog_close(void);
