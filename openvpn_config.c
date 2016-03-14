/*
 *  OpenVPN-GUI -- A Windows GUI for OpenVPN.
 *
 *  Copyright (C) 2004 Mathias Sundman <mathias@nilings.se>
 *                2010 Heiko Hund <heikoh@users.sf.net>
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

typedef enum
{
    match_false,
    match_file,
    match_dir
} match_t;

extern options_t o;

static struct {
   BOOL *used;
   int size;
} index_array = {NULL, 0};

int
GetFreeIndex()
{
    int index = -1, i;

    if (index_array.size < o.num_configs + 1)
    {
#ifdef DEBUG
        PrintDebug(L"allocating index_array");
#endif
        index_array.used = realloc(index_array.used, (index_array.size + 10)*sizeof(*index_array.used));
        if (!index_array.used)
           return index;
        for (i = 0; i < 10; ++i)
           index_array.used[i + index_array.size] = FALSE;
        index_array.size += 10;
    }

    for (i = 0; i < index_array.size; ++i)
    {
        if (!index_array.used[i])
        {
            index = i;
            index_array.used[i] = TRUE;
            break;
        }
    }
#ifdef DEBUG
    PrintDebug(L"index_array size = %d returning index = %d num_configs = %d",
               index_array.size, index, o.num_configs);
#endif

    return index;
}

void
ReleaseIndex (int index)
{
    if (index >= 0 && index < index_array.size)
        index_array.used[index] = FALSE;
#ifdef DEBUG
    else
        PrintDebug(L"ReleaseIndex: index out of bounds: %d", index);
#endif
}

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
    HANDLE h;
    bool ret = FALSE;
    TCHAR path[MAX_PATH];

    _sntprintf_0(path, _T("%s\\%s"), dir, file);

    h = CreateFile (path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL, NULL);
    if ( h != INVALID_HANDLE_VALUE )
    {
        ret = TRUE;
        CloseHandle (h);
    }

    return ret;
}

static DWORD
PurgeConnections()
{
    connection_t **c, *found;
    DWORD i = 0;
#ifdef DEBUG
    PrintDebug (L"In PurgeConnections");
#endif
    for ( c = &o.conn; *c; c = &(*c)->next)
    {
        while (*c && (*c)->state == disconnected &&
               !CheckReadAccess((*c)->config_dir, (*c)->config_file))
        {
#ifdef DEBUG
            PrintDebug (L"In PurgeConnections deleting config %s", (*c)->config_name);
#endif
            found = *c;
            *c = found->next;
            ReleaseIndex (found->index);
            DeleteConnection (found);
            ++i;
        }
        if (!*c) break;
    }
#ifdef DEBUG
            PrintDebug (L"In PurgeConnections purged %d configs", i);
#endif
    return i;
}

static int
ConfigAlreadyExists(const TCHAR *config_file)
{
    return (GetConnByFile (config_file) != NULL);
}

static void
AddConfigFileToList(const TCHAR *filename, const TCHAR *config_dir)
{
    int i;
    connection_t *c;

#ifdef DEBUG
    PrintDebug (L"AddConfig: adding config # %d with file %s in dir %s",
                 o.num_configs, filename, config_dir);
#endif
    if ((c = NewConnection ()) == NULL)
    {
#ifdef DEBUG
        PrintDebug (L"NewConnection failed");
#endif
        exit (1);
    }

    c->index = GetFreeIndex();
    if (c->index < 0) /* BUG(): should not happen  -- TODO: handle error */
    {
        free(c);
#ifdef DEBUG
    PrintDebug (L"BUG c->index < 0 -- abort");
#endif
        exit (1);
    }

    _tcsncpy(c->config_file, filename, _countof(c->config_file) - 1);
    _tcsncpy(c->config_dir, config_dir, _countof(c->config_dir) - 1);
    _tcsncpy(c->config_name, c->config_file, _countof(c->config_name) - 1);
    c->config_name[_tcslen(c->config_name) - _tcslen(o.ext_string) - 1] = _T('\0');
    _sntprintf_0(c->log_path, _T("%s\\%s.log"), o.log_dir, c->config_name);

    c->manage.sk = INVALID_SOCKET;
    c->manage.skaddr.sin_family = AF_INET;
    c->manage.skaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    c->manage.skaddr.sin_port = htons(c->index + 25340);

    /* Check if connection should be autostarted */
    for (i = 0; i < MAX_AUTO_CONNECT && o.auto_connect[i]; ++i)
    {
        if (_tcsicmp(c->config_file, o.auto_connect[i]) == 0)
        {
            c->auto_connect = true;
            break;
        }
    }
