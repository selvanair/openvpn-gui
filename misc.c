/*
 *  OpenVPN-GUI -- A Windows GUI for OpenVPN.
 *
 *  Copyright (C) 2013 Heiko Hund <heikoh@users.sf.net>
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
#include <wincrypt.h>
#include <tchar.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#include "options.h"
#include "manage.h"
#include "main.h"
#include "misc.h"
#include "main.h"

/*
 * Helper function to do base64 conversion through CryptoAPI
 * Returns TRUE on success, FALSE on error. Caller must free *output.
 */
BOOL
Base64Encode(const char *input, int input_len, char **output)
{
    DWORD output_len;

    if (input_len == 0)
    {
        /* set output to empty string  -- matches the behavior in openvpn */
        *output = calloc (1, sizeof(char));
        return TRUE;
    }
    if (!CryptBinaryToStringA((const BYTE *) input, (DWORD) input_len,
        CRYPT_STRING_BASE64, NULL, &output_len) || output_len == 0)
    {
#ifdef DEBUG
        PrintDebug (L"Error in CryptBinaryToStringA: input = '%.*S'", input_len, input);
#endif
        *output = NULL;
        return FALSE;
    }
    *output = (char *)malloc(output_len);
    if (*output == NULL)
        return FALSE;

    if (!CryptBinaryToStringA((const BYTE *) input, (DWORD) input_len,
        CRYPT_STRING_BASE64, *output, &output_len))
    {
#ifdef DEBUG
        PrintDebug (L"Error in CryptBinaryToStringA: input = '%.*S'", input_len, input);
#endif
        free(*output);
        *output = NULL;
        return FALSE;
    }
    /* Trim trailing "\r\n" manually.
       Actually they can be stripped by adding CRYPT_STRING_NOCRLF to dwFlags,
       but Windows XP/2003 does not support this flag. */
    if(output_len > 1 && (*output)[output_len - 1] == '\x0A'
        && (*output)[output_len - 2] == '\x0D')
        (*output)[output_len - 2] = 0;

    return TRUE;
}
/*
 * Decode a nul-terminated base64 encoded input and save the result in
 * an allocated buffer *output. The caller must free *output after use.
 * The decoded output is nul-terminated so that the caller may treat
 * it as a string when appropriate.
 *
 * Return the length of the decoded result (excluding nul) or -1 on
 * error.
 */
int
Base64Decode(const char *input, char **output)
{
    DWORD len;

    PrintDebug (L"decoding %S", input);
    if (!CryptStringToBinaryA(input, 0, CRYPT_STRING_BASE64_ANY,
                              NULL, &len, NULL, NULL) || len == 0)
    {
        *output = NULL;
        return -1;
    }

    *output = malloc(len + 1);
    if (*output == NULL)
        return -1;

    if (!CryptStringToBinaryA(input, 0,
        CRYPT_STRING_BASE64, (BYTE *) *output, &len, NULL, NULL))
    {
        free(*output);
        *output = NULL;
        return -1;
    }

    /* NUL terminate output */
    (*output)[len] = '\0';
    PrintDebug (L"Decoded output %S", *output);

    return len;
}

/*
 * Helper function to convert UCS-2 text from a dialog item to UTF-8.
 * Caller must free *str if *len != 0.
 */
static BOOL
GetDlgItemTextUtf8(HWND hDlg, int id, LPSTR *str, int *len)
{
    int ucs2_len, utf8_len;
    BOOL retval = FALSE;
    LPTSTR ucs2_str = NULL;
    LPSTR utf8_str = NULL;

    *str = "";
    *len = 0;

    ucs2_len = GetWindowTextLength(GetDlgItem(hDlg, id)) + 1;
    if (ucs2_len == 1)
        goto out;

    ucs2_str = malloc(ucs2_len * sizeof(*ucs2_str));
    if (ucs2_str == NULL)
        goto out;

    if (GetDlgItemText(hDlg, id, ucs2_str, ucs2_len) == 0)
        goto out;

    utf8_len = WideCharToMultiByte(CP_UTF8, 0, ucs2_str, -1, NULL, 0, NULL, NULL);
    utf8_str = malloc(utf8_len);
    if (utf8_str == NULL)
        goto out;

    WideCharToMultiByte(CP_UTF8, 0, ucs2_str, -1, utf8_str, utf8_len, NULL, NULL);

    *str = utf8_str;
    *len = utf8_len - 1;
    retval = TRUE;
out:
    free(ucs2_str);
    return retval;
}


