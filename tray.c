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

/* Popup Menus */
HMENU hMenu;
HMENU hMenuService;

NOTIFYICONDATA ni;
extern options_t o;


/* Create popup menus */
void
CreatePopupMenus()
{
    connection_t *c;

    for (c = o.conn; c; c = c->next)
    {
        if (!c->hMenuConn)
            c->hMenuConn = CreatePopupMenu();
    }

    hMenuService = CreatePopupMenu();
    hMenu = CreatePopupMenu();

    if (o.num_configs == 1) {
        /* Create Main menu with actions */
        int index = o.conn->index;
        if (o.service_only[0] == '0') {
            AppendMenu(hMenu, MF_STRING, MAKEWPARAM(IDM_CONNECTMENU, index), LoadLocalizedString(IDS_MENU_CONNECT));
            AppendMenu(hMenu, MF_STRING, MAKEWPARAM(IDM_DISCONNECTMENU, index), LoadLocalizedString(IDS_MENU_DISCONNECT));
            AppendMenu(hMenu, MF_STRING, MAKEWPARAM(IDM_STATUSMENU, index), LoadLocalizedString(IDS_MENU_STATUS));
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        }
        else {
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_START, LoadLocalizedString(IDS_MENU_SERVICEONLY_START));
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_STOP, LoadLocalizedString(IDS_MENU_SERVICEONLY_STOP));
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_RESTART, LoadLocalizedString(IDS_MENU_SERVICEONLY_RESTART));
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        }

        AppendMenu(hMenu, MF_STRING, MAKEWPARAM(IDM_VIEWLOGMENU, index), LoadLocalizedString(IDS_MENU_VIEWLOG));

        if (o.allow_edit[0] == '1')
            AppendMenu(hMenu, MF_STRING, MAKEWPARAM(IDM_EDITMENU, index), LoadLocalizedString(IDS_MENU_EDITCONFIG));

#ifndef DISABLE_CHANGE_PASSWORD
        if (o.allow_password[0] == '1')
            AppendMenu(hMenu, MF_STRING, MAKEWPARAM(IDM_PASSPHRASEMENU, index), LoadLocalizedString(IDS_MENU_PASSPHRASE));
#endif

        AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

        if (o.allow_service[0] == '1' && o.service_only[0] == '0')
        {
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR) hMenuService, LoadLocalizedString(IDS_MENU_SERVICE));
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        }

        AppendMenu(hMenu, MF_STRING, IDM_IMPORT, LoadLocalizedString(IDS_MENU_IMPORT));
        AppendMenu(hMenu, MF_STRING, IDM_SETTINGS, LoadLocalizedString(IDS_MENU_SETTINGS));
        AppendMenu(hMenu, MF_STRING, IDM_CLOSE, LoadLocalizedString(IDS_MENU_CLOSE));

        SetMenuStatus(o.conn,  o.conn->state);
    }
    else {
        /* Create Main menu with all connections */
        for (c = o.conn; c ; c = c->next)
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR) c->hMenuConn, c->config_name);

        if (o.num_configs > 0)
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

        if (o.service_only[0] == '0' && o.allow_service[0] == '1') {
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR) hMenuService, LoadLocalizedString(IDS_MENU_SERVICE));
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        }
        else if (o.service_only[0] == '1') {
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_START, LoadLocalizedString(IDS_MENU_SERVICEONLY_START));
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_STOP, LoadLocalizedString(IDS_MENU_SERVICEONLY_STOP));
            AppendMenu(hMenu, MF_STRING, IDM_SERVICE_RESTART, LoadLocalizedString(IDS_MENU_SERVICEONLY_RESTART));
            AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        }

        AppendMenu(hMenu, MF_STRING, IDM_IMPORT, LoadLocalizedString(IDS_MENU_IMPORT));
        AppendMenu(hMenu, MF_STRING, IDM_SETTINGS, LoadLocalizedString(IDS_MENU_SETTINGS));
        AppendMenu(hMenu, MF_STRING, IDM_CLOSE, LoadLocalizedString(IDS_MENU_CLOSE));


        /* Create popup menus for every connection */
        for (c = o.conn; c; c = c->next) {
            int index = c->index;

            if (o.service_only[0] == '0') {
                AppendMenu(c->hMenuConn, MF_STRING, MAKEWPARAM(IDM_CONNECTMENU,index), LoadLocalizedString(IDS_MENU_CONNECT));
                AppendMenu(c->hMenuConn, MF_STRING, MAKEWPARAM(IDM_DISCONNECTMENU,index), LoadLocalizedString(IDS_MENU_DISCONNECT));
                AppendMenu(c->hMenuConn, MF_STRING, MAKEWPARAM(IDM_STATUSMENU,index), LoadLocalizedString(IDS_MENU_STATUS));
                AppendMenu(c->hMenuConn, MF_SEPARATOR, 0, 0);
            }

            AppendMenu(c->hMenuConn, MF_STRING, MAKEWPARAM(IDM_VIEWLOGMENU, index), LoadLocalizedString(IDS_MENU_VIEWLOG));

            if (o.allow_edit[0] == '1')
                AppendMenu(c->hMenuConn, MF_STRING, MAKEWPARAM(IDM_EDITMENU, index), LoadLocalizedString(IDS_MENU_EDITCONFIG));

#ifndef DISABLE_CHANGE_PASSWORD
            if (o.allow_password[0] == '1')
                AppendMenu(c->hMenuConn, MF_STRING, MAKEWPARAM(IDM_PASSPHRASEMENU, index), LoadLocalizedString(IDS_MENU_PASSPHRASE));
#endif
            SetMenuStatus(c, c->state);
        }
    }

    /* Create service menu */
    if (o.allow_service[0] == '1' && o.service_only[0] == '0')
    {
        AppendMenu(hMenuService, MF_STRING, IDM_SERVICE_START, LoadLocalizedString(IDS_MENU_SERVICE_START));
        AppendMenu(hMenuService, MF_STRING, IDM_SERVICE_STOP, LoadLocalizedString(IDS_MENU_SERVICE_STOP));
        AppendMenu(hMenuService, MF_STRING, IDM_SERVICE_RESTART, LoadLocalizedString(IDS_MENU_SERVICE_RESTART));
    }

    SetServiceMenuStatus();
}


