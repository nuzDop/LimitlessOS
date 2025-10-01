#ifndef LIMITLESS_AUTH_H
#define LIMITLESS_AUTH_H

/*
 * Local Authentication System
 * Phase 1: Basic username/password authentication
 */

#include "kernel.h"

#define USERNAME_MAX_LEN 64
#define PASSWORD_HASH_LEN 128
#define FULLNAME_MAX_LEN 128

/* User ID type */
typedef uint32_t uid_t;
typedef uint32_t gid_t;

#define UID_ROOT 0
#define UID_INVALID 0xFFFFFFFF

/* User structure */
typedef struct user {
    uid_t uid;
    gid_t gid;
    char username[USERNAME_MAX_LEN];
    char password_hash[PASSWORD_HASH_LEN];
    char full_name[FULLNAME_MAX_LEN];
    char home_directory[256];
    char shell[256];
    bool is_admin;
    bool is_locked;
    uint64_t created_time;
    uint64_t last_login;
} user_t;

/* Authentication session */
typedef struct auth_session {
    uint64_t session_id;
    uid_t uid;
    uint64_t created_time;
    uint64_t last_activity;
    uint32_t flags;
    char remote_addr[64];
} auth_session_t;

/* Session flags */
#define AUTH_SESSION_ACTIVE    BIT(0)
#define AUTH_SESSION_ELEVATED  BIT(1)  // Elevated privileges (sudo)
#define AUTH_SESSION_REMOTE    BIT(2)  // Remote session

/* Authentication functions */
status_t auth_init(void);
status_t auth_create_user(const char* username, const char* password, const char* full_name, bool is_admin, uid_t* out_uid);
status_t auth_delete_user(uid_t uid);
status_t auth_verify_password(const char* username, const char* password, uid_t* out_uid);
status_t auth_change_password(uid_t uid, const char* old_password, const char* new_password);
status_t auth_get_user(uid_t uid, user_t* out_user);
status_t auth_find_user(const char* username, user_t* out_user);

/* Session management */
status_t auth_create_session(uid_t uid, auth_session_t* out_session);
status_t auth_destroy_session(uint64_t session_id);
status_t auth_validate_session(uint64_t session_id, auth_session_t* out_session);
status_t auth_elevate_session(uint64_t session_id, const char* password);

/* Password hashing */
void auth_hash_password(const char* password, char* hash_out);
bool auth_verify_hash(const char* password, const char* hash);

/* Privilege checking */
bool auth_is_admin(uid_t uid);
bool auth_can_access(uid_t uid, const char* resource, uint32_t permissions);

#endif /* LIMITLESS_AUTH_H */
