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

void asmlog_console();
void asmlog_stdout(const char* name);
void asmlog_systemd(const char* name);
void asmlog_syslog(const char* name);

void asmlog_enable_debug(void);
void asmlog_close(void);

void asmlog_critical(const char* format, ...);
void asmlog_error(const char* format, ...);
void asmlog_warning(const char* format, ...);
void asmlog_notice(const char* format, ...);
void asmlog_info(const char* format, ...);
void asmlog_debug(const char* format, ...);

#endif /* ASMLOG_H_ */
