/*
 *  OpenVPN-GUI -- A Windows GUI for OpenVPN.
 *
 *  Copyright (C) 2016 Selva Nair <selva.nair@gmail.com>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "config_parser.h"

#define MAX_PARMS 16
#define MAX_LINE 256

struct config_parser {
    FILE *fd;
    wchar_t line[MAX_LINE];
    wchar_t sline[MAX_LINE];
    wchar_t *tokens[MAX_PARMS];
    int ntokens;
    int line_num;
} config_data;

config_parser_t *
OpenConfig (const wchar_t *fname)
{
    config_parser_t *cp = calloc(sizeof(config_parser_t),1);

    if (cp != NULL)
    {
       if (fname == NULL)
           cp->fd = stdin;
       else if ( (cp->fd = _wfopen (fname, L"r")) == NULL )
       {
           free (cp);
           cp = NULL;
       }
    }
    return cp;
}

void
CloseConfig (config_parser_t *cp)
{
    if (cp->fd && cp->fd != stdin)
        fclose(cp->fd);
    free (cp);
}

static void
reset (config_parser_t *cp)
{
    cp->ntokens = 0;
}

static int
legal_escape (wchar_t c)
{
    wchar_t *escapes = L"\"\' \\"; // space, ", ' or backslash
    return (wcschr(escapes, c) != NULL);
}

static int
copy_token (wchar_t **dest, wchar_t **src, wchar_t* delim)
{
    wchar_t *p = *src;
    wchar_t *s = *dest;

    /* copy src to dest until delim character with escaped chars converted */
    for ( ; *p != L'\0' && wcschr(delim, *p) == NULL; p++, s++)
    {
        if (*p == L'\\' && legal_escape(*(p+1)))
            *s = *(++p);
        else if (*p == L'\\')
        {
            /* fwprintf(stderr, L"backslash parse error at position %s \n", p); */
            return -1; // parse error -- illegal backslash in input
        }
        else
            *s = *p;
    }
    /* at this point p is one of the delimiters or null */
    *s = L'\0';
    *src = p;
    *dest = s;
    return 0;
}

static int
tokenize (config_parser_t *cp)
{
    wchar_t *p, *s;
    p = cp->line;
    s = cp->sline;
    int i = 0;
    int status;

    for ( ; *p != L'\0';  p++, s++)
    {
        if (*p == L' ' || *p == L'\t') continue;

        if (MAX_PARMS <= i)
        {
            /* fprintf (stderr, "Too many tokens in options line"); */
            return -1;
        }
        cp->tokens[i++] = s;

        if (*p == L'\'' )
        {
            int len = wcscspn (++p, L"\'");
            wcsncpy (s, p, len);
            s += len;
            p += len;
        }
        else if (*p == L'\"')
        {
            p++;
            status = copy_token (&s, &p, L"\"");
        }
        else
            status = copy_token (&s, &p, L" \t");
        if (status != 0) return status;

        if (*p == L'\0') break;
    }
    cp->ntokens = i;
    return 0;
}

wchar_t *
ConfigReadline (config_parser_t *cp)
{
    wchar_t *s;
    int len;
    char tmp[MAX_LINE];
    int offset = 0;

    reset(cp);

    if (fgets (tmp, MAX_LINE, cp->fd) == NULL)
        return NULL;
    cp->line_num++;
    /* Ignore UTF-8 BOM at start of stream */
    if (cp->line_num == 1 && strncmp (tmp, "\xEF\xBB\xBF", 3) == 0)
        offset = 3;
    mbstowcs (cp->line, tmp + offset, MAX_LINE-1);
    cp->line[MAX_LINE-1] = L'\0';

    s = cp->line;
    len = wcscspn (s, L"\n\r");
    s[len] = L'\0';

    if (tokenize (cp) != 0)
    {
        /* fprintf(stderr, "Error in tokenize\n"); */
        reset (cp);
        return NULL;
    }

    return cp->line;
}

int
ConfigNumTokens (config_parser_t *cp)
{
    return cp->ntokens;
}

wchar_t *
ConfigGetToken (config_parser_t *cp, int n)
{
    if (n < cp->ntokens)
        return cp->tokens[n];
    else
        return L"";
}

BOOL
HasConfigOption(const wchar_t *config_file, const wchar_t *option)
{
    config_parser_t *cp = OpenConfig (config_file);
    if (!cp)
        return FALSE;

    wchar_t *s;
    while ( (s = ConfigReadline (cp)) != NULL)
    {
        if (cp->ntokens < 1
            || wcscmp(ConfigGetToken(cp, 0), option))
            continue;
        break;
    }
    CloseConfig(cp);

    return (s != NULL);
}

#if 0
void error_exit(char *msg)
{
    fwprintf(stderr, L"Fatal error in config_parser: %s\n", msg);
    exit (1);
}

int main (int argc, char *argv[])
{
    config_parser_t *cp;
    int i;

    cp = OpenConfig (argc > 1 ? argv[1] : NULL);
    if (!cp)
        error_exit ("Error in ConfigInit");

    wchar_t *s;
    while ( (s = ConfigReadline (cp)) != NULL)
    {
       fwprintf (stdout, L"read: %ls\n", s);
       for (i = 0; i < cp->ntokens; i++)
           fwprintf (stdout, L"token %d : %ls\n", i, ConfigGetToken (cp, i));
    }
    return 0;
}
#endif
