/*
 *  OpenVPN-GUI -- A Windows GUI for OpenVPN.
 *
 *  Copyright (C) 2021-2022 Selva Nair <selva.nair@gmail.com>
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

#include <windows.h>
#include "main.h"
#include "options.h"
#include "manage.h"
#include "openvpn.h"
#include "misc.h"
#include "openvpn-gui-res.h"
#include "localization.h"
#include "commctrl.h"

extern options_t o;

#define MIN_REQ_MGMT_VERSION 4
#define MIN(a,b) ((a) < (b) ? (a) : (b))

/* state of list array */
#define STATE_GET_COUNT 1
#define STATE_GET_ENTRY 2
#define STATE_FILLED    4
#define STATE_SELECTED  8

struct remote_entry
{
    wchar_t *host;
    wchar_t *port;
    wchar_t *proto;
    char *data;    /* remote entry string value as received from daemon */
};

/* forward declarations */
static bool remote_entry_parse(const char *data, struct remote_entry *re);
static UINT remote_entry_find(struct remote_list *rl, const char *str, UINT start);
static INT_PTR CALLBACK QueryRemoteDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static void CALLBACK remote_listview_fill(HWND hwnd, UINT UNUSED msg, UINT_PTR id, DWORD UNUSED now);
static HWND remote_listview_init(HWND parent);

/* Accept current remote entry */
static void
remote_accept(connection_t *c)
{
    ManagementCommand(c, "remote ACCEPT", NULL, regular);
}

/* Skip multiple remotes entries */
static void
remote_skip(connection_t *c, UINT toskip)
{
    char cmd[sizeof("remote SKIP %u") + 10];

    _snprintf_0(cmd, "remote SKIP %u", toskip);
    ManagementCommand(c, cmd, NULL, regular);
}

static bool
remote_list_alloc(connection_t *c)
{
    struct remote_list *rl = &c->remote_list;
    if (rl->count > 0 && !rl->re)
    {
        rl->re = calloc(rl->count, sizeof(remote_entry_t));
    }
    return (rl->re != NULL);
}

static void
remote_entry_free(remote_entry_t *re)
{
    free(re->host);
    free(re->port);
    free(re->proto);
    free(re->data);
}

/* Free any allocated memory and clear the remote list */
void
remote_list_clear(struct remote_list *rl)
{
    if (rl->re)
    {
        for(UINT i = 0; i < rl->count; i++)
        {
            remote_entry_free(&rl->re[i]);
        }
        free(rl->re);
    }

    CLEAR(*rl);
}

/* Handle remote-query from management
 * Format: host,port,protocol
 */
void
OnRemote(connection_t *c, char *msg)
{
    struct remote_list *rl = &c->remote_list;

    /* if not supported, just accept the remote and move on */
    if (c->management_version < MIN_REQ_MGMT_VERSION)
    {
        WriteStatusLog(c, L"GUI> ", L"Querying remote failed. Accepting proposed value.", false);
        remote_accept(c);
        return;
    }

    /* Dynamic CR causes remote to be prompted again -- just accept the current value */
    if (c->dynamic_cr)
    {
        remote_accept(c);
        return;
    }
    else if (!(rl->state & STATE_SELECTED)) /* no valid selection */
    {
        /* cache the proposed remote, and query user */
        strncpy_s(rl->current, sizeof(rl->current), msg, _TRUNCATE);
        rl->selected = (UINT) -1; /* set selection to an invalid index */

        if (IDOK != LocalizedDialogBoxParam(ID_DLG_REMOTE_QUERY, QueryRemoteDialogProc, (LPARAM)c))
        {
            return;
        }
    }

    UINT n = rl->count;
    UINT i = rl->selected;
    if (!(rl->state & STATE_SELECTED)
        || i > n || strcmp(rl->re[i].data, msg) == 0)
    {
        /* invalid selection or proposed value matches selection  -- accept */
        remote_accept(c);
        rl->state &= ~STATE_SELECTED;
    }
    else
    {
        UINT toskip = (UINT) -1;
        UINT current = remote_entry_find(rl, msg, 0);
        /* if there are duplicates, we are not sure of the current
         * position in the daemon. Choose the smallest possible value
         * for skip. This may take a few round trips to get to the
         * selected point.
         * TODO: have >REMOTE extended to include the index.
         */
        while (current < n)
        {
            toskip = MIN(toskip, (i + n - current) % n);
            current = remote_entry_find(rl, msg, current+1);
        }
        remote_skip(c, toskip < n ? toskip : 1);
    }
}

