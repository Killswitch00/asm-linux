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
 * beuseful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Arma Server Monitor for Linux; if not, see
 * <http://www.gnu.org/licenses/>.
 */


#ifndef ASMSETTINGS_H
#define ASMSETTINGS_H

extern int enableAPImonitoring;
extern int enableProfilePrefixSlotSelection;
extern char OCI0[];
extern char OCI1[];
extern char OCI2[];
extern char OCC0[];
extern char OCC1[];
extern char OCC2[];

int read_settings(void);

#endif /* ASMSETTINGS_H */