/*
 * Generate a management command from user input and send it
 */
BOOL
ManagementCommandFromInput(connection_t *c, LPCSTR fmt, HWND hDlg, int id)
{
    BOOL retval = FALSE;
    LPSTR input, cmd;
    int input_len, cmd_len, pos;

    GetDlgItemTextUtf8(hDlg, id, &input, &input_len);

    /* Escape input if needed */
    for (pos = 0; pos < input_len; ++pos)
    {
        if (input[pos] == '\\' || input[pos] == '"')
        {
            LPSTR buf = realloc(input, ++input_len + 1);
            if (buf == NULL)
                goto out;

            input = buf;
            memmove(input + pos + 1, input + pos, input_len - pos + 1);
            input[pos] = '\\';
            pos += 1;
        }
    }

    cmd_len = input_len + strlen(fmt);
    cmd = malloc(cmd_len);
    if (cmd)
    {
        snprintf(cmd, cmd_len, fmt, input);
        retval = ManagementCommand(c, cmd, NULL, regular);
        free(cmd);
    }

out:
    /* Clear buffers with potentially secret content */
    if (input_len)
    {
        memset(input, 'x', input_len);
        SetDlgItemTextA(hDlg, id, input);
        free(input);
    }

    return retval;
}



/*
 * Generate a management command from double user inputs and send it
 */
BOOL
ManagementCommandFromInputBase64(connection_t *c, LPCSTR fmt, HWND hDlg,int id, int id2)
{
    BOOL retval = FALSE;
    LPSTR input, input2, input_b64, input2_b64, cmd;
    int input_len, input2_len, cmd_len, pos;

    input_b64 = NULL;
    input2_b64 = NULL;

    GetDlgItemTextUtf8(hDlg, id, &input, &input_len);
    GetDlgItemTextUtf8(hDlg, id2, &input2, &input2_len);

    /* Escape input if needed */
    for (pos = 0; pos < input_len; ++pos)
    {
        if (input[pos] == '\\' || input[pos] == '"')
        {
            LPSTR buf = realloc(input, ++input_len + 1);
            if (buf == NULL)
                goto out;

            input = buf;
            memmove(input + pos + 1, input + pos, input_len - pos + 1);
            input[pos] = '\\';
            pos += 1;
        }
    }
    for (pos = 0; pos < input2_len; ++pos)
    {
        if (input2[pos] == '\\' || input2[pos] == '"')
        {
            LPSTR buf = realloc(input2, ++input2_len + 1);
            if (buf == NULL)
                goto out;

            input2 = buf;
            memmove(input2 + pos + 1, input2 + pos, input2_len - pos + 1);
            input2[pos] = '\\';
            pos += 1;
        }
    }

    if (!Base64Encode(input, input_len, &input_b64))
        goto out;
    if (!Base64Encode(input2, input2_len, &input2_b64))
        goto out;

    cmd_len = strlen(input_b64) + strlen(input2_b64) + strlen(fmt);
    cmd = malloc(cmd_len);
    if (cmd)
    {
        snprintf(cmd, cmd_len, fmt, input_b64, input2_b64);
        retval = ManagementCommand(c, cmd, NULL, regular);
        free(cmd);
    }

out:
    /* Clear buffers with potentially secret content */
    if (input_b64)
        memset(input_b64, 0, strlen(input_b64));
    if (input2_b64)
        memset(input2_b64, 0, strlen(input2_b64));
    free(input_b64);
    free(input2_b64);

    if (input_len)
    {
        memset(input, 'x', input_len);
        SetDlgItemTextA(hDlg, id, input);
        free(input);
    }
    if (input2_len)
    {
        memset(input2, 'x', input2_len);
        SetDlgItemTextA(hDlg, id2, input2);
        free(input2);
    }

    return retval;
}


