/*
 * Copyright 2015 Killswitch
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
#ifndef ASMLOG_H_
#define ASMLOG_H_

extern int asmlog_level;

void asmlog_stdout(const char* name);
void asmlog_syslog(const char* name);

void asmlog_enable_debug();

#define ASMLOG(x) void asmlog_x (const char* format, ...)
ASMLOG(critical);
ASMLOG(error);
ASMLOG(warning);
ASMLOG(notice);
ASMLOG(info);
ASMLOG(debug);

#endif /* ASMLOG_H_ */
