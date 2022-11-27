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
#include <glib.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm.h"
#include "asmlog.h"
#include "settings.h"
#include "util.h"

// Settings and their default values
asm_settings settings = {
	.enableAPImonitoring = 0,  // Not implemented
	.enableProfilePrefixSlotSelection = 1,
	.OCI0 = "30",
	.OCI1 = "60",
	.OCI2 = "0",
	.OCC0 = "count entities \"\"All\"\";",
	.OCC1 = "count vehicles;",
	.OCC2 = "count allMissionObjects \"\"All\"\";"
};

/*
 * Read settings from the ASM.ini file.
 *
 * TODO: if any parsing errors occur, inform the user of this
 *       (and fall back to the defaults?)
 */
int read_settings(void)
{
	int   status   = 0;
	char* home     = NULL;
	char  inipath[PATH_MAX];

	GKeyFile* asm_ini    = NULL;
	GError*   gerror     = NULL;
	gboolean  ini_loaded = FALSE;


	memset(inipath, 0, PATH_MAX);

	home = getenv("HOME");
	if (home && strcmp(home, "/") == 0) {
		home = NULL;
	}

	asm_ini = g_key_file_new();
	if (asm_ini == NULL)
	{
		// TODO: log the error
		status = -1;
	}
	else
	{
		do
		{
			// First, try loading ASM.ini from the current working directory
			snprintf(inipath, PATH_MAX - 1, "./asm.ini");
			ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }

			snprintf(inipath, PATH_MAX - 1, "./ASM.ini");
			ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }


			// Then try the per-user ASM settings file
			if (home) {
				snprintf(inipath, PATH_MAX - 1, "%s/etc/asm.ini", home);
				ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &gerror);
				if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
				if (ini_loaded) { break; }

				snprintf(inipath, PATH_MAX - 1, "%s/etc/ASM.ini", home);
				ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &gerror);
				if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
				if (ini_loaded) { break; }

				snprintf(inipath, PATH_MAX - 1, "%s/.asm.ini", home);
				ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &gerror);
				if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
				if (ini_loaded) { break; }

				snprintf(inipath, PATH_MAX - 1, "%s/.ASM.ini", home);
				ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &gerror);
				if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
				if (ini_loaded) { break; }
			}


			// Last resort - perhaps there is a system-wide settings file?
			snprintf(inipath, PATH_MAX - 1, "/etc/asm.ini");
			ini_loaded = g_key_file_load_from_file(asm_ini, "/etc/asm.ini", G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }

			snprintf(inipath, PATH_MAX - 1, "/etc/ASM.ini");
			ini_loaded = g_key_file_load_from_file(asm_ini, "/etc/ASM.ini", G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }

			snprintf(inipath, PATH_MAX - 1, "/usr/local/etc/asm.ini");
			ini_loaded = g_key_file_load_from_file(asm_ini, "/usr/local/etc/asm.ini", G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }

			snprintf(inipath, PATH_MAX - 1, "/usr/local/etc/ASM.ini");
			ini_loaded = g_key_file_load_from_file(asm_ini, "/usr/local/etc/ASM.ini", G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }
		} while (0);

		if (ini_loaded)
		{
			int ival;
			gchar *value;

			asmlog_info("Reading settings from %s", inipath);

			value = g_key_file_get_string(asm_ini, "ASM", "enableAPImonitoring", NULL);
			if (value != NULL)
			{
				if (isdigit(*value))
				{
					ival = atoi(value);
					if (errno == 0 && ival >= 0 && ival < 3) {
						settings.enableAPImonitoring = ival;
					}
				}
				free(value);
			}

			value = g_key_file_get_string(asm_ini, "ASM", "enableProfilePrefixSlotSelection", NULL);
			if (value != NULL)
			{
				if (isdigit(*value))
				{
					ival = atoi(value);
					if (errno == 0 && ival >= 0 && ival < 2) {
						settings.enableProfilePrefixSlotSelection = ival;
					}
				}
				free(value);
			}

			value = g_key_file_get_string(asm_ini, "ASM", "objectcountinterval0", NULL);
			if (value != NULL)
			{
				if (isdigit(*value))
				{
					ival = atoi(value);
					if (errno == 0 && ival >= 0) {
						strncpy(settings.OCI0, value, sizeof(settings.OCI0));
						settings.OCI0[sizeof(settings.OCI0) - 1] = '\0';
					}
				}
				free(value);
			}

			value = g_key_file_get_string(asm_ini, "ASM", "objectcountinterval1", NULL);
			if (value != NULL)
			{
				if (isdigit(*value))
				{
					ival = atoi(value);
					if (errno == 0 && ival >= 0) {
						strncpy(settings.OCI1, value, sizeof(settings.OCI1));
						settings.OCI1[sizeof(settings.OCI1) - 1] = '\0';
					}
				}
				free(value);
			}

			value = g_key_file_get_string(asm_ini, "ASM", "objectcountinterval2", NULL);
			if (value != NULL)
			{
				if (isdigit(*value))
				{
					ival = atoi(value);
					if (errno == 0 && ival >= 0) {
						strncpy(settings.OCI2, value, sizeof(settings.OCI2));
						settings.OCI2[sizeof(settings.OCI2) - 1] = '\0';
					}
				}
				free(value);
			}

			value = g_key_file_get_string(asm_ini, "ASM", "objectcountcommand0", NULL);
			if (value != NULL) {
				strncpy(settings.OCC0, value, sizeof(settings.OCC0));
				settings.OCC0[sizeof(settings.OCC0) - 1] = '\0';
				free(value);
			}

			value = g_key_file_get_string(asm_ini, "ASM", "objectcountcommand1", NULL);
			if (value != NULL) {
				strncpy(settings.OCC1, value, sizeof(settings.OCC1));
				settings.OCC1[sizeof(settings.OCC1) - 1] = '\0';
				free(value);
			}

			value = g_key_file_get_string(asm_ini, "ASM", "objectcountcommand2", NULL);
			if (value != NULL) {
				strncpy(settings.OCC2, value, sizeof(settings.OCC2));
				settings.OCC2[sizeof(settings.OCC2) - 1] = '\0';
				free(value);
			}
		}
		else
		{
			asmlog_warning("No ASM.ini file could be loaded - using default values.");
		}
		g_key_file_free(asm_ini);
	}

	return status;
}
