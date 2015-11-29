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
#include "config.h"
#include "asmlog.h"

#ifdef ENABLE_SYSTEMD_JOURNAL
#include <systemd/sd-journal.h>
#else
#include <syslog.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum logprefix {
	ASM_LOGPREFIX_NONE, // no prefix
	ASM_LOGPREFIX_NUM,  // "<6>", "<4>" etc
	ASM_LOGPREFIX_TXT   // "<INFO>", "<ERR>" etc
};
enum logdest {
	ASM_LOGDEST_CLOSED,
	ASM_LOGDEST_STDOUT,
	ASM_LOGDEST_SYSLOG
};
typedef struct _logprefixname
{
	const char *num;
	const char *str;
} logprefixname;

static int logprefix = ASM_LOGPREFIX_NONE;
static int logdest = ASM_LOGDEST_CLOSED;
static const char* lognone = "";
static char* logname = NULL;

logprefixname logprefixnames[] =
{
		{"<0>", "<EMERG>"},
		{"<1>", "<ALERT>"},
		{"<2>", "<CRIT>"},
		{"<3>", "<ERR>"},
		{"<4>", "<WARNING>"},
		{"<5>", "<NOTICE>"},
		{"<6>", "<INFO>"},
		{"<7>", "<DEBUG>"},
};

int asmlog_level = LOG_INFO;

static void asmlog_init(const char* name, int dest, int prefix)
{
	if (name) {
		logname = strdup(name);
	}
	logdest = dest;
	logprefix = prefix;
}

// Log directly to stdout/stderr
void asmlog_console()
{
	asmlog_init(NULL, ASM_LOGDEST_STDOUT, ASM_LOGPREFIX_NONE);
}

// Log to stdout with program name prefix
void asmlog_stdout(const char* name)
{
	asmlog_init(name, ASM_LOGDEST_STDOUT, ASM_LOGPREFIX_NONE);
}

// Log to stdout with a "<n>" prefix at the beginning of the log message
void asmlog_systemd(const char* name)
{
	asmlog_init(name, ASM_LOGDEST_STDOUT, ASM_LOGPREFIX_NUM);
}

// Send output to the system log
void asmlog_syslog(const char* name)
{
	asmlog_init(name, ASM_LOGDEST_SYSLOG, ASM_LOGPREFIX_NONE);
	openlog(logname, LOG_CONS|LOG_ODELAY|LOG_PID, LOG_USER);
}

// Allow LOG_DEBUG level messages to be shown
void asmlog_enable_debug(void)
{
	asmlog_level = LOG_DEBUG;
}

void asmlog_close(void)
{
	if (logdest == ASM_LOGDEST_SYSLOG)
	{
		closelog();
	}
	if (logname)
	{
		free(logname);
		logname = 0;
	}
	logdest = ASM_LOGDEST_CLOSED;
}

static const char* asmlog_prefix(int level)
{
	const char* prefix;

	switch (logprefix)
	{
		case ASM_LOGPREFIX_NONE:
			prefix = lognone;
			break;
		case ASM_LOGPREFIX_NUM:
			prefix = logprefixnames[level].num;
			break;
		case ASM_LOGPREFIX_TXT:
			prefix = logprefixnames[level].str;
	}

	return prefix;
}

static void asmlog(int level, const char* format, va_list ap)
{
	va_list apc;

	if (level < LOG_EMERG || level > LOG_DEBUG || format == NULL) return;

	va_copy(apc, ap);
	if (logdest == ASM_LOGDEST_STDOUT)
	{
		FILE *os;

		if (level > LOG_ERR) {
			os = stdout;
		} else {
			os = stderr;
		}
		if (level <= asmlog_level) {
			if (logprefix != ASM_LOGPREFIX_NONE) {
				fprintf(os, "%s ", asmlog_prefix(level));
			}
			if (logname) {
				fprintf(os, "%s: ", logname);
			}
			vfprintf(os, format, apc);
			fprintf(os,"\n");
			fflush(os);
		}
	}
	if (logdest == ASM_LOGDEST_SYSLOG)
	{
		vsyslog(level, format, apc);

	}
	va_end(apc);
}

void asmlog_critical(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	asmlog(LOG_CRIT, format, ap);
	va_end(ap);
}
void asmlog_error(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	asmlog(LOG_ERR, format, ap);
	va_end(ap);
}
void asmlog_warning(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	asmlog(LOG_WARNING, format, ap);
	va_end(ap);
}
void asmlog_notice(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	asmlog(LOG_NOTICE, format, ap);
	va_end(ap);
}
void asmlog_info(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	asmlog(LOG_INFO, format, ap);
	va_end(ap);
}
void asmlog_debug(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	asmlog(LOG_DEBUG, format, ap);
	va_end(ap);
}