#ifdef DEBUG
    PrintDebug (L"AddConfig: added config # %d with index = %d autostart = %d", o.num_configs, c->index, c->auto_connect);
#endif
    o.num_configs++;
}

#define ISSUE_WARNINGS 1

static void
BuildFileList0(const TCHAR *config_dir, DWORD flags)
{
    WIN32_FIND_DATA find_obj;
    HANDLE find_handle;
    TCHAR find_string[MAX_PATH];

    _sntprintf_0(find_string, _T("%s\\*"), config_dir);
    find_handle = FindFirstFile(find_string, &find_obj);
    if (find_handle == INVALID_HANDLE_VALUE)
        return;

    /* Loop over each config file in main config dir */
    do
    {
        match_t match_type = match(&find_obj, o.ext_string);
        if (match_type == match_file)
        {
            if (ConfigAlreadyExists(find_obj.cFileName))
            {
#ifdef DEBUG
                PrintDebug(L"Ignoring duplicate %s in %s", find_obj.cFileName, config_dir);
#endif
                if (flags & ISSUE_WARNINGS)
                    ShowLocalizedMsg(IDS_ERR_CONFIG_EXIST, find_obj.cFileName);
                continue;
            }

            if (CheckReadAccess (config_dir, find_obj.cFileName))
                AddConfigFileToList(find_obj.cFileName, config_dir);
        }
        else if (match_type == match_dir &&
                 _tcsncmp(find_obj.cFileName, _T("."), _tcslen(find_obj.cFileName)) != 0 &&
                 _tcsncmp(find_obj.cFileName, _T(".."), _tcslen(find_obj.cFileName)) != 0)
        {
            /* recurse into the subdir -- only one level */
            WIN32_FIND_DATA find_obj1;
            HANDLE find_handle1;
            TCHAR subdir[MAX_PATH];

            _sntprintf_0(subdir, _T("%s\\%s"), config_dir, find_obj.cFileName);
            _sntprintf_0(find_string, _T("%s\\*"), subdir);

            find_handle1 = FindFirstFile(find_string, &find_obj1);

            if (find_handle1 == INVALID_HANDLE_VALUE)
            {
#ifdef DEBUG
                PrintDebug(L"Searching %s failed", find_string);
#endif
                return; // TODO handle error
            }

            /* Loop over each config file in the subdir */
            do
            {
                match_t match_type = match(&find_obj1, o.ext_string);
                if (match_type != match_file)
                    continue;
                if (ConfigAlreadyExists(find_obj1.cFileName))
                {
#ifdef DEBUG
                    PrintDebug(L"Ignoring duplicate %s in %s", find_obj1.cFileName, subdir);
#endif
                    if (flags & ISSUE_WARNINGS)
                        ShowLocalizedMsg(IDS_ERR_CONFIG_EXIST, find_obj1.cFileName);
                    continue;
                }

                if (CheckReadAccess (subdir, find_obj1.cFileName))
                    AddConfigFileToList(find_obj1.cFileName, subdir);

            } while (FindNextFile (find_handle1, &find_obj1));

            FindClose (find_handle1);
        }
    } while (FindNextFile(find_handle, &find_obj));

    FindClose(find_handle);
}

void
BuildFileList()
{
    static int flags = ISSUE_WARNINGS;

    int purged = PurgeConnections ();

    o.num_configs -= purged;

    BuildFileList0 (o.config_dir, flags);

    if (_tcscmp (o.global_config_dir, o.config_dir))
        BuildFileList0 (o.global_config_dir, flags);

    if (o.num_configs == 0 && (flags & ISSUE_WARNINGS))
        ShowLocalizedMsg(IDS_NFO_NO_CONFIGS);

    flags &= ~ISSUE_WARNINGS;
}