/* Callback when remote entry count is received */
static void
remote_count_recv(connection_t *c, char *msg)
{
    if (msg)
    {
        c->remote_list.count = strtoul(msg, NULL, 10);
    }
    else
    {
        c->remote_list.state &= ~STATE_GET_COUNT;
    }
}

/* Callback for receiving remote entry from daemon.
 * Expect msg = i,host,port,proto
 */
static void
remote_entry_recv(connection_t *c, char *msg)
{
    struct remote_list *rl = &c->remote_list;
    char *p = NULL;

    if (!msg)
    {
        return;
    }
    if (!rl->re
        && !remote_list_alloc(c))
    {
        WriteStatusLog(c, L"GUI> ", L"Out of memory for remote entry list", false);
        return;
    }

    UINT i = strtoul(msg, &p, 10);

    if (!p || *p != ',' || i >= rl->count)
    {
        WriteStatusLog(c, L"GUI> ", L"Invalid remote entry ignored.", false);
        return;
    }

    /* load remote data into re[i] */
    if (!remote_entry_parse(++p, &rl->re[i]))
    {
        WriteStatusLog(c, L"GUI> ", L"Failed to parse remote entry. Ignored.", false);
        return;
    }

    if (i + 1 == rl->count) /* done */
    {
        rl->state |= STATE_FILLED;
    }
}

/* Comparison function for sorting enrties in remote list.
 * i and j are the original indices of the list and
 * we return (i - j) if rl->show_sorted is false.
 * This may be used to restore the original ordering.
 * Otherwise the ordering is that of the hostname in the
 * entry.
 */
static int
remote_entry_compare(UINT i, UINT j, const struct remote_list *rl)
{
    int res = (i - j);

    if (rl->show_sorted && i < rl->count && j < rl->count)
    {
       res = wcscmp(rl->re[i].host, rl->re[j].host);
    }

    return res;
}

/*
 *  Populate the remote list by querying the daemon
 *
 *  The requests are queued and completed asyncronously.
 *  We return immediately if already in progress or
 *  not yet ready to populate. Call repeatedly until
 *  state & STATE_GET_ENTRY evaluates to 1.
 *
 *  To recreate the list afresh, call remote_list_clear(c) first.
 *
 *  Functions that consume the list should check
 *  state & STATE_FILLED
 */
static void
remote_list_update(connection_t *c)
{
    struct remote_list *rl = &c->remote_list;

    if (c->management_version < MIN_REQ_MGMT_VERSION)
    {
        return;
    }
    else if (rl->count == 0 && (rl->state & STATE_GET_COUNT) == 0)
    {
        ManagementCommand(c, "remote-entry-count", remote_count_recv, regular);
        c->remote_list.state |= STATE_GET_COUNT;
    }
    else if ((rl->state & STATE_GET_ENTRY) == 0)
    {
        ManagementCommand(c, "remote-entry-get all", remote_entry_recv, regular);
        c->remote_list.state |= STATE_GET_ENTRY;
    }
}

