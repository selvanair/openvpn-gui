/*
 *  OpenVPN-GUI -- A Windows GUI for OpenVPN.
 *
 *  Copyright (C) 2004 Mathias Sundman <mathias@nilings.se>
 *                2010 Heiko Hund <heikoh@users.sf.net>
 *                2016 Selva Nair <selva.nair@gmail.com>
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

#include "main.h"
#include "openvpn-gui-res.h"
#include "options.h"
#include "localization.h"
#include "save_pass.h"
#include "misc.h"
#include "passphrase.h"

typedef enum
{
    match_false,
    match_file,
    match_dir
} match_t;

extern options_t o;

static match_t
match(const WIN32_FIND_DATA *find, const TCHAR *ext)
{
    size_t ext_len = _tcslen(ext);
    int i;

    if (find->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return match_dir;

    if (ext_len == 0)
        return match_file;

    i = _tcslen(find->cFileName) - ext_len - 1;

    if (i > 0 && find->cFileName[i] == '.'
    && _tcsicmp(find->cFileName + i + 1, ext) == 0)
        return match_file;

    return match_false;
}

static bool
CheckReadAccess (const TCHAR *dir, const TCHAR *file)
{
    TCHAR path[MAX_PATH];

    _sntprintf_0 (path, _T("%s\\%s"), dir, file);

    return CheckFileAccess (path, GENERIC_READ);
}

static int
ConfigAlreadyExists(TCHAR *newconfig)
{
    int i;
    for (i = 0; i < o.num_configs; ++i)
    {
        if (_tcsicmp(o.conn[i].config_file, newconfig) == 0)
            return true;
    }
    return false;
}

static void
AddConfigFileToList(int config, const TCHAR *filename, const TCHAR *config_dir)
{
    connection_t *c = &o.conn[config];
    int i;

    memset(c, 0, sizeof(*c));

    _tcsncpy(c->config_file, filename, _countof(c->config_file) - 1);
    _tcsncpy(c->config_dir, config_dir, _countof(c->config_dir) - 1);
    _tcsncpy(c->config_name, c->config_file, _countof(c->config_name) - 1);
    c->config_name[_tcslen(c->config_name) - _tcslen(o.ext_string) - 1] = _T('\0');
    _sntprintf_0(c->log_path, _T("%s\\%s.log"), o.log_dir, c->config_name);

    c->manage.sk = INVALID_SOCKET;
    c->manage.skaddr.sin_family = AF_INET;
    c->manage.skaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    c->manage.skaddr.sin_port = htons(25340 + config);

#ifndef DISABLE_CHANGE_PASSWORD
    if (CheckKeyFileWriteAccess (c))
        c->flags |= FLAG_ALLOW_CHANGE_PASSPHRASE;
#endif

    /* Check if connection should be autostarted */
    for (i = 0; i < o.num_auto_connect; ++i)
    {
        if (_tcsicmp(c->config_file, o.auto_connect[i]) == 0
            || _tcsicmp(c->config_name, o.auto_connect[i]) == 0)
        {
            c->auto_connect = true;
            break;
        }
    }
    /* check whether passwords are saved */
    if (o.disable_save_passwords)
    {
        DisableSavePasswords(c);
    }
    else
    {
        if (IsAuthPassSaved(c->config_name))
            c->flags |= FLAG_SAVE_AUTH_PASS;
        if (IsKeyPassSaved(c->config_name))
            c->flags |= FLAG_SAVE_KEY_PASS;
    }
}

#define FLAG_WARN_DUPLICATES        (0x1)
#define FLAG_WARN_MAX_CONFIGS       (0x2)
#define FLAG_SET_CONFIG_GROUPS      (0x4)

/* Associate a group name with a config. For now we just use
 * the name of the subdirectory in which the config resides
 * or an empty string L"" if the config is in the root of
 * config_dir or global_config_dir.
 * This name is used when a hierarchical view of the config list
 * is activated.
 */
