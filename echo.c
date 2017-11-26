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
#include <openssl/sha.h>
#include "main.h"
#include "options.h"
#include "misc.h"
#include "openvpn.h"
#include "echo.h"
#include "save_pass.h"
#include "tray.h"
#include "localization.h"
#include "openvpn-gui-res.h"

extern options_t o;

struct echo_msg_history {
    BYTE digest[HASHLEN];
    time_t timestamp;
    struct echo_msg_history *next;
};

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

static struct echo_msg_history *
echo_msg_recall(BYTE *digest, struct echo_msg_history *hist)
{
    for( ; hist; hist = hist->next)
    {
       PrintDebug(L"In recall: history entry with digest[0] = %u", hist->digest[0]);
       if (memcmp(hist->digest, digest, HASHLEN) == 0) break;
    }
    if (hist)
        PrintDebug(L"Found matching message in history: time = %u", (UINT)hist->timestamp);
    else
        PrintDebug(L"No entry in history with with digest[0] = %02x", digest[0]);
    return hist;
}

/* Return true if message in history and last displayed not long ago */
static BOOL
echo_msg_repeated(struct echo_msg *msg)
{
    const struct echo_msg_history *hist;

    hist = echo_msg_recall(msg->digest, msg->history);
    return (hist && (hist->timestamp + o.popup_mute_interval*3600 > msg->timestamp));
}

/* Add message to history -- update if already present */
static void
echo_msg_save(struct echo_msg *msg)
{
    struct echo_msg_history *hist = echo_msg_recall(msg->digest, msg->history);
    if (hist)
    {
        hist->timestamp = msg->timestamp;
        PrintDebug(L"Updated history entry with digest[0] = %02x", hist->digest[0]);
    }
    else
    {
        hist = malloc(sizeof(struct echo_msg_history));
        if (!hist)
            return;
        memcpy(hist->digest, msg->digest, HASHLEN);
        hist->timestamp = msg->timestamp;
        hist->next = msg->history;
        msg->history = hist;
        PrintDebug(L"Added history entry with digest[0] = %02x", hist->digest[0]);
    }
}

static void
echo_msg_add_digest(struct echo_msg *msg)
{
    if (!msg->text) return;
    size_t len = msg->length*sizeof(msg->text[0]);
    SHA1((BYTE*)msg->text, len, msg->digest);
    PrintDebug(L"msg digest[0] = %02x", msg->digest[0]);
}

static void
echo_msg_append(connection_t *c, time_t timestamp, const char *msg, BOOL addnl)
{
    wchar_t *separator = L"";
    wchar_t *wmsg = NULL;

    PrintDebug(L"echo_msg so far: %s (%d chars)\n", c->echo_msg.text, c->echo_msg.length);

    if (!(wmsg = Widen(msg)))
    {
        WriteStatusLog(c, L"GUI> ", L"Error: out of memory while processing echo msg", false);
        goto out;
    }

    size_t len = c->echo_msg.length + wcslen(wmsg) + 1;  /* including null terminator */
    if (addnl && c->echo_msg.length != 0)
    {
        separator = L"\r\n";
        len += 2;
    }
    WCHAR *s = realloc(c->echo_msg.text, len*sizeof(WCHAR));
    if (!s)
    {
        WriteStatusLog(c, L"GUI> ", L"Error: out of memory while processing echo msg", false);
        goto out;
    }
    swprintf(s + c->echo_msg.length, len - c->echo_msg.length,  L"%s%s", separator, wmsg);

    s[len-1] = L'\0';
    c->echo_msg.text = s;
    c->echo_msg.length = len - 1; /* exclude null terminator */
    c->echo_msg.timestamp = timestamp;

    PrintDebug(L"echo_msg so far: %s (%d chars)\n", c->echo_msg.text, c->echo_msg.length);

out:
    free(wmsg);
    return;
}

