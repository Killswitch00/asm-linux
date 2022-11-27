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


#ifndef ASMSETTINGS_H
#define ASMSETTINGS_H

#include "asm.h"

typedef struct {
	int enableAPImonitoring;
	int enableProfilePrefixSlotSelection;
	char OCI0[SMALSTRINGSIZE];
	char OCI1[SMALSTRINGSIZE];
	char OCI2[SMALSTRINGSIZE];
	char OCC0[FUNCTIONSIZE];
	char OCC1[FUNCTIONSIZE];
	char OCC2[FUNCTIONSIZE];
} asm_settings;

extern asm_settings settings;

void read_settings(void);

#endif /* ASMSETTINGS_H */
