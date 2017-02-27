/*
 *  OpenVPN-GUI -- A Windows GUI for OpenVPN.
 *
 *  Copyright (C) 2004 Mathias Sundman <mathias@nilings.se>
 *                2011 Heiko Hund <heikoh@users.sf.net>
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
#include <process.h>
#include <tchar.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "main.h"
#include "openvpn-gui-res.h"
#include "options.h"
#include "misc.h"
#include "localization.h"

extern options_t o;

/**
 * Make an env block by appending process env block to items in es
 */
static WCHAR *
merge_env_block(const struct env_item *es)
{
    DWORD len1 = 0, len2 = 0;
    /* e should be treated as read-only though cannot be defined as such */
    WCHAR *e = GetEnvironmentStringsW();
    const struct env_item *item;
    const WCHAR *pc;

    if (!e)
    {
        PrintDebug(L"Failed to get process env strings: error = %lu", GetLastError());
        return NULL;
    }

    PrintDebug(L"Process env: ");
    for (pc = e; *pc; pc += wcslen(pc)+1)
    {
        PrintDebug(L"%s", pc);
    }
    len1 = (pc - e);

    for(item = es; item; item = item->next)
    {
        len2 += wcslen(item->nameval) + 1;
    }

    WCHAR *env = malloc(sizeof(WCHAR)*(len1 + len2 + 1));
    if (!env)
    {
        PrintDebug(L"Failed to allocate space for env (size = %lu wchars)", len1 + len2 +1);
        FreeEnvironmentStringsW(e);
        return NULL;
    }

    WCHAR *p = env;
    /* first add the custom env variables */
    for(item = es; item; item = item->next)
    {
        DWORD len = wcslen(item->nameval) + 1;
        memcpy(p, item->nameval, len*sizeof(WCHAR));
        p += len;
    }
    memcpy(p, e, len1*sizeof(WCHAR));
    p += len1;
    *p = L'\0';

#ifdef DEBUG
    PrintDebug(L"Combined env:");
    for (pc = env; *pc; pc += wcslen(pc)+1)
    {
        PrintDebug(L"%s", pc);
    }
#endif
    FreeEnvironmentStringsW(e);

    return env;
}

void
RunPreconnectScript(connection_t *c)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    TCHAR cmdline[256];
    DWORD exit_code;
    struct _stat st;
    int i;

    /* Cut off extention from config filename and add "_pre.bat" */
    int len = _tcslen(c->config_file) - _tcslen(o.ext_string) - 1;
    _sntprintf_0(cmdline, _T("%s\\%.*s_pre.bat"), c->config_dir, len, c->config_file);

    /* Return if no script exists */
    if (_tstat(cmdline, &st) == -1)
        return;

    CLEAR(si);
    CLEAR(pi);

    /* fill in STARTUPINFO struct */
    GetStartupInfo(&si);
    si.cb = sizeof(si);
    si.dwFlags = 0;
    si.wShowWindow = SW_SHOWDEFAULT;
    si.hStdInput = NULL;
    si.hStdOutput = NULL;

    /* make an env array with confg specific env appended to the process's env */
    WCHAR *env = c->es ? merge_env_block(c->es) : NULL;
    DWORD flags = CREATE_UNICODE_ENVIRONMENT;

    if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE,
                       (o.show_script_window ? flags|CREATE_NEW_CONSOLE : flags|CREATE_NO_WINDOW),
                       env, c->config_dir, &si, &pi))
    {
        free(env);
        return;
    }

    for (i = 0; i <= (int) o.preconnectscript_timeout; i++)
    {
        if (!GetExitCodeProcess(pi.hProcess, &exit_code))
            goto out;

        if (exit_code != STILL_ACTIVE)
            goto out;

        Sleep(1000);
    }
out:
    free(env);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}


