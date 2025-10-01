/*
 * Local Authentication System
 * Phase 1: Basic implementation
 */

#include "kernel.h"
#include "auth.h"
#include "microkernel.h"

/* Maximum users (Phase 1 limitation) */
#define MAX_USERS 256
#define MAX_SESSIONS 1024

/* User database (in-memory for Phase 1) */
static user_t user_db[MAX_USERS];
static uint32_t user_count = 0;
static uid_t next_uid = 1000;  // Start from 1000 (root is 0)

/* Session database */
static auth_session_t session_db[MAX_SESSIONS];
static uint32_t session_count = 0;
static uint64_t next_session_id = 1;

/* Lock for thread safety */
static volatile uint32_t auth_lock = 0;

/* Simple string functions */
static UNUSED size_t auth_strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

static void auth_strcpy(char* dest, const char* src, size_t max_len) {
    size_t i = 0;
    while (src[i] && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int auth_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* SHA-256 constants */
static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* SHA-256 helper functions */
#define SHA256_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA256_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_EP0(x) (SHA256_ROTR(x, 2) ^ SHA256_ROTR(x, 13) ^ SHA256_ROTR(x, 22))
#define SHA256_EP1(x) (SHA256_ROTR(x, 6) ^ SHA256_ROTR(x, 11) ^ SHA256_ROTR(x, 25))
#define SHA256_SIG0(x) (SHA256_ROTR(x, 7) ^ SHA256_ROTR(x, 18) ^ ((x) >> 3))
#define SHA256_SIG1(x) (SHA256_ROTR(x, 17) ^ SHA256_ROTR(x, 19) ^ ((x) >> 10))

/* SHA-256 transform */
static void sha256_transform(uint32_t state[8], const uint8_t data[64]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h, t1, t2;

    /* Prepare message schedule */
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               ((uint32_t)data[i * 4 + 3]);
    }

    for (int i = 16; i < 64; i++) {
        w[i] = SHA256_SIG1(w[i - 2]) + w[i - 7] + SHA256_SIG0(w[i - 15]) + w[i - 16];
    }

    /* Initialize working variables */
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    /* Main loop */
    for (int i = 0; i < 64; i++) {
        t1 = h + SHA256_EP1(e) + SHA256_CH(e, f, g) + sha256_k[i] + w[i];
        t2 = SHA256_EP0(a) + SHA256_MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    /* Add compressed chunk to current hash value */
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

/* Compute SHA-256 hash */
static void sha256_compute(const uint8_t* data, size_t len, uint8_t hash[32]) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    uint8_t block[64];
    size_t i = 0;

    /* Process full blocks */
    while (i + 64 <= len) {
        for (int j = 0; j < 64; j++) {
            block[j] = data[i + j];
        }
        sha256_transform(state, block);
        i += 64;
    }

    /* Prepare final block with padding */
    size_t remaining = len - i;
    for (size_t j = 0; j < remaining; j++) {
        block[j] = data[i + j];
    }
    block[remaining] = 0x80;

    if (remaining >= 56) {
        for (size_t j = remaining + 1; j < 64; j++) {
            block[j] = 0;
        }
        sha256_transform(state, block);
        for (int j = 0; j < 56; j++) {
            block[j] = 0;
        }
    } else {
        for (size_t j = remaining + 1; j < 56; j++) {
            block[j] = 0;
        }
    }

    /* Append length in bits */
    uint64_t bit_len = len * 8;
    for (int j = 0; j < 8; j++) {
        block[63 - j] = (bit_len >> (j * 8)) & 0xFF;
    }
    sha256_transform(state, block);

    /* Produce final hash */
    for (int i = 0; i < 8; i++) {
        hash[i * 4] = (state[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = state[i] & 0xFF;
    }
}

/* Initialize authentication system */
status_t auth_init(void) {
    /* Clear databases */
    for (uint32_t i = 0; i < MAX_USERS; i++) {
        user_db[i].uid = UID_INVALID;
    }

    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        session_db[i].session_id = 0;
    }

    user_count = 0;
    session_count = 0;

    /* Create root user */
    user_db[0].uid = UID_ROOT;
    user_db[0].gid = 0;
    auth_strcpy(user_db[0].username, "root", USERNAME_MAX_LEN);
    auth_strcpy(user_db[0].full_name, "Root Administrator", FULLNAME_MAX_LEN);
    auth_strcpy(user_db[0].home_directory, "/root", 256);
    auth_strcpy(user_db[0].shell, "/bin/sh", 256);
    user_db[0].is_admin = true;
    user_db[0].is_locked = false;
    user_db[0].created_time = 0;
    user_db[0].last_login = 0;

    /* Default root password hash (empty for Phase 1) */
    auth_hash_password("", user_db[0].password_hash);

    user_count = 1;

    KLOG_INFO("AUTH", "Authentication system initialized");
    return STATUS_OK;
}

/* Hash password using SHA-256 */
void auth_hash_password(const char* password, char* hash_out) {
    uint8_t hash[32];
    size_t len = 0;

    /* Calculate password length */
    while (password[len]) len++;

    /* Compute SHA-256 hash */
    sha256_compute((const uint8_t*)password, len, hash);

    /* Convert to hex string */
    const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hash_out[i * 2] = hex[(hash[i] >> 4) & 0xF];
        hash_out[i * 2 + 1] = hex[hash[i] & 0xF];
    }
    hash_out[64] = '\0';
}

/* Verify password hash */
bool auth_verify_hash(const char* password, const char* hash) {
    char computed_hash[PASSWORD_HASH_LEN];
    auth_hash_password(password, computed_hash);
    return auth_strcmp(computed_hash, hash) == 0;
}

/* Create user */
status_t auth_create_user(const char* username, const char* password, const char* full_name, bool is_admin, uid_t* out_uid) {
    if (!username || !password || user_count >= MAX_USERS) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&auth_lock, 1);

    /* Check if username exists */
    for (uint32_t i = 0; i < MAX_USERS; i++) {
        if (user_db[i].uid != UID_INVALID && auth_strcmp(user_db[i].username, username) == 0) {
            __sync_lock_release(&auth_lock);
            return STATUS_EXISTS;
        }
    }

    /* Find free slot */
    uint32_t slot = 0;
    for (slot = 0; slot < MAX_USERS; slot++) {
        if (user_db[slot].uid == UID_INVALID) {
            break;
        }
    }

    if (slot >= MAX_USERS) {
        __sync_lock_release(&auth_lock);
        return STATUS_NOMEM;
    }

    /* Create user */
    user_db[slot].uid = next_uid++;
    user_db[slot].gid = user_db[slot].uid;
    auth_strcpy(user_db[slot].username, username, USERNAME_MAX_LEN);
    auth_hash_password(password, user_db[slot].password_hash);
    auth_strcpy(user_db[slot].full_name, full_name ? full_name : "", FULLNAME_MAX_LEN);

    /* Set home directory */
    char home[256] = "/home/";
    auth_strcpy(home + 6, username, 250);
    auth_strcpy(user_db[slot].home_directory, home, 256);

    auth_strcpy(user_db[slot].shell, "/bin/sh", 256);
    user_db[slot].is_admin = is_admin;
    user_db[slot].is_locked = false;
    user_db[slot].created_time = 0;  // Would use timer in production
    user_db[slot].last_login = 0;

    user_count++;

    if (out_uid) {
        *out_uid = user_db[slot].uid;
    }

    __sync_lock_release(&auth_lock);

    KLOG_INFO("AUTH", "User created: %s (UID %d)", username, user_db[slot].uid);
    return STATUS_OK;
}

/* Delete user */
status_t auth_delete_user(uid_t uid) {
    if (uid == UID_ROOT) {
        return STATUS_DENIED;  // Cannot delete root
    }

    __sync_lock_test_and_set(&auth_lock, 1);

    for (uint32_t i = 0; i < MAX_USERS; i++) {
        if (user_db[i].uid == uid) {
            user_db[i].uid = UID_INVALID;
            user_count--;
            __sync_lock_release(&auth_lock);
            return STATUS_OK;
        }
    }

    __sync_lock_release(&auth_lock);
    return STATUS_NOTFOUND;
}

/* Verify password and get UID */
status_t auth_verify_password(const char* username, const char* password, uid_t* out_uid) {
    if (!username || !password) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&auth_lock, 1);

    for (uint32_t i = 0; i < MAX_USERS; i++) {
        if (user_db[i].uid != UID_INVALID && auth_strcmp(user_db[i].username, username) == 0) {
            if (user_db[i].is_locked) {
                __sync_lock_release(&auth_lock);
                return STATUS_DENIED;
            }

            bool valid = auth_verify_hash(password, user_db[i].password_hash);
            if (valid) {
                if (out_uid) {
                    *out_uid = user_db[i].uid;
                }
                user_db[i].last_login = 0;  // Would use timer
                __sync_lock_release(&auth_lock);
                return STATUS_OK;
            }

            __sync_lock_release(&auth_lock);
            return STATUS_DENIED;
        }
    }

    __sync_lock_release(&auth_lock);
    return STATUS_NOTFOUND;
}

