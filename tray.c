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
#include <shellapi.h>
#include <tchar.h>
#include <time.h>

#include "tray.h"
#include "service.h"
#include "main.h"
#include "options.h"
#include "openvpn.h"
#include "openvpn_config.h"
#include "openvpn-gui-res.h"
#include "localization.h"
#include "misc.h"
#include "assert.h"

/* Popup Menus */
HMENU hMenu;
HMENU hMenuConn[MAX_CONFIGS];
HMENU hMenuService;

/* A struct to store nodes in the menu tree */
typedef struct {
    HMENU h;         /* menu handle */
    int pos;         /* menu position within parent -- needed for sending messages */
    int entries;     /* number of entries in a menu */
} group_menu_t;

/* We need to construct the menu tree dynamically as the number of
 * of directory nodes is not guaranteed to be < MAX_CONFIGS.
 * But for connection profiles (configs) we use a static
 * array: wastes at most ~32KB
 */
group_menu_t *groupMenu;

int configPos[MAX_CONFIGS]; /* position of config in its group menu */

NOTIFYICONDATA ni;
extern options_t o;


#define USE_GROUPED_CONFIG_MENU ((o.config_menu_view == 0 && o.num_configs > 50) || (o.config_menu_view == 2))

static void
SetMenuImage(HMENU h, int pos)
{
    MENUITEMINFO mi =
    {
        .cbSize = sizeof(mi),
        .fMask = MIIM_BITMAP|MIIM_DATA,
        .hbmpItem = HBMMENU_SYSTEM,
    };
    mi.dwItemData = (ULONG_PTR) o.hWnd;
    SetMenuItemInfo(h, pos, TRUE, &mi);
}

/* Create popup menus */
void
CreatePopupMenus()
{
    int i;

    /* We use groupMenu[0].h as the root menu, so,
     * even if num_configs = 0, we want num_groups > 0.
     * This is guaranteed as the root node is always defined.
     */
    assert(o.num_groups > 0);

    void *tmp = realloc(groupMenu, sizeof(*groupMenu)*o.num_groups);
    if (!tmp)
    {
        ErrorExit(1, L"Out of memory while creating menus");
    }
    groupMenu = tmp;

    for (i = 0; i < o.num_configs; i++)
    {
        hMenuConn[i] = CreatePopupMenu();
    }
    for (i = 0; i < o.num_groups; i++)
    {
        groupMenu[i].h = CreatePopupMenu();
        groupMenu[i].entries = 0;
    }

    hMenuService = CreatePopupMenu();
    hMenu = groupMenu[0].h; /* the first group menu is also the root menu */

    if (o.num_configs == 1) {
        /* Create Main menu with actions */
        if (o.service_only == 0) {
            AppendMenu(hMenu, MF_STRING, IDM_CONNECTMENU, LoadLocalizedString(IDS_MENU_CONNECT));
            AppendMenu(hMenu, MF_STRING, IDM_DISCONNECTMENU, LoadLocalizedString(IDS_MENU_DISCONNECT));
            AppendMenu(hMenu, MF_STRING, IDM_RECONNECTMENU, LoadLocalizedString(IDS_MENU_RECONNECT));
            AppendMenu(hMenu, MF_STRING, IDM_STATUSMENU, LoadLocalizedString(IDS_MENU_STATUS));
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        }
        else {
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_START, LoadLocalizedString(IDS_MENU_SERVICEONLY_START));
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_STOP, LoadLocalizedString(IDS_MENU_SERVICEONLY_STOP));
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_RESTART, LoadLocalizedString(IDS_MENU_SERVICEONLY_RESTART));
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        }

        AppendMenu(hMenu, MF_STRING, IDM_VIEWLOGMENU, LoadLocalizedString(IDS_MENU_VIEWLOG));

        AppendMenu(hMenu, MF_STRING, IDM_EDITMENU, LoadLocalizedString(IDS_MENU_EDITCONFIG));
        AppendMenu(hMenu, MF_STRING, IDM_CLEARPASSMENU, LoadLocalizedString(IDS_MENU_CLEARPASS));

#ifndef DISABLE_CHANGE_PASSWORD
        if (o.conn[0].flags & FLAG_ALLOW_CHANGE_PASSPHRASE)
            AppendMenu(hMenu, MF_STRING, IDM_PASSPHRASEMENU, LoadLocalizedString(IDS_MENU_PASSPHRASE));
