#include <stdlib.h>
#include <windows.h>
#include <wincrypt.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "options.h"
#include "registry.h"
#include "secure_store.h"

extern options_t o;

static DWORD
crypt_protect(BYTE *data, int szdata, LPCWSTR desc, BYTE **out)
{
    DATA_BLOB data_in;
    DATA_BLOB data_out;

    data_in.pbData = data;
    data_in.cbData = szdata;

    if(CryptProtectData(&data_in, desc, NULL, NULL, NULL, 0, &data_out))
    {
        *out = data_out.pbData;
        return data_out.cbData;
    }
    else
        return 0;
}

static DWORD
crypt_unprotect(BYTE *data, int szdata, BYTE **out)
{
    DATA_BLOB data_in;
    DATA_BLOB data_out;

    data_in.pbData = data;
    data_in.cbData = szdata;

    if(CryptUnprotectData(&data_in, NULL, NULL, NULL, NULL, 0, &data_out))
    {
        *out = data_out.pbData;
        return
            data_out.cbData;
    }
    else
        return 0;
}

/*
 * Encrypt the null terminated string passphrase and store it in the
 * registry with key name "key-data".
 */
int
store_passphrase(connection_t *c, const WCHAR *passphrase)
{
    BYTE *out;
    DWORD len = (wcslen(passphrase) + 1) * sizeof(*passphrase);

    len = crypt_protect((BYTE*) passphrase, len, NULL, &out); /* null termination included */
    if(len > 0)
    {
        SetConfigRegistryValueBinary(c, L"key-data", out, len);
        LocalFree(out);
    }
    return 0;
}

/*
 * Encrypt the username/password struct and store in the registry with
 * key name "user-data". The username is always stored, but password is
 * zeroed before storing.
 */
int
store_user_pass(connection_t *c, const user_pass_t *ua, BOOL save_pass)
{
    BYTE *out;
    DWORD len;
    user_pass_t u; /* local copy for zeroing password before save if needed */

    memcpy(&u, ua, sizeof(u));
    if(!save_pass)
        SecureZeroMemory(&u.password, sizeof(u.password));

    len = crypt_protect((BYTE*) &u, sizeof(user_pass_t), NULL, &out);
    if(len > 0) {
        SetConfigRegistryValueBinary(c, L"user-data", out, len);
        LocalFree(out);
    }

    SecureZeroMemory(&u, sizeof(u));

    return 0;
}

/*
 * Returns 0 on success, 1 on failure. passphrase should be have space
 * for up to USER_PASS_LEN wide chars incluing null termination
 */
int
recall_passphrase(connection_t *c, WCHAR *passphrase)
{
    BYTE in[2048];
    BYTE *out;
    int len;
    DWORD retval = 1;

    *passphrase = 0;
    len = GetConfigRegistryValue(c, L"key-data", in, sizeof(in));
    if(len <= 0)
        return 1;

    len = crypt_unprotect(in, len, &out);
    if (len  <= USER_PASS_LEN)
    {
        wcsncpy(passphrase, (WCHAR*) out, len);
        passphrase[len-1] = 0; /* in case the data was corrupted */
        retval = 0;
    }

    SecureZeroMemory(&out, len);
    LocalFree(out);

    return retval;
}

int
recall_user_pass(connection_t *c, user_pass_t *ua)
{
    BYTE in[2048];
    BYTE *out;
    int len;

    len = GetConfigRegistryValue(c, L"user-data", in, _countof(in));
    if(len <= 0)
        return 1;
    len = crypt_unprotect(in, len, &out);
    if(len != sizeof(user_pass_t))
    {
        SecureZeroMemory(out, len);
        LocalFree(out);
        return 1; /* wrong data? */
    }
    memcpy(ua, out, len);
    ua->username[USER_PASS_LEN-1] = L'\0';
    ua->password[USER_PASS_LEN-1] = L'\0';

    SecureZeroMemory(out, len);
    LocalFree(out);

    return 0;
}

int
clear_user_pass(connection_t *c)
{
    user_pass_t ua;
    if(recall_user_pass(c, &ua) == 0)
    {
        memset(ua.password, 0, sizeof(ua.password));
        store_user_pass(c, &ua, TRUE);
    }
    return 0;
}

int
clear_passphrase(connection_t *c)
{
    const WCHAR *passphrase = L"";
    store_passphrase(c, passphrase);
    return 0;
}

/* clear all saved secure data */
int
delete_secure_store(connection_t *c)
{
    DeleteConfigRegistryValue(c, L"key-data");
    DeleteConfigRegistryValue(c, L"user-data");
    return 0;
}