static INT_PTR CALLBACK
MessageDialogFunc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void
echo_msg_display(connection_t *c, time_t timestamp, const char *title, int type)
{
    WCHAR *wtitle = Widen(title);

    c->echo_msg.timestamp = timestamp;
    if (wtitle)
    {
        c->echo_msg.title = wtitle;
    }
    else
    {
        WriteStatusLog(c, L"GUI> ", L"Error: out of memory converting echo message title to widechar", false);
        c->echo_msg.title = L"Admin speaking";
    }
    echo_msg_add_digest(&c->echo_msg);

    WriteStatusLog(c, L"GUI> New message: ", c->echo_msg.title, false);

    /* We ignore msg-window if not in connecting state (eg., when reconnecting)
     * All messages ignored if same as recently shown.
     */
    if ((c->state != connecting && type == ECHO_MSG_WINDOW)
        || c->flags & FLAG_DISABLE_ECHO_MSG || echo_msg_repeated(&c->echo_msg))
    {
        WriteStatusLog(c, L"GUI> ", L"Repeated message muted", false);
        return;
    }
    if (type == ECHO_MSG_WINDOW)
    {
        /* MessageBoxEx(NULL, c->echo_msg.text, c->echo_msg.title, MB_OK, GetGUILanguage()); */
        HWND h = CreateLocalizedDialogParam(ID_DLG_MESSAGE, MessageDialogFunc, (LPARAM) &(c->echo_msg));
        ShowWindow(h, SW_SHOW);
    }
    else /* assume notify type */
    {
        ShowTrayBalloon(c->echo_msg.title, c->echo_msg.text);
    }
    /* save in or update history */
    echo_msg_save(&c->echo_msg);
}

static void
echo_msg_parse(connection_t *c, time_t timestamp, const char *s)
{
    wchar_t errmsg[256] = L"";

    char *msg = url_decode(s);
    if (!msg)
    {
        WriteStatusLog(c, L"GUI> ", L"Error in url_decode of echo message", false);
        return;
    }

    if (strbegins(msg, "msg "))
    {
        echo_msg_append(c, timestamp, msg + 4, true);
    }
    else if (streq(msg, "msg")) /* empty msg is treated as a new line */
    {
        echo_msg_append(c, timestamp, msg+3, true);
    }
    else if (strbegins(msg, "msg-n "))
    {
        echo_msg_append(c, timestamp, msg + 6, false);
    }
    else if (strbegins(msg, "msg-window "))
    {
        echo_msg_display(c, timestamp, msg + 11, ECHO_MSG_WINDOW);
        echo_msg_clear(c, false);
    }
    else if (strbegins(msg, "msg-notify "))
    {
        echo_msg_display(c, timestamp, msg + 11, ECHO_MSG_NOTIFY);
        echo_msg_clear(c, false);
    }
    else
    {
        _sntprintf_0(errmsg, L"WARNING: Unknown ECHO directive '%hs' ignored.", msg);
        WriteStatusLog(c, L"GUI> ", errmsg, false);
    }
    free(msg);
}

void
echo_msg_clear(connection_t *c, BOOL clear_history)
{
    free(c->echo_msg.text);
    free(c->echo_msg.title);
    c->echo_msg.text = NULL;
    c->echo_msg.length = 0;
    c->echo_msg.title = NULL;

    if (clear_history)
    {
        struct echo_msg_history *head = c->echo_msg.history;
        struct echo_msg_history *next;
        while (head)
        {
            next = head->next;
            free(head);
            head = next;
        }
        CLEAR(c->echo_msg);
        PrintDebug(L"Cleared echo msg history");
    }
}

void
process_echo(connection_t *c, time_t timestamp, const char *msg)
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
    else if (strbegins(msg, "msg"))
    {
        echo_msg_parse(c, timestamp, msg);
    }
    else
    {
        _sntprintf_0(errmsg, L"WARNING: Unknown ECHO directive '%S' ignored.", msg);
        WriteStatusLog(c, L"GUI> ", errmsg, false);
    }
}

