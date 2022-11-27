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
 */
void read_settings(void)
{
	const char *home;
	char  inipath[PATH_MAX];

	GKeyFile* asm_ini;
	GError*   error      = NULL;
	gboolean  ini_loaded = FALSE;
	gint ival;
	gchar *value;

	memset(inipath, 0, PATH_MAX);

	home = getenv("HOME");
	if (home == NULL) {
		home = "";
	}

	asm_ini = g_key_file_new();

	do
	{
		// First, try loading ASM.ini from the current working directory
		snprintf(inipath, PATH_MAX - 1, "./asm.ini");
		ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &error);
		g_clear_error(&error);
		if (ini_loaded) { break; }

		snprintf(inipath, PATH_MAX - 1, "./ASM.ini");
		ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &error);
		g_clear_error(&error);
		if (ini_loaded) { break; }


		// Then try the per-user ASM settings file
		if (home) {
			snprintf(inipath, PATH_MAX - 1, "%s/etc/asm.ini", home);
			ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &error);
			g_clear_error(&error);
			if (ini_loaded) { break; }

			snprintf(inipath, PATH_MAX - 1, "%s/etc/ASM.ini", home);
			ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &error);
			g_clear_error(&error);
			if (ini_loaded) { break; }

			snprintf(inipath, PATH_MAX - 1, "%s/.asm.ini", home);
			ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &error);
			g_clear_error(&error);
			if (ini_loaded) { break; }

			snprintf(inipath, PATH_MAX - 1, "%s/.ASM.ini", home);
			ini_loaded = g_key_file_load_from_file(asm_ini, inipath, G_KEY_FILE_NONE, &error);
			g_clear_error(&error);
			if (ini_loaded) { break; }
		}


		// Last resort - perhaps there is a system-wide settings file?
		snprintf(inipath, PATH_MAX - 1, "/etc/asm.ini");
		ini_loaded = g_key_file_load_from_file(asm_ini, "/etc/asm.ini", G_KEY_FILE_NONE, &error);
		g_clear_error(&error);
		if (ini_loaded) { break; }

		snprintf(inipath, PATH_MAX - 1, "/etc/ASM.ini");
		ini_loaded = g_key_file_load_from_file(asm_ini, "/etc/ASM.ini", G_KEY_FILE_NONE, &error);
		g_clear_error(&error);
		if (ini_loaded) { break; }

		snprintf(inipath, PATH_MAX - 1, "/usr/local/etc/asm.ini");
		ini_loaded = g_key_file_load_from_file(asm_ini, "/usr/local/etc/asm.ini", G_KEY_FILE_NONE, &error);
		g_clear_error(&error);
		if (ini_loaded) { break; }

		snprintf(inipath, PATH_MAX - 1, "/usr/local/etc/ASM.ini");
		ini_loaded = g_key_file_load_from_file(asm_ini, "/usr/local/etc/ASM.ini", G_KEY_FILE_NONE, &error);
		g_clear_error(&error);
	} while (0);

	if (!ini_loaded) {
		asmlog_warning("No ASM.ini file found - using default values.");
		goto done;
	}

	asmlog_info("Reading settings from %s", inipath);

	ival = g_key_file_get_integer(asm_ini, "ASM", "enableAPImonitoring", &error);
	if (error != NULL) {
		asmlog_warning("asm.ini: %s", error->message);
		g_clear_error(&error);
	} else {
		if (ival >= 0 && ival < 3) {
			settings.enableAPImonitoring = ival;
		}
	}

	ival = g_key_file_get_integer(asm_ini, "ASM", "enableProfilePrefixSlotSelection", &error);
	if (error != NULL) {
		asmlog_warning("asm.ini: %s", error->message);
		g_clear_error(&error);
	} else {
		if (ival >= 0 && ival < 2) {
			settings.enableProfilePrefixSlotSelection = ival;
		}
	}

	ival = g_key_file_get_integer(asm_ini, "ASM", "objectcountinterval0", &error);
	if (error != NULL) {
		asmlog_warning("asm.ini: %s", error->message);
		g_clear_error(&error);
	} else {
		if (ival >= 0) {
			snprintf(settings.OCI0, sizeof(settings.OCI0), "%d", ival);
			settings.OCI0[sizeof(settings.OCI0) - 1] = '\0';
		}
	}

	ival = g_key_file_get_integer(asm_ini, "ASM", "objectcountinterval1", &error);
	if (error != NULL) {
		asmlog_warning("asm.ini: %s", error->message);
		g_clear_error(&error);
	} else {
		if (ival >= 0) {
			snprintf(settings.OCI1, sizeof(settings.OCI1), "%d", ival);
			settings.OCI1[sizeof(settings.OCI1) - 1] = '\0';
		}
	}

	ival = g_key_file_get_integer(asm_ini, "ASM", "objectcountinterval2", &error);
	if (error != NULL) {
		asmlog_warning("asm.ini: %s", error->message);
		g_clear_error(&error);
	} else {
		if (ival >= 0) {
			snprintf(settings.OCI2, sizeof(settings.OCI2), "%d", ival);
			settings.OCI2[sizeof(settings.OCI2) - 1] = '\0';
		}
	}

	value = g_key_file_get_string(asm_ini, "ASM", "objectcountcommand0", &error);
	if (error != NULL) {
		asmlog_warning("asm.ini: %s", error->message);
		g_clear_error(&error);
	} else {
		if (strlen(value) < sizeof(settings.OCC0)) {
			snprintf(settings.OCC0, sizeof(settings.OCC0), "%s", value);
		}
		g_free(value);
	}

	value = g_key_file_get_string(asm_ini, "ASM", "objectcountcommand1", &error);
	if (error != NULL) {
		asmlog_warning("asm.ini: %s", error->message);
		g_clear_error(&error);
	} else {
		if (strlen(value) < sizeof(settings.OCC1)) {
			snprintf(settings.OCC1, sizeof(settings.OCC1), "%s", value);
		}
		g_free(value);
	}

	value = g_key_file_get_string(asm_ini, "ASM", "objectcountcommand2", &error);
	if (error != NULL) {
		asmlog_warning("asm.ini: %s", error->message);
		g_clear_error(&error);
	} else {
		if (strlen(value) < sizeof(settings.OCC2)) {
			snprintf(settings.OCC2, sizeof(settings.OCC2), "%s", value);
		}
		g_free(value);
	}

done:
	g_key_file_free(asm_ini);
}
