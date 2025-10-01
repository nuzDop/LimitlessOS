#ifndef LIMITLESS_VFS_H
#define LIMITLESS_VFS_H

/*
 * Virtual File System (VFS)
 * Unified interface for all filesystems
 */

#include "kernel.h"

/* File types */
#define VFS_TYPE_FILE       0x1
#define VFS_TYPE_DIR        0x2
#define VFS_TYPE_CHARDEV    0x3
#define VFS_TYPE_BLOCKDEV   0x4
#define VFS_TYPE_FIFO       0x5
#define VFS_TYPE_SYMLINK    0x6
#define VFS_TYPE_SOCKET     0x7

/* File permissions */
#define VFS_PERM_RUSR  0400
#define VFS_PERM_WUSR  0200
#define VFS_PERM_XUSR  0100
#define VFS_PERM_RGRP  0040
#define VFS_PERM_WGRP  0020
#define VFS_PERM_XGRP  0010
#define VFS_PERM_ROTH  0004
#define VFS_PERM_WOTH  0002
#define VFS_PERM_XOTH  0001

/* Open flags */
#define VFS_O_RDONLY   0x0000
#define VFS_O_WRONLY   0x0001
#define VFS_O_RDWR     0x0002
#define VFS_O_CREAT    0x0040
#define VFS_O_EXCL     0x0080
#define VFS_O_TRUNC    0x0200
#define VFS_O_APPEND   0x0400
#define VFS_O_NONBLOCK 0x0800
#define VFS_O_DIRECTORY 0x10000

/* Seek whence */
#define VFS_SEEK_SET   0
#define VFS_SEEK_CUR   1
#define VFS_SEEK_END   2

/* Maximum path length */
#define VFS_MAX_PATH 4096
#define VFS_MAX_NAME 256

/* Forward declarations */
typedef struct vfs_node vfs_node_t;
typedef struct vfs_mount vfs_mount_t;
typedef struct vfs_filesystem vfs_filesystem_t;

/* File statistics */
typedef struct vfs_stat {
    uint64_t inode;
    uint32_t mode;
    uint32_t type;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t blocks;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t nlink;
    uint32_t block_size;
} vfs_stat_t;

/* Directory entry */
typedef struct vfs_dirent {
    uint64_t inode;
    uint32_t type;
    char name[VFS_MAX_NAME];
} vfs_dirent_t;

/* File operations */
typedef struct vfs_file_ops {
    ssize_t (*read)(vfs_node_t* node, void* buffer, size_t size, uint64_t offset);
    ssize_t (*write)(vfs_node_t* node, const void* buffer, size_t size, uint64_t offset);
    status_t (*open)(vfs_node_t* node, uint32_t flags);
    status_t (*close)(vfs_node_t* node);
    status_t (*ioctl)(vfs_node_t* node, uint32_t request, void* arg);
    status_t (*truncate)(vfs_node_t* node, uint64_t size);
    status_t (*flush)(vfs_node_t* node);
} vfs_file_ops_t;

/* Directory operations */
typedef struct vfs_dir_ops {
    status_t (*lookup)(vfs_node_t* dir, const char* name, vfs_node_t** out_node);
    status_t (*create)(vfs_node_t* dir, const char* name, uint32_t mode, vfs_node_t** out_node);
    status_t (*mkdir)(vfs_node_t* dir, const char* name, uint32_t mode);
    status_t (*rmdir)(vfs_node_t* dir, const char* name);
    status_t (*unlink)(vfs_node_t* dir, const char* name);
    status_t (*rename)(vfs_node_t* old_dir, const char* old_name, vfs_node_t* new_dir, const char* new_name);
    status_t (*readdir)(vfs_node_t* dir, vfs_dirent_t* dirent, uint64_t index);
} vfs_dir_ops_t;

/* VFS node (inode) */
struct vfs_node {
    uint64_t inode;
    uint32_t type;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t nlink;
    uint32_t ref_count;