static void
SetConfigGroup(int config_id, int group_id, int flags)
{
    PrintDebug(L"%S: config_id = %d group_id = %d flags = %d",
              __func__, config_id, group_id, flags);
    if (config_id < 0 || config_id >= o.num_configs)
        return;

    connection_t *c = &o.conn[config_id];

    c->group_id = (flags & FLAG_SET_CONFIG_GROUPS) ? group_id : 0;
    PrintDebug(L"config %d name %s: set group_id = %d group_name = %s",
               config_id, c->config_name, c->group_id, o.groups[c->group_id]);
}

/*
 * Create a new group below the parent group with parent_id
 * and return the id of the new group.
 * Input: parent_id (-1 if no parent)
 * Group created only if FLAG_SET_CONFIG_GROUPS is set
 * else return parent_id.
 */
static int
NewConfigGroup(const wchar_t *name, int parent_id, int flags)
{
    if (!(flags & FLAG_SET_CONFIG_GROUPS))
        return parent_id;

    if (!o.groups || o.num_groups == o.max_groups)
    {
        o.max_groups += 10;
        void *tmp = realloc(o.groups, sizeof(*o.groups)*o.max_groups);
        if (!tmp)
        {
            o.max_groups -= 10;
            ErrorExit(1, L"Out of memory while grouping configs");
        }
        o.groups = tmp;
    }

    config_group_t *cg = &o.groups[o.num_groups];
    _sntprintf_0(cg->name, L"%s", name);
    cg->parent_id = parent_id;
    cg->active = false;          /* Activated if not empty by ActivateConfigGroups */
    cg->children = 0;
    cg->id = o.num_groups;

    return o.num_groups++;
}

/*
 * All configs that links at least one config to the root are
 * enabled. Dangling entries with no terminal configs will stay
 * disabled and are not displayed in the menu tree.
 */
static void
ActivateConfigGroups(int flags)
{
    if (!(flags & FLAG_SET_CONFIG_GROUPS))
        return;

    o.groups[0].active = true;

    /* count children of each group -- this includes groups and configs */
    for (int i = 0; i < o.num_groups; i++)
    {
        int j = o.groups[i].parent_id;
        o.groups[j].children++;
    }
    for (int i = 0; i < o.num_configs; i++)
    {
        int j = o.conn[i].group_id;
        o.groups[j].children++;
    }

    /* Squash single config directories one depth up.
     * This is done so that automatically imported configs
     * which are added as a single config per in a directory
     * are handled like a config in the parent directory.
     * Makes navigation easier.
     */
    for (int i = 0; i < o.num_configs; i++)
    {
        int j = o.conn[i].group_id;
        config_group_t *cg = &o.groups[j];

        if (cg->parent_id !=1 && cg->children == 1)
        {
            cg->children--;
            o.conn[i].group_id = cg->parent_id; /* set connection parent to one group up */
        }
    }

    /* activate all groups that connect a config to the root */
    for (int i = 0; i < o.num_configs; i++)
    {
        int j = o.conn[i].group_id;
        config_group_t *cg = &o.groups[j];

        while (cg->parent_id != -1)
        {
            cg->active = true;
            cg = &o.groups[cg->parent_id];
        }
    }
}

/* Scan for configs in config_dir recursing down up to recurse_depth.
 * Input: config_dir -- root of the directory to scan from
 *        group_id   -- id of the group into which add the configs to
 *        flags      -- FLAG_WARN_DUPLICATES to warn duplicate configs with same name
 * Currently configs in a directory are grouped together so group_id is
 * the index of the parent directory in a directory array.
 * This may be recursively called until depth becomes 1 and each time
 * the group_id is changed to that of the directory being recursed into.
 */
