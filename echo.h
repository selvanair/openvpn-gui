/*
 *  OpenVPN-GUI -- A Windows GUI for OpenVPN.
 *
 *  Copyright (C) 2013 Selva Nair <selva.nair@gmail.com>
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

#ifndef ECHO_H
#define ECHO_H

#include <wchar.h>
#include "options.h"

struct env_item;
struct env_item {
    wchar_t *nameval;
    struct env_item *next;
};
struct env_item *env_item_new(const char *nameval);
struct env_item *env_item_add(struct env_item *head, struct env_item *item);
void env_item_del_all(struct env_item *head);
BOOL is_valid_env_name(const char *name);
void process_echo(connection_t *c, time_t timestamp, const char *msg);

#endif