/* Dialog proc for querying remote */
static INT_PTR CALLBACK
QueryRemoteDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    connection_t *c;

    switch (msg)
    {
    case WM_INITDIALOG:
        c = (connection_t *) lParam;
        SetProp(hwndDlg, cfgProp, (HANDLE)lParam);
        SetStatusWinIcon(hwndDlg, ID_ICO_APP);

        if (remote_listview_init(hwndDlg))
        {
            /* Delay the first call to listvew_fill to after
             * we return from here.
             */
            SetTimer(hwndDlg, 0, 100, remote_listview_fill);
        }
        else
        {
            WriteStatusLog(c, L"GUI> ", L"Error initializing remote selection dialog.", false);
            EndDialog(hwndDlg, wParam);
        }
        return TRUE;

    case WM_COMMAND:
        c = (connection_t *) GetProp(hwndDlg, cfgProp);
        if (LOWORD(wParam) == IDOK)
        {
            HWND lv = GetDlgItem(hwndDlg, ID_LVW_REMOTE);
            int id = (int) ListView_GetNextItem(lv, -1, LVNI_ALL|LVNI_SELECTED);
            LVITEM lvi = {.iItem=id, .mask=LVIF_PARAM};
            if (id >= 0 && ListView_GetItem(lv, &lvi))
            {
                c->remote_list.selected = (UINT) lvi.lParam;
                c->remote_list.state |= STATE_SELECTED;
            }
            PrintDebug(L"QueryRemote dialog: selected = %d", c->remote_list.selected);
            EndDialog(hwndDlg, wParam);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            StopOpenVPN(c);
            EndDialog(hwndDlg, wParam);
            return TRUE;
        }
        break;

    case WM_NOTIFY:
        c = (connection_t *) GetProp(hwndDlg, cfgProp);
        /* On clicking any column toggle sorting between original vs alphabetical */
        if (((LPNMHDR)lParam)->code == LVN_COLUMNCLICK)
        {
            HWND lv = GetDlgItem(hwndDlg, ID_LVW_REMOTE);
            c->remote_list.show_sorted = !c->remote_list.show_sorted;
            ListView_SortItems(lv, remote_entry_compare, &c->remote_list);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        c = (connection_t *) GetProp(hwndDlg, cfgProp);
        StopOpenVPN(c);
        EndDialog(hwndDlg, wParam);
        return TRUE;

    case WM_NCDESTROY:
        RemoveProp(hwndDlg, cfgProp);
        HFONT hf = (HFONT) GetProp(hwndDlg, L"header_font");
        if (hf) {
           DeleteObject(hf);
        }
        RemoveProp(hwndDlg, cfgProp);
        break;
    }
    return FALSE;
}

/* Find index of remote entry matching a string
 * The search starts at index=start. Use start = 0 to search
 * the whole list. Returns -1 if not found.
 */
static UINT
remote_entry_find(struct remote_list *rl, const char *str, UINT start)
{
    int res = (UINT) -1;

    if (!rl->re)
    {
        return res;
    }

    for (UINT i = start; i < rl->count; i++)
    {
        if(strcmp(rl->re[i].data, str) == 0)
        {
           res = i;
           break;
        }
    }
    return res;
}

/* Parse remote entry UTF-8 string "host,port,protocol" and
 * fill in host, port and proto, data in remote enrty.
 * Returns false on error, true on success.
 * On success, caller must free host, proto and data after use.
 */
static bool
remote_entry_parse(const char *data, struct remote_entry *re)
{
    char *token = NULL;
    bool res = false;

    char *p = strdup(data);

    re->host = NULL;
    re->proto = NULL;
    re->port = NULL;
    re->data = NULL;

    if (!p)
    {
        return res;
    }

    token = strtok(p, ",");
    if (token)
    {
        re->host = Widen(token);
        token = strtok(NULL, ",");
    }
    if (token)
    {
        re->port = Widen(token);
        token = strtok(NULL, ",");
    }
    if (token)
    {
        re->proto = Widen(token);
    }
    free(p);

    re->data = strdup(data);

    res = (re->host && re->port && re->proto && re->data);
    if (!res)
    {
        remote_entry_free(re);
    }
    return res;
}

/*
 * Position widgets in remote list window using current dpi.
 * Takes client area width and height in screen pixels as input.
 */
