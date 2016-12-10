/*
 *  OpenVPN-GUI -- A Windows GUI for OpenVPN.
 *
 *  Copyright (C) 2016 Selva Nair <selva.nair@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H
#include <windows.h>

typedef struct config_parser config_parser_t;

config_parser_t *OpenConfig (const wchar_t *fname);
void CloseConfig (config_parser_t *cp);
wchar_t *ConfigReadline (config_parser_t *cp);
int ConfigNumTokens (config_parser_t *cp);
wchar_t *ConfigGetToken (config_parser_t *cp, int n);
BOOL HasConfigOption(const wchar_t * config_file, const wchar_t *option);

#endif