#endif

        AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

        AppendMenu(hMenu, MF_STRING, IDM_IMPORT, LoadLocalizedString(IDS_MENU_IMPORT));
        AppendMenu(hMenu, MF_STRING ,IDM_SETTINGS, LoadLocalizedString(IDS_MENU_SETTINGS));
        AppendMenu(hMenu, MF_STRING ,IDM_CLOSE, LoadLocalizedString(IDS_MENU_CLOSE));

        SetMenuStatusById(0,  o.conn[0].state);
    }
    else {
        /* Create Main menu with all connections */
        int i;
        for (i = 0; i < o.num_configs; i++)
        {
            connection_t *c = &o.conn[i];
            group_menu_t *current = &groupMenu[0]; /* the root menu by default */

            if (USE_GROUPED_CONFIG_MENU)
            {
                if (c->group_id >= 0 && c->group_id < o.num_groups) /* should be always true */
                    current = &groupMenu[c->group_id];
            }

            /* Add config to the current sub menu */
            AppendMenu(current->h, MF_POPUP, (UINT_PTR) hMenuConn[i], o.conn[i].config_name);
            configPos[i] = current->entries++;
            //SetMenuImage(current->h, configPos[i]);
            PrintDebug(L"Config %d name %s in group %d with position = %d group_id = %d",
                        i, o.conn[i].config_name, c->group_id, configPos[i], o.conn[i].group_id);
        }

        /* construct the submenu tree */
        if (USE_GROUPED_CONFIG_MENU)
        {
            /* i = 0 is the root menu and has no parent */
            for (i = 1; i < o.num_groups; i++)
            {
                if (!o.groups[i].active)
                    continue;
                int parent = o.groups[i].parent_id;
                if (parent < 0 || parent > o.num_groups) /* should not happen */
                    continue;
                AppendMenu(groupMenu[parent].h, MF_POPUP, (UINT_PTR) groupMenu[i].h, o.groups[i].name);
                groupMenu[i].pos = groupMenu[parent].entries++;
                PrintDebug(L"Group %d name %s added to Parent %d with position = %d",
                        i, o.groups[i].name, parent, groupMenu[i].pos);
            }
        }

        if (o.num_configs > 0)
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

        if (o.service_only) {
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_START, LoadLocalizedString(IDS_MENU_SERVICEONLY_START));
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_STOP, LoadLocalizedString(IDS_MENU_SERVICEONLY_STOP));
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_RESTART, LoadLocalizedString(IDS_MENU_SERVICEONLY_RESTART));
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        }

        AppendMenu(hMenu, MF_STRING, IDM_IMPORT, LoadLocalizedString(IDS_MENU_IMPORT));
        AppendMenu(hMenu, MF_STRING, IDM_SETTINGS, LoadLocalizedString(IDS_MENU_SETTINGS));
        AppendMenu(hMenu, MF_STRING, IDM_CLOSE, LoadLocalizedString(IDS_MENU_CLOSE));


        /* Create popup menus for every connection */
        for (i=0; i < o.num_configs; i++) {
            if (o.service_only == 0) {
                AppendMenu(hMenuConn[i], MF_STRING, IDM_CONNECTMENU + i, LoadLocalizedString(IDS_MENU_CONNECT));
                AppendMenu(hMenuConn[i], MF_STRING, IDM_DISCONNECTMENU + i, LoadLocalizedString(IDS_MENU_DISCONNECT));
                AppendMenu(hMenuConn[i], MF_STRING, IDM_RECONNECTMENU + i, LoadLocalizedString(IDS_MENU_RECONNECT));
                AppendMenu(hMenuConn[i], MF_STRING, IDM_STATUSMENU + i, LoadLocalizedString(IDS_MENU_STATUS));
                AppendMenu(hMenuConn[i], MF_SEPARATOR, 0, 0);
            }

            AppendMenu(hMenuConn[i], MF_STRING, IDM_VIEWLOGMENU + i, LoadLocalizedString(IDS_MENU_VIEWLOG));

            AppendMenu(hMenuConn[i], MF_STRING, IDM_EDITMENU + i, LoadLocalizedString(IDS_MENU_EDITCONFIG));
            AppendMenu(hMenuConn[i], MF_STRING, IDM_CLEARPASSMENU + i, LoadLocalizedString(IDS_MENU_CLEARPASS));

#ifndef DISABLE_CHANGE_PASSWORD
            if (o.conn[i].flags & FLAG_ALLOW_CHANGE_PASSPHRASE)
                AppendMenu(hMenuConn[i], MF_STRING, IDM_PASSPHRASEMENU + i, LoadLocalizedString(IDS_MENU_PASSPHRASE));
#endif

            SetMenuStatusById(i, o.conn[i].state);
        }
    }

    SetServiceMenuStatus();
}