/*
 * Ensures the given directory exists, by checking for and
 * creating missing parts of the path.
 * If the path does not exist and cannot be created return FALSE.
 */
BOOL
EnsureDirExists(LPTSTR dir)
{
    DWORD attr = GetFileAttributes(dir);
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        DWORD error = GetLastError();
        if (error == ERROR_PATH_NOT_FOUND)
        {
            LPTSTR pos = _tcsrchr(dir, '\\');
            if (pos == NULL)
                return FALSE;

            *pos = '\0';
            BOOL ret = EnsureDirExists(dir);
            *pos = '\\';
            if (ret == FALSE)
                return FALSE;
        }
        else if (error != ERROR_FILE_NOT_FOUND)
            return FALSE;

        /* No error if directory already exists */
        return (CreateDirectory(dir, NULL) == TRUE
            ||  GetLastError() == ERROR_ALREADY_EXISTS);
    }

    return (attr & FILE_ATTRIBUTE_DIRECTORY ? TRUE : FALSE);
}


/*
 * Various string helper functions
 */
BOOL
streq(LPCSTR str1, LPCSTR str2)
{
    return (strcmp(str1, str2) == 0);
}

BOOL
strbegins(const char *str, const char *begin)
{
    return (strncmp(str, begin, strlen(begin)) == 0);
}

BOOL
wcsbegins(LPCWSTR str, LPCWSTR begin)
{
    return (wcsncmp(str, begin, wcslen(begin)) == 0);
}

/*
 * Force setting window as foreground window by simulating an ALT keypress
 */
BOOL
ForceForegroundWindow(HWND hWnd)
{
    BOOL ret = FALSE;

    keybd_event(VK_MENU, 0, 0, 0);
    ret = SetForegroundWindow(hWnd);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);

    return ret;
}

/*
 * Check user has admin rights
 * Taken from https://msdn.microsoft.com/en-us/library/windows/desktop/aa376389(v=vs.85).aspx
 * Returns true if the calling process token has the local Administrators group enabled
 * in its SID.  Assumes the caller is not impersonating and has access to open its own 
 * process token.
 */
BOOL IsUserAdmin(VOID)
{
    BOOL b;
    SID_IDENTIFIER_AUTHORITY NtAuthority = {SECURITY_NT_AUTHORITY};
    PSID AdministratorsGroup;

    b = AllocateAndInitializeSid (&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &AdministratorsGroup);
    if(b)
    {
        if (!CheckTokenMembership(NULL, AdministratorsGroup, &b))
            b = FALSE;
        FreeSid(AdministratorsGroup);
    }

    return(b);
}

HANDLE
InitSemaphore (void)
{
    HANDLE semaphore = NULL;
    semaphore = CreateSemaphore (NULL, 1, 1, NULL);
    if (!semaphore)
    {
        MessageBoxW (NULL, L"Error creating semaphore", TEXT(PACKAGE_NAME), MB_OK);
#ifdef DEBUG
        PrintDebug (L"InitSemaphore: CreateSemaphore failed [error = %lu]", GetLastError());
#endif
    }
    return semaphore;
}

/* Check access rights on an existing file */
BOOL
CheckFileAccess (const TCHAR *path, int access)
{
    HANDLE h;
    bool ret = FALSE;

    h = CreateFile (path, access, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL, NULL);
    if ( h != INVALID_HANDLE_VALUE )
    {
        ret = TRUE;
        CloseHandle (h);
    }

    return ret;
}

/*
 * Convert a NUL terminated utf8 string to widechar. The caller must free
 * the returned pointer. Return NULL on error.
 */
WCHAR *
Widen(const char *utf8)
{
    WCHAR *wstr = NULL;
    if (!utf8)
        return wstr;

    int nch = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (nch > 0)
        wstr = malloc(sizeof(WCHAR) * nch);
    if (wstr)
        nch =  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, nch);

    if (nch == 0 && wstr)
    {
        free (wstr);
        wstr = NULL;
    }

    return wstr;
}

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
 * form xxx=yyy, only the part xxx is used for matchig.
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
