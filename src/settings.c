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
#include "util.h"

// Settings and their default values
int  enableAPImonitoring  =  0;				// Because not implemented
int  enableProfilePrefixSlotSelection = 1;
char OCI0[SMALSTRINGSIZE] = "30";
char OCI1[SMALSTRINGSIZE] = "60";
char OCI2[SMALSTRINGSIZE] = "0";
char OCC0[FUNCTIONSIZE]   = "count entities \"\"All\"\";";
char OCC1[FUNCTIONSIZE]   = "count vehicles;";
char OCC2[FUNCTIONSIZE]   = "count allMissionObjects \"\"All\"\";";


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

	GKeyFile* settings   = NULL;
	GError*   gerror     = NULL;
	gboolean  ini_loaded = FALSE;


	memset(inipath, 0, PATH_MAX);

	home = getenv("HOME");

	settings = g_key_file_new();
	if (settings == NULL)
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
			ini_loaded = g_key_file_load_from_file(settings, inipath, G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }

			snprintf(inipath, PATH_MAX - 1, "./ASM.ini");
			ini_loaded = g_key_file_load_from_file(settings, inipath, G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }


			// Then try the per-user ASM settings file
			if (home) {
				snprintf(inipath, PATH_MAX - 1, "%s/etc/asm.ini", home);
				ini_loaded = g_key_file_load_from_file(settings, inipath, G_KEY_FILE_NONE, &gerror);
				if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
				if (ini_loaded) { break; }

				snprintf(inipath, PATH_MAX - 1, "%s/etc/ASM.ini", home);
				ini_loaded = g_key_file_load_from_file(settings, inipath, G_KEY_FILE_NONE, &gerror);
				if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
				if (ini_loaded) { break; }

				snprintf(inipath, PATH_MAX - 1, "%s/.asm.ini", home);
				ini_loaded = g_key_file_load_from_file(settings, inipath, G_KEY_FILE_NONE, &gerror);
				if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
				if (ini_loaded) { break; }

				snprintf(inipath, PATH_MAX - 1, "%s/.ASM.ini", home);
				ini_loaded = g_key_file_load_from_file(settings, inipath, G_KEY_FILE_NONE, &gerror);
				if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
				if (ini_loaded) { break; }
			}


			// Last resort - perhaps there is a system-wide settings file?
			ini_loaded = g_key_file_load_from_file(settings, "/etc/asm.ini", G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }

			ini_loaded = g_key_file_load_from_file(settings, "/etc/ASM.ini", G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }

			ini_loaded = g_key_file_load_from_file(settings, "/usr/local/etc/asm.ini", G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }

			ini_loaded = g_key_file_load_from_file(settings, "/usr/local/etc/ASM.ini", G_KEY_FILE_NONE, &gerror);
			if (gerror != NULL) { g_error_free(gerror); gerror = NULL; }
			if (ini_loaded) { break; }
		} while (0);

		if (ini_loaded)
		{
			int ival;
			gchar *value;

			asmlog_info("Reading settings from %s", inipath);

			value = g_key_file_get_string(settings, "ASM", "enableAPImonitoring", NULL);
			if (value != NULL)
			{
				if (isdigit(*value))
				{
					ival = atoi(value);
					if (errno == 0 && ival >= 0 && ival < 3) { enableAPImonitoring = ival; }
				}
				free(value);
			}

			value = g_key_file_get_string(settings, "ASM", "enableProfilePrefixSlotSelection", NULL);
			if (value != NULL)
			{
				if (isdigit(*value))
				{
					ival = atoi(value);
					if (errno == 0 && ival >= 0 && ival < 2) { enableProfilePrefixSlotSelection = ival; }
				}
				free(value);
			}

			value = g_key_file_get_string(settings, "ASM", "objectcountinterval0", NULL);
			if (value != NULL)
			{
				if (isdigit(*value))
				{
					ival = atoi(value);
					if (errno == 0 && ival >= 0) { strncpy(OCI0, value, SMALSTRINGSIZE); OCI0[SMALSTRINGSIZE - 1] = '\0'; }
				}
				free(value);
			}

			value = g_key_file_get_string(settings, "ASM", "objectcountinterval1", NULL);
			if (value != NULL)
			{
				if (isdigit(*value))
				{
					ival = atoi(value);
					if (errno == 0 && ival >= 0) { strncpy(OCI1, value, SMALSTRINGSIZE); OCI1[SMALSTRINGSIZE - 1] = '\0';}
				}
				free(value);
			}

			value = g_key_file_get_string(settings, "ASM", "objectcountinterval2", NULL);
			if (value != NULL)
			{
				if (isdigit(*value))
				{
					ival = atoi(value);
					if (errno == 0 && ival >= 0) { strncpy(OCI2, value, SMALSTRINGSIZE); OCI2[SMALSTRINGSIZE - 1] = '\0'; }
				}
				free(value);
			}

			value = g_key_file_get_string(settings, "ASM", "objectcountcommand0", NULL);
			if (value != NULL) { strncpy(OCC0, value, SMALSTRINGSIZE); OCC0[SMALSTRINGSIZE - 1] = '\0'; free(value); }

			value = g_key_file_get_string(settings, "ASM", "objectcountcommand1", NULL);
			if (value != NULL) { strncpy(OCC1, value, SMALSTRINGSIZE); OCC1[SMALSTRINGSIZE - 1] = '\0'; free(value); }

			value = g_key_file_get_string(settings, "ASM", "objectcountcommand2", NULL);
			if (value != NULL) { strncpy(OCC2, value, SMALSTRINGSIZE); OCC2[SMALSTRINGSIZE - 1] = '\0'; free(value); }
		}
		else
		{
			asmlog_warning("No ASM.ini file could be loaded - using default values.");
		}
		g_key_file_free(settings);
	}

	return status;
}