/* Destroy popup menus */
static void
DestroyPopupMenus()
{
    connection_t *c;
    for (c = o.conn; c; c = c->next)
    {
        DestroyMenu(c->hMenuConn);
        c->hMenuConn = NULL;
    }

    DestroyMenu(hMenuService);
    DestroyMenu(hMenu);
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
        if (o.service_only[0] == '1') {
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
            int disconnected_conns;

            DestroyPopupMenus();
            BuildFileList();
            CreatePopupMenus();

            /* Start connection if only one config exist */
            if (o.num_configs == 1 && o.conn->state == disconnected)
                    StartOpenVPN(o.conn);
            disconnected_conns = CountConnState(disconnected);
            if (disconnected_conns == o.num_configs - 1) {
                /* Show status window if only one connection is running */
                connection_t *c;
                for (c = o.conn; c;  c = c->next) {
                    if (c->state != disconnected) {
                        ShowWindow(c->hwndStatus, SW_SHOW);
                        SetForegroundWindow(c->hwndStatus);
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
  ni.hIcon = LoadLocalizedIcon(ID_ICO_DISCONNECTED);
  _tcsncpy(ni.szTip, LoadLocalizedString(IDS_TIP_DEFAULT), _countof(ni.szTip));

  Shell_NotifyIcon(NIM_ADD, &ni);
}

void
SetTrayIcon(conn_state_t state)
{
    TCHAR msg[500];
    TCHAR msg_connected[100];
    TCHAR msg_connecting[100];
    BOOL first_conn;
    UINT icon_id;
    connection_t *c, *c0;

    _tcsncpy(msg, LoadLocalizedString(IDS_TIP_DEFAULT), _countof(ni.szTip));
    _tcsncpy(msg_connected, LoadLocalizedString(IDS_TIP_CONNECTED), _countof(msg_connected));
    _tcsncpy(msg_connecting, LoadLocalizedString(IDS_TIP_CONNECTING), _countof(msg_connecting));

    first_conn = TRUE;
    c0 = o.conn;
    for (c = o.conn; c; c = c->next) {
        if (c->state == connected) {
            /* Append connection name to Icon Tip Msg */
            _tcsncat(msg, (first_conn ? msg_connected : _T(", ")), _countof(msg) - _tcslen(msg) - 1);
            _tcsncat(msg, c->config_name, _countof(msg) - _tcslen(msg) - 1);
            first_conn = FALSE;
            c0 = c;
        }
    }

    first_conn = TRUE;
    for (c = o.conn; c; c = c->next) {
        if (c->state == connecting || c->state == resuming || c->state == reconnecting) {
            /* Append connection name to Icon Tip Msg */
            _tcsncat(msg, (first_conn ? msg_connecting : _T(", ")), _countof(msg) - _tcslen(msg) - 1);
            _tcsncat(msg, c->config_name, _countof(msg) - _tcslen(msg) - 1);
            first_conn = FALSE;
        }
    }

    if (CountConnState(connected) == 1) {
        /* Append "Connected since and assigned IP" to message */
        TCHAR time[50];

        LocalizedTime(c0->connected_since, time, _countof(time));
        _tcsncat(msg, LoadLocalizedString(IDS_TIP_CONNECTED_SINCE), _countof(msg) - _tcslen(msg) - 1);
        _tcsncat(msg, time, _countof(msg) - _tcslen(msg) - 1);

        if (_tcslen(c0->ip) > 0) {
            TCHAR *assigned_ip = LoadLocalizedString(IDS_TIP_ASSIGNED_IP, c0->ip);
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
    ni.hIcon = LoadLocalizedIcon(icon_id);
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
        if (CountConnState(connecting) != 0 || CountConnState(reconnecting) != 0 ||
            CountConnState(resuming) != 0 || o.service_state == service_connecting)
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
    if (!c) return; /* BUG(): should not happen */
    if (o.num_configs == 1)
    {
        int index = o.conn->index;
        if (state == disconnected)
        {
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_CONNECTMENU ,index), MF_ENABLED);
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_DISCONNECTMENU ,index), MF_GRAYED);
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_STATUSMENU ,index), MF_GRAYED);
        }
        else if (state == onhold)
        {
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_CONNECTMENU ,index), MF_ENABLED);
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_DISCONNECTMENU ,index), MF_GRAYED);
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_STATUSMENU ,index), MF_ENABLED);
        }
        else if (state == connecting || state == resuming || state == connected)
        {
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_CONNECTMENU ,index), MF_GRAYED);
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_DISCONNECTMENU ,index), MF_ENABLED);
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_STATUSMENU ,index), MF_ENABLED);
        }
        else if (state == disconnecting)
        {
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_CONNECTMENU ,index), MF_GRAYED);
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_DISCONNECTMENU ,index), MF_GRAYED);
            EnableMenuItem(hMenu, MAKEWPARAM(IDM_STATUSMENU ,index), MF_ENABLED);
        }
    }
    else
    {
        int i, index = c->index;
        connection_t *c1;

        for (c1 = o.conn, i = 0; c1; c1 = c1->next, ++i)
            if (c1 == c) break;

        BOOL checked = (state == connected || state == disconnecting);
        CheckMenuItem(hMenu, i, MF_BYPOSITION | (checked ? MF_CHECKED : MF_UNCHECKED));

        if (state == disconnected)
        {
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_CONNECTMENU, index), MF_ENABLED);
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_DISCONNECTMENU, index), MF_GRAYED);
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_STATUSMENU, index), MF_GRAYED);
        }
        else if (state == onhold)
        {
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_CONNECTMENU, index), MF_ENABLED);
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_DISCONNECTMENU, index), MF_GRAYED);
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_STATUSMENU, index), MF_ENABLED);
        }
        else if (state == connecting || state == resuming || state == connected)
        {
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_CONNECTMENU, index), MF_GRAYED);
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_DISCONNECTMENU, index), MF_ENABLED);
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_STATUSMENU, index), MF_ENABLED);
        }
        else if (state == disconnecting)
        {
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_CONNECTMENU, index), MF_GRAYED);
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_DISCONNECTMENU, index), MF_GRAYED);
            EnableMenuItem(c->hMenuConn, MAKEWPARAM(IDM_STATUSMENU, index), MF_ENABLED);
        }
    }
}


void
SetServiceMenuStatus()
{
    HMENU hMenuHandle;

    if (o.allow_service[0] == '0' && o.service_only[0] == '0')
        return;

    if (o.service_only[0] == '1')
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