/* Destroy popup menus */
static void
DestroyPopupMenus()
{
    DestroyMenu(hMenuService);
    DestroyMenu(hMenu); /* this destroys all sub-menus */
    hMenuService = NULL;
    hMenu = NULL;
}


/*
 * Handle mouse clicks on tray icon
 */
void
OnNotifyTray(LPARAM lParam)
{
    POINT pt;

    switch (lParam) {
    case WM_RBUTTONUP:
        /* Recreate popup menus */
        DestroyPopupMenus();
        BuildFileList();
        CreatePopupMenus();

        GetCursorPos(&pt);
        SetForegroundWindow(o.hWnd);
        TrackPopupMenu(hMenu, TPM_RIGHTALIGN, pt.x, pt.y, 0, o.hWnd, NULL);
        PostMessage(o.hWnd, WM_NULL, 0, 0);
        break;

    case WM_LBUTTONDBLCLK:
        if (o.service_only) {
            /* Start or stop OpenVPN service */
            if (o.service_state == service_disconnected) {
                MyStartService();
            }
            else if (o.service_state == service_connected
            && ShowLocalizedMsgEx(MB_YESNO, _T(PACKAGE_NAME), IDS_MENU_ASK_STOP_SERVICE) == IDYES) {
                MyStopService();
            }
        }
        else {
            int disconnected_conns = CountConnState(disconnected);

            DestroyPopupMenus();
            BuildFileList();
            CreatePopupMenus();

            /* Start connection if only one config exist */
            if (o.num_configs == 1 && o.conn[0].state == disconnected)
                    StartOpenVPN(&o.conn[0]);
            else if (disconnected_conns == o.num_configs - 1) {
                /* Show status window if only one connection is running */
                int i;
                for (i = 0; i < o.num_configs; i++) {
                    if (o.conn[i].state != disconnected) {
                        ShowWindow(o.conn[i].hwndStatus, SW_SHOW);
                        SetForegroundWindow(o.conn[i].hwndStatus);
                        break;
                    }
                }
            }
        }
        break;
    }
}


void
OnDestroyTray()
{
    DestroyMenu(hMenu);
    hMenu = NULL;
    Shell_NotifyIcon(NIM_DELETE, &ni);
}


void
ShowTrayIcon()
{
  ni.cbSize = sizeof(ni);
  ni.uID = 0;
  ni.hWnd = o.hWnd;
  ni.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
  ni.uCallbackMessage = WM_NOTIFYICONTRAY;
  ni.hIcon = LoadLocalizedSmallIcon(ID_ICO_DISCONNECTED);
  _tcsncpy(ni.szTip, LoadLocalizedString(IDS_TIP_DEFAULT), _countof(ni.szTip));

  Shell_NotifyIcon(NIM_ADD, &ni);
}

void
SetTrayIcon(conn_state_t state)
{
    TCHAR msg[500];
    TCHAR msg_connected[100];
    TCHAR msg_connecting[100];
    int i, config = 0;
    BOOL first_conn;
    UINT icon_id;

    _tcsncpy(msg, LoadLocalizedString(IDS_TIP_DEFAULT), _countof(ni.szTip));
    _tcsncpy(msg_connected, LoadLocalizedString(IDS_TIP_CONNECTED), _countof(msg_connected));
    _tcsncpy(msg_connecting, LoadLocalizedString(IDS_TIP_CONNECTING), _countof(msg_connecting));

    first_conn = TRUE;
    for (i = 0; i < o.num_configs; i++) {
        if (o.conn[i].state == connected) {
            /* Append connection name to Icon Tip Msg */
            _tcsncat(msg, (first_conn ? msg_connected : _T(", ")), _countof(msg) - _tcslen(msg) - 1);
            _tcsncat(msg, o.conn[i].config_name, _countof(msg) - _tcslen(msg) - 1);
            first_conn = FALSE;
            config = i;
        }
    }

    first_conn = TRUE;
    for (i = 0; i < o.num_configs; i++) {
        if (o.conn[i].state == connecting || o.conn[i].state == resuming || o.conn[i].state == reconnecting) {
            /* Append connection name to Icon Tip Msg */
            _tcsncat(msg, (first_conn ? msg_connecting : _T(", ")), _countof(msg) - _tcslen(msg) - 1);
            _tcsncat(msg, o.conn[i].config_name, _countof(msg) - _tcslen(msg) - 1);
            first_conn = FALSE;
        }
    }

    if (CountConnState(connected) == 1) {
        /* Append "Connected since and assigned IP" to message */
        const connection_t *c = &o.conn[config];
        TCHAR time[50];

        LocalizedTime(o.conn[config].connected_since, time, _countof(time));
        _tcsncat(msg, LoadLocalizedString(IDS_TIP_CONNECTED_SINCE), _countof(msg) - _tcslen(msg) - 1);
        _tcsncat(msg, time, _countof(msg) - _tcslen(msg) - 1);

        if ( _tcslen(c->ip) > 0) {
            /* concatenate ipv4 and ipv6 addresses into one string */
            WCHAR ip[64];
            wcs_concat2(ip, _countof(ip), c->ip, c->ipv6, L", ");
            WCHAR *assigned_ip = LoadLocalizedString(IDS_TIP_ASSIGNED_IP, ip);
            _tcsncat(msg, assigned_ip, _countof(msg) - _tcslen(msg) - 1);
        }
    }

    icon_id = ID_ICO_CONNECTING;
    if (state == connected)
        icon_id = ID_ICO_CONNECTED;
    else if (state == disconnected)
        icon_id = ID_ICO_DISCONNECTED;

    ni.cbSize = sizeof(ni);
    ni.uID = 0;
    ni.hWnd = o.hWnd;
    ni.hIcon = LoadLocalizedSmallIcon(icon_id);
    ni.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
    ni.uCallbackMessage = WM_NOTIFYICONTRAY;
    _tcsncpy(ni.szTip, msg, _countof(ni.szTip));

    Shell_NotifyIcon(NIM_MODIFY, &ni);
}