void
remote_listview_resize(HWND hwnd, UINT w, UINT h)
{
    MoveWindow(GetDlgItem(hwnd, ID_LVW_REMOTE), DPI_SCALE(20), DPI_SCALE(25), w - DPI_SCALE(40), h - DPI_SCALE(90), TRUE);
    MoveWindow(GetDlgItem(hwnd, ID_TXT_REMOTE), DPI_SCALE(20), DPI_SCALE(5), w-DPI_SCALE(30), DPI_SCALE(15), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDOK), DPI_SCALE(20), h - DPI_SCALE(30), DPI_SCALE(110), DPI_SCALE(23), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDCANCEL), DPI_SCALE(145), h - DPI_SCALE(30), DPI_SCALE(110), DPI_SCALE(23), TRUE);
}

static HWND
remote_listview_init(HWND parent)
{
    HWND lv;
    RECT rc;

    lv = GetDlgItem(parent, ID_LVW_REMOTE);
    if (!lv)
    {
        return NULL;
    }

    SendMessage(lv, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

    /* Use bold font for header */
    HFONT hf = (HFONT) SendMessage(lv, WM_GETFONT, 0, 0);
    if (hf)
    {
        LOGFONT lf;
        GetObject(hf, sizeof(LOGFONT), &lf);
        lf.lfWeight = FW_BOLD;

        HFONT hfb = CreateFontIndirect(&lf);
        if (hfb)
        {
            SendMessage(ListView_GetHeader(lv), WM_SETFONT, (WPARAM)hfb, 1);
            SetProp(parent, L"header_font", (HANDLE)hfb);
        }
    }

    /* Add column headings */
    UINT width[3] = {DPI_SCALE(140), DPI_SCALE(40), DPI_SCALE(60)};
    WCHAR *str[3] = {L"Hostname", L"Port", L"Protocol"};
    /*
    str[0] = LoadLocalizedString(IDS_HOSTNAME);
    str[1] = LoadLocalizedString(IDS_PORT);
    str[1] = LoadLocalizedString(IDS_PROTOCOL);
    */

    LVCOLUMNW lvc;
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    for (int i = 0; i < 3; i++)
    {
        lvc.iSubItem = i;
        lvc.pszText = str[i];
        lvc.cx = width[i];
        ListView_InsertColumn(lv, i, &lvc);
    }

    GetClientRect(parent, &rc);
    remote_listview_resize(parent, rc.right-rc.left, rc.bottom-rc.top);

    EnableWindow(lv, FALSE); /* disable until filled in */

    return lv;
}

static void CALLBACK
remote_listview_fill(HWND hwnd, UINT UNUSED msg, UINT_PTR id, DWORD UNUSED now)
{
    connection_t *c = (connection_t *) GetProp(hwnd, cfgProp);
    struct remote_list *rl = &c->remote_list;

    HWND lv = GetDlgItem(hwnd, ID_LVW_REMOTE);

    LVITEMW lvi = {0};
    lvi.mask = LVIF_TEXT|LVIF_PARAM;

    if (rl->state & STATE_FILLED)
    {
        int pos;
        UINT selected = (UINT) -1;
        for (UINT i = 0; i < rl->count; i++)
        {
            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.pszText = rl->re[i].host;
            lvi.lParam = (LPARAM) i;
            pos = ListView_InsertItem(lv, &lvi);

            if (strcmp(rl->current, rl->re[i].data) == 0)
            {
                selected = i;
            }

            ListView_SetItemText(lv, pos, 1, rl->re[i].port);
            ListView_SetItemText(lv, pos, 2, rl->re[i].proto);
        }
        EnableWindow(lv, TRUE);
        if (selected < rl->count)
        {
            ListView_EnsureVisible(lv, selected, false);
            ListView_SetItemState(lv, selected, LVIS_SELECTED, LVIS_SELECTED);
        }
        ListView_SortItems(lv, remote_entry_compare, &c->remote_list);
        SetFocus(lv);
        KillTimer(hwnd, id);
    }
    else /* request list update and set a timer to call this routine again */
    {
        remote_list_update(c); /* this returns immediately if list building is in progress */
        SetTimer(hwnd, id, 100, remote_listview_fill);
    }
}