/* Get user information */
status_t auth_get_user(uid_t uid, user_t* out_user) {
    if (!out_user) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&auth_lock, 1);

    for (uint32_t i = 0; i < MAX_USERS; i++) {
        if (user_db[i].uid == uid) {
            *out_user = user_db[i];
            __sync_lock_release(&auth_lock);
            return STATUS_OK;
        }
    }

    __sync_lock_release(&auth_lock);
    return STATUS_NOTFOUND;
}

/* Find user by username */
status_t auth_find_user(const char* username, user_t* out_user) {
    if (!username || !out_user) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&auth_lock, 1);

    for (uint32_t i = 0; i < MAX_USERS; i++) {
        if (user_db[i].uid != UID_INVALID && auth_strcmp(user_db[i].username, username) == 0) {
            *out_user = user_db[i];
            __sync_lock_release(&auth_lock);
            return STATUS_OK;
        }
    }

    __sync_lock_release(&auth_lock);
    return STATUS_NOTFOUND;
}

/* Create authentication session */
status_t auth_create_session(uid_t uid, auth_session_t* out_session) {
    if (!out_session || session_count >= MAX_SESSIONS) {
        return STATUS_INVALID;
    }

    /* Verify user exists */
    user_t user;
    if (FAILED(auth_get_user(uid, &user))) {
        return STATUS_NOTFOUND;
    }

    __sync_lock_test_and_set(&auth_lock, 1);

    /* Find free slot */
    uint32_t slot = 0;
    for (slot = 0; slot < MAX_SESSIONS; slot++) {
        if (session_db[slot].session_id == 0) {
            break;
        }
    }

    if (slot >= MAX_SESSIONS) {
        __sync_lock_release(&auth_lock);
        return STATUS_NOMEM;
    }

    /* Create session */
    session_db[slot].session_id = next_session_id++;
    session_db[slot].uid = uid;
    session_db[slot].created_time = 0;
    session_db[slot].last_activity = 0;
    session_db[slot].flags = AUTH_SESSION_ACTIVE;

    *out_session = session_db[slot];
    session_count++;

    __sync_lock_release(&auth_lock);

    return STATUS_OK;
}