void
CheckAndSetTrayIcon()
{
    if (o.service_state == service_connected)
    {
        SetTrayIcon(connected);
        return;
    }

    if (CountConnState(connected) != 0)
    {
        SetTrayIcon(connected);
    }
    else
    {
        if (CountConnState(connecting) != 0 || CountConnState(reconnecting) != 0
        ||  CountConnState(resuming) != 0 || o.service_state == service_connecting)
            SetTrayIcon(connecting);
        else
            SetTrayIcon(disconnected);
    }
}


void
ShowTrayBalloon(TCHAR *infotitle_msg, TCHAR *info_msg)
{
    ni.cbSize = sizeof(ni);
    ni.uID = 0;
    ni.hWnd = o.hWnd;
    ni.uFlags = NIF_INFO;
    ni.uTimeout = 5000;
    ni.dwInfoFlags = NIIF_INFO;
    _tcsncpy(ni.szInfo, info_msg, _countof(ni.szInfo));
    _tcsncpy(ni.szInfoTitle, infotitle_msg, _countof(ni.szInfoTitle));

    Shell_NotifyIcon(NIM_MODIFY, &ni);
}


void
SetMenuStatus(connection_t *c, conn_state_t state)
{
    int i;
    for (i = 0; i < o.num_configs; ++i)
    {
        if (c == &o.conn[i])
        {
            SetMenuStatusById(i, state);
            break;
        }
    }
}