void
RunConnectScript(connection_t *c, int run_as_service)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    TCHAR cmdline[256];
    DWORD exit_code;
    struct _stat st;
    int i;

    /* Cut off extention from config filename and add "_up.bat" */
    int len = _tcslen(c->config_file) - _tcslen(o.ext_string) - 1;
    _sntprintf_0(cmdline, _T("%s\\%.*s_up.bat"), c->config_dir, len, c->config_file);

    /* Return if no script exists */
    if (_tstat(cmdline, &st) == -1)
        return;

    if (!run_as_service)
        SetDlgItemText(c->hwndStatus, ID_TXT_STATUS, LoadLocalizedString(IDS_NFO_STATE_CONN_SCRIPT));

    CLEAR(si);
    CLEAR(pi);

    /* fill in STARTUPINFO struct */
    GetStartupInfo(&si);
    si.cb = sizeof(si);
    si.dwFlags = 0;
    si.wShowWindow = SW_SHOWDEFAULT;
    si.hStdInput = NULL;
    si.hStdOutput = NULL;

    /* make an env array with confg specific env appended to the process's env */
    WCHAR *env = c->es ? merge_env_block(c->es) : NULL;
    DWORD flags = CREATE_UNICODE_ENVIRONMENT;

    if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE,
                       (o.show_script_window ? flags|CREATE_NEW_CONSOLE : flags|CREATE_NO_WINDOW),
                       env, c->config_dir, &si, &pi))
    {
        PrintDebug(L"CreateProcess: error = %lu", GetLastError());
        ShowLocalizedMsg(IDS_ERR_RUN_CONN_SCRIPT, cmdline);
        free(env);
        return;
    }

    if (o.connectscript_timeout == 0)
        goto out;

    for (i = 0; i <= (int) o.connectscript_timeout; i++)
    {
        if (!GetExitCodeProcess(pi.hProcess, &exit_code))
        {
            ShowLocalizedMsg(IDS_ERR_GET_EXIT_CODE, cmdline);
            goto out;
        }

        if (exit_code != STILL_ACTIVE)
        {
            if (exit_code != 0)
                ShowLocalizedMsg(IDS_ERR_CONN_SCRIPT_FAILED, exit_code);
            goto out;
        }

        Sleep(1000);
    }

    ShowLocalizedMsg(IDS_ERR_RUN_CONN_SCRIPT_TIMEOUT, o.connectscript_timeout);

out:
    free(env);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}


void
RunDisconnectScript(connection_t *c, int run_as_service)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    TCHAR cmdline[256];
    DWORD exit_code;
    struct _stat st;
    int i;

    /* Cut off extention from config filename and add "_down.bat" */
    int len = _tcslen(c->config_file) - _tcslen(o.ext_string) - 1;
    _sntprintf_0(cmdline, _T("%s\\%.*s_down.bat"), c->config_dir, len, c->config_file);

    /* Return if no script exists */
    if (_tstat(cmdline, &st) == -1)
        return;

    if (!run_as_service)
        SetDlgItemText(c->hwndStatus, ID_TXT_STATUS, LoadLocalizedString(IDS_NFO_STATE_DISCONN_SCRIPT));

    CLEAR(si);
    CLEAR(pi);

    /* fill in STARTUPINFO struct */
    GetStartupInfo(&si);
    si.cb = sizeof(si);
    si.dwFlags = 0;
    si.wShowWindow = SW_SHOWDEFAULT;
    si.hStdInput = NULL;
    si.hStdOutput = NULL;

    /* make an env array with confg specific env appended to the process's env */
    WCHAR *env = c->es ? merge_env_block(c->es) : NULL;
    DWORD flags = CREATE_UNICODE_ENVIRONMENT;

    if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE,
                       (o.show_script_window ? flags|CREATE_NEW_CONSOLE : flags|CREATE_NO_WINDOW),
                       NULL, c->config_dir, &si, &pi))
    {
        free(env);
        return;
    }

    for (i = 0; i <= (int) o.disconnectscript_timeout; i++)
    {
        if (!GetExitCodeProcess(pi.hProcess, &exit_code))
            goto out;

        if (exit_code != STILL_ACTIVE)
            goto out;

        Sleep(1000);
    }
out:
    free(env);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}