static void
RenderMessageWindow(HWND hwnd, UINT w, UINT h)
{
    HWND hmsg = GetDlgItem(hwnd, ID_TXT_MESSAGE);
    MoveWindow(hmsg, 0, 0, w, h, TRUE);
}

static INT_PTR CALLBACK
MessageDialogFunc(HWND hwnd, UINT msg, UNUSED WPARAM wParam, LPARAM lParam)
{
    static HBRUSH hbr = NULL;
    HWND hmsg = GetDlgItem(hwnd, ID_TXT_MESSAGE);
    struct echo_msg *echo_msg;
    HICON hIcon = LoadLocalizedIcon(ID_ICO_APP);


    switch (msg)
    {
    case WM_INITDIALOG:
        echo_msg = (struct echo_msg *) lParam;

        if (hIcon) {
            SendMessage(hwnd, WM_SETICON, (WPARAM) (ICON_SMALL), (LPARAM) (hIcon));
            SendMessage(hwnd, WM_SETICON, (WPARAM) (ICON_BIG), (LPARAM) (hIcon));
        }

        SendMessage(hmsg, EM_SETMARGINS, EC_LEFTMARGIN|EC_RIGHTMARGIN,
                    MAKELPARAM(DPI_SCALE(10), DPI_SCALE(10)));
        SetDlgItemText(hwnd, ID_TXT_MESSAGE, echo_msg->text);
        SetWindowText(hwnd, echo_msg->title);

        /* Re-size the window to match the text width/height */
        int sx = GetSystemMetrics(SM_CXSCREEN); /* screen size along x*/
        int sy = GetSystemMetrics(SM_CYSCREEN); /* screen size along y*/
        RECT rect;
        GetClientRect(hwnd, &rect);
        PrintDebug(L"client rect = %d %d", rect.right, rect.bottom);
        RECT rect1 = rect;
        int height = DrawText(GetDC(hmsg), echo_msg->text, -1, &rect1,
                              DT_TOP|DT_LEFT|DT_EDITCONTROL|DT_WORDBREAK|DT_CALCRECT);
        rect.bottom = DPI_SCALE(height*3/4) + DPI_SCALE(70); /* extra for the margins, caption */
        if (rect.bottom > sy) rect.bottom = sy;
        PrintDebug(L"text rect1 = %d %d %d %d (height=%d)", rect1.left, rect1.top, rect1.right, rect1.bottom, height);
        /* place near top right of screen variably offset by ~50 pixels,
         * in case multiple windows popup
         */
        int nx = sx - rect.right - DPI_SCALE(rand()%50+25);
        PrintDebug(L"nx = %d", nx);
        MoveWindow(hwnd, (nx < 0) ? 0:nx, DPI_SCALE(rand()%50+25), rect.right, rect.bottom, TRUE);
        GetClientRect(hwnd, &rect);
        PrintDebug(L"window rect = %d %d %d %d", rect.left, rect.top, rect.right, rect.bottom);
        RenderMessageWindow(hwnd, rect.right, rect.bottom);

        SetFocus(hmsg);
        /* move cursor to end */
        SendMessage(hmsg, EM_SETSEL, 0, (LPARAM) -1);
        SendMessage(hmsg, EM_SETSEL, (WPARAM) -1, (LPARAM) -1);
        return FALSE;

    case WM_SIZE:
        RenderMessageWindow(hwnd, LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_CTLCOLORSTATIC:
        if (GetDlgCtrlID((HWND) lParam) == ID_TXT_MESSAGE)
        {
            SetBkColor((HDC) wParam, RGB(255,255,255));
            if (!hbr)
                hbr = CreateSolidBrush(RGB(255, 255, 255));
            return (INT_PTR) hbr;
        }
        break;

    case WM_CLOSE:
        if (hbr)
            DeleteObject(hbr);
        DestroyWindow(hwnd);
        return TRUE;
    }

    return 0;
}