void
SetMenuStatusById(int i, conn_state_t state)
{
    if (i < 0 || i >= o.num_configs)
        return;

    connection_t *c = &o.conn[i];
    BOOL checked = (state == connected || state == disconnecting);


    if (o.num_configs == 1)
    {
        CheckMenuItem(hMenu, 0, MF_BYPOSITION | (checked ? MF_CHECKED : MF_UNCHECKED));
        if (state == disconnected)
        {
            EnableMenuItem(hMenu, IDM_CONNECTMENU, MF_ENABLED);
            EnableMenuItem(hMenu, IDM_DISCONNECTMENU, MF_GRAYED);
            EnableMenuItem(hMenu, IDM_RECONNECTMENU, MF_GRAYED);
            EnableMenuItem(hMenu, IDM_STATUSMENU, MF_GRAYED);
        }
        else if (state == connecting || state == resuming || state == connected)
        {
            EnableMenuItem(hMenu, IDM_CONNECTMENU, MF_GRAYED);
            EnableMenuItem(hMenu, IDM_DISCONNECTMENU, MF_ENABLED);
            EnableMenuItem(hMenu, IDM_RECONNECTMENU, MF_ENABLED);
            EnableMenuItem(hMenu, IDM_STATUSMENU, MF_ENABLED);
        }
        else if (state == disconnecting)
        {
            EnableMenuItem(hMenu, IDM_CONNECTMENU, MF_GRAYED);
            EnableMenuItem(hMenu, IDM_DISCONNECTMENU, MF_GRAYED);
            EnableMenuItem(hMenu, IDM_RECONNECTMENU, MF_GRAYED);
            EnableMenuItem(hMenu, IDM_STATUSMENU, MF_ENABLED);
        }
        if (c->flags & (FLAG_SAVE_AUTH_PASS | FLAG_SAVE_KEY_PASS))
            EnableMenuItem(hMenu, IDM_CLEARPASSMENU, MF_ENABLED);
        else
            EnableMenuItem(hMenu, IDM_CLEARPASSMENU, MF_GRAYED);
    }
    else
    {
        int group_id = 0;
        int pos = configPos[i];

        if (USE_GROUPED_CONFIG_MENU)
            group_id = c->group_id;

        CheckMenuItem(groupMenu[group_id].h, pos, MF_BYPOSITION | (checked ? MF_CHECKED : MF_UNCHECKED));

        PrintDebug(L"setting state of config %s with group_name %s checked = %d, parent %s, pos %d",
                    c->config_name, o.groups[group_id].name, checked, (group_id == 0)? L"Main Menu" : L"SubMenu", pos);

        if (checked) /* also check all parent groups */
        {
            while (group_id > 0)
            {
                pos = groupMenu[group_id].pos;
                group_id = o.groups[group_id].parent_id;
                PrintDebug(L"checking sub menu with id %d pos %d", group_id, pos);
                CheckMenuItem(groupMenu[group_id].h, pos, MF_BYPOSITION | MF_CHECKED);
            }
        }


        if (state == disconnected)
        {
            EnableMenuItem(hMenuConn[i], IDM_CONNECTMENU + i, MF_ENABLED);
            EnableMenuItem(hMenuConn[i], IDM_DISCONNECTMENU + i, MF_GRAYED);
            EnableMenuItem(hMenuConn[i], IDM_RECONNECTMENU + i, MF_GRAYED);
            EnableMenuItem(hMenuConn[i], IDM_STATUSMENU + i, MF_GRAYED);
        }
        else if (state == connecting || state == resuming || state == connected)
        {
            EnableMenuItem(hMenuConn[i], IDM_CONNECTMENU + i, MF_GRAYED);
            EnableMenuItem(hMenuConn[i], IDM_DISCONNECTMENU + i, MF_ENABLED);
            EnableMenuItem(hMenuConn[i], IDM_RECONNECTMENU + i, MF_ENABLED);
            EnableMenuItem(hMenuConn[i], IDM_STATUSMENU + i, MF_ENABLED);
        }
        else if (state == disconnecting)
        {
            EnableMenuItem(hMenuConn[i], IDM_CONNECTMENU + i, MF_GRAYED);
            EnableMenuItem(hMenuConn[i], IDM_DISCONNECTMENU + i, MF_GRAYED);
            EnableMenuItem(hMenuConn[i], IDM_RECONNECTMENU + i, MF_GRAYED);
            EnableMenuItem(hMenuConn[i], IDM_STATUSMENU + i, MF_ENABLED);
        }
        if (c->flags & (FLAG_SAVE_AUTH_PASS | FLAG_SAVE_KEY_PASS))
            EnableMenuItem(hMenuConn[i], IDM_CLEARPASSMENU + i, MF_ENABLED);
        else
            EnableMenuItem(hMenuConn[i], IDM_CLEARPASSMENU + i, MF_GRAYED);
    }
}


void
SetServiceMenuStatus()
{
    HMENU hMenuHandle;

    if (o.service_only == 0)
        return;

    if (o.service_only)
        hMenuHandle = hMenu;
    else
        hMenuHandle = hMenuService;

    if (o.service_state == service_noaccess
    ||  o.service_state == service_connecting) {
        EnableMenuItem(hMenuHandle, IDM_SERVICE_START, MF_GRAYED);
        EnableMenuItem(hMenuHandle, IDM_SERVICE_STOP, MF_GRAYED);
        EnableMenuItem(hMenuHandle, IDM_SERVICE_RESTART, MF_GRAYED);
    }
    else if (o.service_state == service_connected) {
        EnableMenuItem(hMenuHandle, IDM_SERVICE_START, MF_GRAYED);
        EnableMenuItem(hMenuHandle, IDM_SERVICE_STOP, MF_ENABLED);
        EnableMenuItem(hMenuHandle, IDM_SERVICE_RESTART, MF_ENABLED);
    }
    else {
        EnableMenuItem(hMenuHandle, IDM_SERVICE_START, MF_ENABLED);
        EnableMenuItem(hMenuHandle, IDM_SERVICE_STOP, MF_GRAYED);
        EnableMenuItem(hMenuHandle, IDM_SERVICE_RESTART, MF_GRAYED);
    }
}