/* Destroy session */
status_t auth_destroy_session(uint64_t session_id) {
    __sync_lock_test_and_set(&auth_lock, 1);

    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        if (session_db[i].session_id == session_id) {
            session_db[i].session_id = 0;
            session_count--;
            __sync_lock_release(&auth_lock);
            return STATUS_OK;
        }
    }

    __sync_lock_release(&auth_lock);
    return STATUS_NOTFOUND;
}

/* Check if user is admin */
bool auth_is_admin(uid_t uid) {
    user_t user;
    if (SUCCESS(auth_get_user(uid, &user))) {
        return user.is_admin;
    }
    return false;
}

/* Change password */
status_t auth_change_password(uid_t uid, const char* old_password, const char* new_password) {
    if (!old_password || !new_password) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&auth_lock, 1);

    for (uint32_t i = 0; i < MAX_USERS; i++) {
        if (user_db[i].uid == uid) {
            if (!auth_verify_hash(old_password, user_db[i].password_hash)) {
                __sync_lock_release(&auth_lock);
                return STATUS_DENIED;
            }

            auth_hash_password(new_password, user_db[i].password_hash);
            __sync_lock_release(&auth_lock);
            return STATUS_OK;
        }
    }

    __sync_lock_release(&auth_lock);
    return STATUS_NOTFOUND;
}