    /* Operations */
    vfs_file_ops_t* file_ops;
    vfs_dir_ops_t* dir_ops;

    /* Filesystem and mount */
    vfs_filesystem_t* fs;
    vfs_mount_t* mount;

    /* Private data for filesystem driver */
    void* private_data;

    /* Lock */
    uint32_t lock;

    /* List linkage */
    struct list_head list_node;
};

/* Filesystem operations */
typedef struct vfs_fs_ops {
    status_t (*mount)(vfs_mount_t* mount, const char* device, uint32_t flags);
    status_t (*unmount)(vfs_mount_t* mount);
    status_t (*sync)(vfs_mount_t* mount);
    status_t (*statfs)(vfs_mount_t* mount, void* statbuf);
    vfs_node_t* (*get_root)(vfs_mount_t* mount);
} vfs_fs_ops_t;

/* Filesystem type */
struct vfs_filesystem {
    char name[64];
    vfs_fs_ops_t* ops;
    struct list_head list_node;
};

/* Mount point */
struct vfs_mount {
    char mount_point[VFS_MAX_PATH];
    char device[VFS_MAX_PATH];
    vfs_filesystem_t* fs;
    vfs_node_t* root;
    uint32_t flags;
    void* private_data;
    struct list_head list_node;
};

/* Open file descriptor */
typedef struct vfs_file {
    vfs_node_t* node;
    uint64_t offset;
    uint32_t flags;
    uint32_t ref_count;
} vfs_file_t;

/* VFS initialization */
status_t vfs_init(void);
void vfs_shutdown(void);

/* Filesystem registration */
status_t vfs_register_filesystem(vfs_filesystem_t* fs);
status_t vfs_unregister_filesystem(const char* name);
vfs_filesystem_t* vfs_find_filesystem(const char* name);

/* Mount operations */
status_t vfs_mount(const char* device, const char* mount_point, const char* fs_type, uint32_t flags);
status_t vfs_unmount(const char* mount_point);
vfs_mount_t* vfs_find_mount(const char* path);

/* Path resolution */
status_t vfs_resolve_path(const char* path, vfs_node_t** out_node);
status_t vfs_resolve_parent(const char* path, vfs_node_t** out_parent, char** out_name);

/* File operations */
status_t vfs_open(const char* path, uint32_t flags, uint32_t mode, vfs_file_t** out_file);
status_t vfs_close(vfs_file_t* file);
ssize_t vfs_read(vfs_file_t* file, void* buffer, size_t size);
ssize_t vfs_write(vfs_file_t* file, const void* buffer, size_t size);
status_t vfs_seek(vfs_file_t* file, int64_t offset, int whence, uint64_t* out_offset);
status_t vfs_stat(const char* path, vfs_stat_t* stat);
status_t vfs_fstat(vfs_file_t* file, vfs_stat_t* stat);

/* Directory operations */
status_t vfs_mkdir(const char* path, uint32_t mode);
status_t vfs_rmdir(const char* path);
status_t vfs_readdir(vfs_file_t* dir, vfs_dirent_t* dirent, uint64_t index);

/* File management */
status_t vfs_unlink(const char* path);
status_t vfs_rename(const char* old_path, const char* new_path);
status_t vfs_truncate(const char* path, uint64_t size);

/* Node operations */
vfs_node_t* vfs_node_alloc(void);
void vfs_node_free(vfs_node_t* node);
void vfs_node_ref(vfs_node_t* node);
void vfs_node_unref(vfs_node_t* node);

/* Utilities */
void vfs_normalize_path(const char* path, char* out_path);
status_t vfs_split_path(const char* path, char* dir, char* name);
bool vfs_is_absolute_path(const char* path);

/* String utilities (internal use by filesystems) */
int vfs_strcmp(const char* s1, const char* s2);
void vfs_strcpy(char* dest, const char* src, size_t max);

#endif /* LIMITLESS_VFS_H */
