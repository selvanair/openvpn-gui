#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include "save_pass.h"

void Usage(WCHAR *prog)
{
    wprintf(L"\nRead password saved by OpenVPN-GUI and save it as\n"
           "a DOMAIN_PASSWORD in windows credential vault\n\n"
           "Usage: %s config_name domain\n\n", prog);
    exit(2);
}
int wmain(int argc, WCHAR *argv[])
{
    WCHAR *domain = L"global.local";
    WCHAR *config_name;
    _setmode(_fileno(stdout), _O_U16TEXT);
    if (argc < 2)
        Usage(argv[0]);
    config_name = argv[1];
    if (argc > 2)
        domain = argv[2];

    DWORD status = SaveDomainCredentials(config_name, domain);
    if (status == 232)
        wprintf(L"Saving credentials failed: no saved passwords found for this config '%s'\n",
                config_name);
    else if (status)
        wprintf(L"Saving credentials failed: error = %lu\n", status);
    return (status == 0) ? 0 : 1;
}

/* functions needed for save_pass */

#ifndef UNUSED
#define UNUSED __attribute__ ((unused))
#endif
/* ----- from registry.c --------- */
/*
 * Open HKCU\Software\OpenVPN-GUI\configs\config-name.
 * The caller must close the key. Returns 1 on success.
 */
static int
OpenConfigRegistryKey(const WCHAR *config_name, HKEY *regkey, BOOL create)
{
    DWORD status;
    const WCHAR fmt[] = L"SOFTWARE\\OpenVPN-GUI\\configs\\%s";
    int count = (wcslen(config_name) + wcslen(fmt) + 1);
    WCHAR *name = malloc(count * sizeof(WCHAR));

    if (!name)
        return 0;

    _snwprintf(name, count, fmt, config_name);
    name[count-1] = L'\0';

    if (!create)
       status = RegOpenKeyEx (HKEY_CURRENT_USER, name, 0, KEY_READ | KEY_WRITE, regkey);
    else
    /* create if key doesn't exist */
       status = RegCreateKeyEx(HKEY_CURRENT_USER, name, 0, NULL,
               REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, regkey, NULL);
    free (name);

    return (status == ERROR_SUCCESS);
}

int
SetConfigRegistryValueBinary(UNUSED const WCHAR *config_name, UNUSED const WCHAR *name, UNUSED const BYTE *data, UNUSED DWORD len)
{
    /* unused -- abort */
    wprintf(L"save_cred: call to %S not supported. Abort.\n", __func__);
    exit(1);
}

int
DeleteConfigRegistryValue(UNUSED const WCHAR *config_name, UNUSED const WCHAR *name)
{
    /* unused -- abort */
    wprintf(L"save_cred: call to %S not supported. Abort.\n", __func__);
    exit(1);
}

DWORD
GetConfigRegistryValue(const WCHAR *config_name, const WCHAR *name, BYTE *data, DWORD len)
{
    DWORD status;
    DWORD type;
    HKEY regkey;

    if (!OpenConfigRegistryKey(config_name, &regkey, FALSE))
        return 0;
    status = RegQueryValueEx(regkey, name, NULL, &type, data, &len);
    RegCloseKey(regkey);
    if (status == ERROR_SUCCESS)
        return len;
    else
        return 0;
}

/* ----- from main.c --------- */
void
PrintDebugMsg(WCHAR *msg)
{
    wprintf(msg);
}

/* ----- from passphrase.c --------- */
BOOL
GetRandomPassword(UNUSED char *s, UNUSED size_t n)
{
    /* not used -- abort */
    wprintf(L"save_cred: call to %S not supported. Abort.\n", __func__);
    exit(1);
}
