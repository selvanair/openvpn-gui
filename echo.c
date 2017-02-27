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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <windows.h>
#include <wchar.h>
#include "main.h"
#include "echo.h"
#include "save_pass.h"
#include "misc.h"
#include "openvpn.h"

/* To match with openvpn we accept only :ALPHA:, :DIGIT: or '_' in names */
BOOL
is_valid_env_name(const char *name)
{
    if (strlen(name) == 0)
    {
        PrintDebug(L"Empty env var name rejected");
        return false;
    }
    while (*name)
    {
        const char c = *name;
        if (!isalnum(c) && c != '_')
        {
            PrintDebug(L"Invalid character '%c' in env var name", c);
            return false;
        }
        name++;
    }
    return true;
}

/* Delete an env var item with matching name: if name is of the
 * form xxx=yyy, only the part xxx is used for matching.
 */
static struct env_item *
env_item_del(struct env_item *head, const WCHAR *name)
{
    struct env_item *item, *prev = NULL;
    BOOL found = FALSE;

    for (item = head; item; item = item->next)
    {
        const WCHAR *s1 = item->nameval;
        const WCHAR *s2 = name;
        while (*s1 && *s2 && *s1++ == *s2++)
        {
            if (*s1 == L'=') /* end of name part of nameval */
            {
                if ((*s2 == L'=') || (*s2 == L'\0')) /* name matches */
                    found = TRUE;
                break; /* out of the while loop */
            }
        }
        if (found)
        {
            if (prev)
                prev->next = item->next;
            else
                head = item->next;
            free(item->nameval);
            free(item);
            PrintDebug(L"env item with name matching '%s' deleted", name);
            break;
        }
        prev = item;
    }
    return head;
}

/* Create a new env item from name=val */
struct env_item *
env_item_new(const char *nameval)
{
    struct env_item *new = malloc(sizeof(struct env_item));

    if (!new)
    {
        PrintDebug(L"No memory for new env item");
        return NULL;
    }
    new->nameval = Widen(nameval);

    new->next = NULL;
    if (!new->nameval)
    {
        PrintDebug(L"No memory for new env item");
        free(new);
        return NULL;
    }
    return new;
}

/* Add a env var item to an env set: any existing item
 * with same name is replaced by the new entry
 */
struct env_item *
env_item_add(struct env_item *head, struct env_item *item)
{
    /* delete any existing item with same name */
    head = env_item_del(head, item->nameval);
    item->next = head;
    return item;
}

void
env_item_del_all(struct env_item *head)
{
    struct env_item *next;
    for ( ; head; head = next)
    {
        next = head->next;
        free(head->nameval);
        free(head);
    }
}

void
process_echo(connection_t *c, UNUSED time_t timestamp, const char *msg)
{
    wchar_t errmsg[256];

    if (strcmp(msg, "forget-passwords") == 0)
    {
        DeleteSavedPasswords(c->config_name);
    }
    else if (strcmp(msg, "save-passwords") == 0)
    {
        c->flags |= (FLAG_SAVE_KEY_PASS | FLAG_SAVE_AUTH_PASS);
    }
    else if (strbegins(msg, "setenv "))
    {
        /* add name=val to private env set with name prefixed by OPENVPN_ */
        msg = strchr(msg, ' ') + 1;
        const char *prefix = "OPENVPN_";
        char *p;
        char *nameval;

        if (*msg == ' ')
        {
            WriteStatusLog(c, L"GUI> ", L"Error: name empty in echo setenv", false);
            return;
        }

        nameval = malloc(strlen(prefix) + strlen(msg));
        if (!nameval)
        {
            WriteStatusLog(c, L"GUI> ", L"Error: out of memory for adding env var", false);
            return;
        }

        strcpy(nameval, prefix);
        strcat(nameval, msg);

        if ((p = strchr(nameval, ' ')) != NULL)
        {
            *p = '\0';
            if (is_valid_env_name(nameval))
            {
                *p = '=';
                PrintDebug(L"Adding env var '%S'", nameval);
                struct env_item *new = env_item_new(nameval);
                if (new)
                {
                    /* this removes any exiting item with same name */
                    c->es = env_item_add(c->es, new);
                }
                else
                    WriteStatusLog(c, L"GUI> ", L"Error: no memory for adding env var", false);
            }
            else
                WriteStatusLog(c, L"GUI> ", L"Error: empty or illegal name in echo setenv", false);
            free(nameval);
        }
        else
        {
            WriteStatusLog(c, L"GUI>", L"Error: no value specified in echo setenv", false);
            PrintDebug(L"Error: no value specified in 'echo setenv %S'", msg);
        }
    }
    else
    {
        _sntprintf_0(errmsg, L"WARNING: Unknown ECHO directive '%S' ignored.", msg);
        WriteStatusLog(c, L"GUI> ", errmsg, false);
    }
}
