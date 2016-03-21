#ifndef SECURE_STORE_H
#define SECURE_STORE_H

#define USER_PASS_LEN 128
typedef struct user_pass {
   WCHAR username[USER_PASS_LEN];
   WCHAR password[USER_PASS_LEN];
} user_pass_t;

int store_passphrase(connection_t *c, const WCHAR *passphrase);
int store_user_pass(connection_t *c, const user_pass_t *ua, BOOL save_pass);
int recall_passphrase(connection_t *c, WCHAR *passphrase);

int recall_user_pass(connection_t *c, user_pass_t *ua);

int clear_user_pass(connection_t *c);
int clear_passphrase(connection_t *c);
int delete_secure_store(connection_t *c);
#endif