static void
BuildFileList0(const TCHAR *config_dir, int recurse_depth, int group_id, int flags)
{
    WIN32_FIND_DATA find_obj;
    HANDLE find_handle;
    TCHAR find_string[MAX_PATH];
    TCHAR subdir_name[MAX_PATH];

    _sntprintf_0(find_string, _T("%s\\*"), config_dir);
    find_handle = FindFirstFile(find_string, &find_obj);
    if (find_handle == INVALID_HANDLE_VALUE)
        return;

    PrintDebug(L"Scanning configs in %s with group_id %d", config_dir, group_id);
    /* Loop over each config file in config dir */
    do
    {
        if (!o.conn || o.num_configs == o.max_configs)
        {
            o.max_configs += 50;
            PrintDebug(L"(Re)allocating options.conn to hold %d configs. num_configs = %d", o.max_configs, o.num_configs);
            void *tmp = realloc(o.conn, sizeof(*o.conn)*o.max_configs);
            if (!tmp)
            {
                o.max_configs -= 50;
                FindClose(find_handle);
                ErrorExit(1, L"Out of memory while scanning configs");
            }
            o.conn = tmp;
        }

        match_t match_type = match(&find_obj, o.ext_string);
        if (match_type == match_file)
        {
            if (ConfigAlreadyExists(find_obj.cFileName))
            {
                if (flags & FLAG_WARN_DUPLICATES)
                    ShowLocalizedMsg(IDS_ERR_CONFIG_EXIST, find_obj.cFileName);
                continue;
            }

            if (CheckReadAccess (config_dir, find_obj.cFileName))
            {
                AddConfigFileToList(o.num_configs++, find_obj.cFileName, config_dir);
                SetConfigGroup(o.num_configs-1, group_id, flags);
            }
        }
    } while (FindNextFile(find_handle, &find_obj));

    FindClose(find_handle);

    /* optionally loop over each subdir */
    if (recurse_depth <= 1)
        return;

    find_handle = FindFirstFile (find_string, &find_obj);
    if (find_handle == INVALID_HANDLE_VALUE)
        return;

    do
    {
        match_t match_type = match(&find_obj, o.ext_string);
        if (match_type == match_dir)
        {
            if (wcscmp(find_obj.cFileName, _T("."))
                &&  wcscmp(find_obj.cFileName, _T("..")))
            {
                /* recurse into subdirectory */
                _sntprintf_0(subdir_name, _T("%s\\%s"), config_dir, find_obj.cFileName);
                int sub_group = NewConfigGroup(find_obj.cFileName, group_id, flags);
                BuildFileList0(subdir_name, recurse_depth - 1, sub_group, flags);
            }
        }
    } while (FindNextFile(find_handle, &find_obj));

    FindClose(find_handle);
}

void
BuildFileList()
{
    static bool issue_warnings = true;
    int recurse_depth = 4; /* read config_dir and 3 levels of sub-directories */
    int flags = 0;
    int group_id = 0;

    if (o.silent_connection)
        issue_warnings = false;

    /*
     * If no connections are active reset num_configs and rescan
     * to make a new list. Else we keep all current configs and
     * rescan to add any new one's found
     * Config file grouping is enabled only in the former case.
     */
    if (CountConnState(disconnected) == o.num_configs)
    {
        o.num_configs = 0;
        o.num_groups = 0;
        flags |= FLAG_SET_CONFIG_GROUPS;
        group_id = NewConfigGroup(L"User Profiles", -1, flags);
    }

    if (issue_warnings)
    {
        flags |= FLAG_WARN_DUPLICATES | FLAG_WARN_MAX_CONFIGS;
    }

    BuildFileList0 (o.config_dir, recurse_depth, group_id, flags);

    if (flags & FLAG_SET_CONFIG_GROUPS)
    {
        group_id = NewConfigGroup(L"System Profiles", 0, flags);
    }
    if (_tcscmp (o.global_config_dir, o.config_dir))
        BuildFileList0 (o.global_config_dir, recurse_depth, group_id, flags);

    if (o.num_configs == 0 && issue_warnings)
        ShowLocalizedMsg(IDS_NFO_NO_CONFIGS, o.config_dir, o.global_config_dir);

    /* More than MAX_CONFIGS are ignored in the menu listing: see tun.c */
    if (o.num_configs > MAX_CONFIGS)
    {
        if (issue_warnings)
            ShowLocalizedMsg(IDS_ERR_MANY_CONFIGS, o.num_configs);
        o.num_configs = MAX_CONFIGS; /* ignore the rest of configs */
    }

    ActivateConfigGroups(flags);

    issue_warnings = false;
}
