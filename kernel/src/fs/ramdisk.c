/*
 * Ramdisk Filesystem
 * Simple in-memory filesystem for boot and temporary storage
 */

#include "kernel.h"
#include "microkernel.h"
#include "vfs.h"

#define RAMDISK_MAX_FILES 1024
#define RAMDISK_MAX_FILE_SIZE (16 * 1024 * 1024)  // 16MB per file
#define RAMDISK_BLOCK_SIZE 4096

/* Ramdisk file entry */
typedef struct ramdisk_file {
    uint64_t inode;
    uint32_t type;
    char name[VFS_MAX_NAME];
    uint8_t* data;
    uint64_t size;
    uint64_t capacity;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t parent_inode;
    bool in_use;
} ramdisk_file_t;

/* Ramdisk filesystem state */
typedef struct ramdisk_fs {
    ramdisk_file_t files[RAMDISK_MAX_FILES];
    uint64_t next_inode;
    uint32_t file_count;
    uint32_t lock;
} ramdisk_fs_t;

/* Forward declarations */
static ssize_t ramdisk_read(vfs_node_t* node, void* buffer, size_t size, uint64_t offset);
static ssize_t ramdisk_write(vfs_node_t* node, const void* buffer, size_t size, uint64_t offset);
static status_t ramdisk_open(vfs_node_t* node, uint32_t flags);
static status_t ramdisk_close(vfs_node_t* node);
static status_t ramdisk_truncate(vfs_node_t* node, uint64_t size);

static status_t ramdisk_lookup(vfs_node_t* dir, const char* name, vfs_node_t** out_node);
static status_t ramdisk_create(vfs_node_t* dir, const char* name, uint32_t mode, vfs_node_t** out_node);
static status_t ramdisk_mkdir(vfs_node_t* dir, const char* name, uint32_t mode);
static status_t ramdisk_unlink(vfs_node_t* dir, const char* name);
static status_t ramdisk_readdir(vfs_node_t* dir, vfs_dirent_t* dirent, uint64_t index);

static status_t ramdisk_mount(vfs_mount_t* mount, const char* device, uint32_t flags);
static vfs_node_t* ramdisk_get_root(vfs_mount_t* mount);

/* File operations */
static vfs_file_ops_t ramdisk_file_ops = {
    .read = ramdisk_read,
    .write = ramdisk_write,
    .open = ramdisk_open,
    .close = ramdisk_close,
    .truncate = ramdisk_truncate,
    .ioctl = NULL,
    .flush = NULL,
};

/* Directory operations */
static vfs_dir_ops_t ramdisk_dir_ops = {
    .lookup = ramdisk_lookup,
    .create = ramdisk_create,
    .mkdir = ramdisk_mkdir,
    .rmdir = ramdisk_unlink,
    .unlink = ramdisk_unlink,
    .rename = NULL,
    .readdir = ramdisk_readdir,
};

/* Filesystem operations */
static vfs_fs_ops_t ramdisk_fs_ops = {
    .mount = ramdisk_mount,
    .unmount = NULL,
    .sync = NULL,
    .statfs = NULL,
    .get_root = ramdisk_get_root,
};

/* Filesystem type */
static vfs_filesystem_t ramdisk_filesystem = {
    .name = "ramdisk",
    .ops = &ramdisk_fs_ops,
};

/* Find file by inode */
static ramdisk_file_t* ramdisk_find_by_inode(ramdisk_fs_t* fs, uint64_t inode) {
    for (uint32_t i = 0; i < RAMDISK_MAX_FILES; i++) {
        if (fs->files[i].in_use && fs->files[i].inode == inode) {
            return &fs->files[i];
        }
    }
    return NULL;
}

/* Find file in directory by name */
static ramdisk_file_t* ramdisk_find_in_dir(ramdisk_fs_t* fs, uint64_t parent_inode, const char* name) {
    for (uint32_t i = 0; i < RAMDISK_MAX_FILES; i++) {
        if (fs->files[i].in_use &&
            fs->files[i].parent_inode == parent_inode &&
            vfs_strcmp(fs->files[i].name, name) == 0) {
            return &fs->files[i];
        }
    }
    return NULL;
}

/* Allocate file entry */
static ramdisk_file_t* ramdisk_alloc_file(ramdisk_fs_t* fs) {
    for (uint32_t i = 0; i < RAMDISK_MAX_FILES; i++) {
        if (!fs->files[i].in_use) {
            ramdisk_file_t* file = &fs->files[i];
            file->inode = fs->next_inode++;
            file->in_use = true;
            file->data = NULL;
            file->size = 0;
            file->capacity = 0;
            fs->file_count++;
            return file;
        }
    }
    return NULL;
}

/* Create VFS node from ramdisk file */
static vfs_node_t* ramdisk_create_node(ramdisk_file_t* file, vfs_mount_t* mount) {
    vfs_node_t* node = vfs_node_alloc();
    if (!node) {
        return NULL;
    }

    node->inode = file->inode;
    node->type = file->type;
    node->mode = file->mode;
    node->uid = file->uid;
    node->gid = file->gid;
    node->size = file->size;
    node->nlink = 1;
    node->mount = mount;
    node->private_data = file;

    if (file->type == VFS_TYPE_DIR) {
        node->dir_ops = &ramdisk_dir_ops;
    } else {
        node->file_ops = &ramdisk_file_ops;
    }

    return node;
}

/* Mount ramdisk */
static status_t ramdisk_mount(vfs_mount_t* mount, const char* device, uint32_t flags) {
    /* Allocate filesystem state */
    ramdisk_fs_t* fs = (ramdisk_fs_t*)pmm_alloc_pages(
        (sizeof(ramdisk_fs_t) + PAGE_SIZE - 1) / PAGE_SIZE
    );
    if (!fs) {
        return STATUS_NOMEM;
    }

    /* Initialize filesystem */
    for (uint32_t i = 0; i < RAMDISK_MAX_FILES; i++) {
        fs->files[i].in_use = false;
    }
    fs->next_inode = 1;
    fs->file_count = 0;
    fs->lock = 0;

    /* Create root directory */
    ramdisk_file_t* root = ramdisk_alloc_file(fs);
    if (!root) {
        pmm_free_pages((paddr_t)fs, (sizeof(ramdisk_fs_t) + PAGE_SIZE - 1) / PAGE_SIZE);
        return STATUS_NOMEM;
    }

    root->type = VFS_TYPE_DIR;
    root->name[0] = '/';
    root->name[1] = '\0';
    root->mode = 0755;
    root->uid = 0;
    root->gid = 0;
    root->parent_inode = 0;

    mount->private_data = fs;

    KLOG_INFO("RAMDISK", "Mounted ramdisk filesystem");
    return STATUS_OK;
}

/* Get root node */
static vfs_node_t* ramdisk_get_root(vfs_mount_t* mount) {
    if (!mount || !mount->private_data) {
        return NULL;
    }

    ramdisk_fs_t* fs = (ramdisk_fs_t*)mount->private_data;
    ramdisk_file_t* root = ramdisk_find_by_inode(fs, 1);

    if (!root) {
        return NULL;
    }

    return ramdisk_create_node(root, mount);
}

/* Read from file */
static ssize_t ramdisk_read(vfs_node_t* node, void* buffer, size_t size, uint64_t offset) {
    if (!node || !buffer) {
        return -1;
    }

    ramdisk_file_t* file = (ramdisk_file_t*)node->private_data;
    if (!file || !file->data) {
        return 0;
    }

    if (offset >= file->size) {
        return 0;
    }

    size_t to_read = size;
    if (offset + to_read > file->size) {
        to_read = file->size - offset;
    }

    uint8_t* src = file->data + offset;
    uint8_t* dest = (uint8_t*)buffer;
    for (size_t i = 0; i < to_read; i++) {
        dest[i] = src[i];
    }

    return to_read;
}

/* Write to file */
static ssize_t ramdisk_write(vfs_node_t* node, const void* buffer, size_t size, uint64_t offset) {
    if (!node || !buffer) {
        return -1;
    }

    ramdisk_file_t* file = (ramdisk_file_t*)node->private_data;
    if (!file) {
        return -1;
    }

    /* Check size limit */
    if (offset + size > RAMDISK_MAX_FILE_SIZE) {
        return -1;
    }

    /* Expand if needed */
    if (offset + size > file->capacity) {
        uint64_t new_capacity = ((offset + size + RAMDISK_BLOCK_SIZE - 1) / RAMDISK_BLOCK_SIZE) * RAMDISK_BLOCK_SIZE;
        uint8_t* new_data = (uint8_t*)pmm_alloc_pages((new_capacity + PAGE_SIZE - 1) / PAGE_SIZE);

        if (!new_data) {
            return -1;
        }

        if (file->data) {
            /* Copy old data */
            for (uint64_t i = 0; i < file->size; i++) {
                new_data[i] = file->data[i];
            }
            pmm_free_pages((paddr_t)file->data, (file->capacity + PAGE_SIZE - 1) / PAGE_SIZE);
        }

        file->data = new_data;
        file->capacity = new_capacity;
    }

    /* Write data */
    const uint8_t* src = (const uint8_t*)buffer;
    uint8_t* dest = file->data + offset;
    for (size_t i = 0; i < size; i++) {
        dest[i] = src[i];
    }

    if (offset + size > file->size) {
        file->size = offset + size;
        node->size = file->size;
    }

    return size;
}

/* Open file */
static status_t ramdisk_open(vfs_node_t* node, uint32_t flags) {
    return STATUS_OK;
}

/* Close file */
static status_t ramdisk_close(vfs_node_t* node) {
    return STATUS_OK;
}

/* Truncate file */
static status_t ramdisk_truncate(vfs_node_t* node, uint64_t size) {
    if (!node) {
        return STATUS_INVALID;
    }

    ramdisk_file_t* file = (ramdisk_file_t*)node->private_data;
    if (!file) {
        return STATUS_INVALID;
    }

    if (size > file->capacity) {
        return STATUS_INVALID;
    }

    file->size = size;
    node->size = size;

    return STATUS_OK;
}

/* Lookup file in directory */
static status_t ramdisk_lookup(vfs_node_t* dir, const char* name, vfs_node_t** out_node) {
    if (!dir || !name || !out_node) {
        return STATUS_INVALID;
    }

    ramdisk_file_t* dir_file = (ramdisk_file_t*)dir->private_data;
    if (!dir_file) {
        return STATUS_INVALID;
    }

    ramdisk_fs_t* fs = (ramdisk_fs_t*)dir->mount->private_data;
    ramdisk_file_t* file = ramdisk_find_in_dir(fs, dir_file->inode, name);

    if (!file) {
        return STATUS_NOTFOUND;
    }

    *out_node = ramdisk_create_node(file, dir->mount);
    return STATUS_OK;
}

/* Create file */
static status_t ramdisk_create(vfs_node_t* dir, const char* name, uint32_t mode, vfs_node_t** out_node) {
    if (!dir || !name || !out_node) {
        return STATUS_INVALID;
    }

    ramdisk_file_t* dir_file = (ramdisk_file_t*)dir->private_data;
    if (!dir_file) {
        return STATUS_INVALID;
    }

    ramdisk_fs_t* fs = (ramdisk_fs_t*)dir->mount->private_data;

    /* Check if already exists */
    if (ramdisk_find_in_dir(fs, dir_file->inode, name)) {
        return STATUS_EXISTS;
    }

    /* Allocate new file */
    ramdisk_file_t* file = ramdisk_alloc_file(fs);
    if (!file) {
        return STATUS_NOMEM;
    }

    file->type = VFS_TYPE_FILE;
    vfs_strcpy(file->name, name, VFS_MAX_NAME);
    file->mode = mode;
    file->uid = 0;
    file->gid = 0;
    file->parent_inode = dir_file->inode;

    *out_node = ramdisk_create_node(file, dir->mount);
    return STATUS_OK;
}

/* Create directory */
static status_t ramdisk_mkdir(vfs_node_t* dir, const char* name, uint32_t mode) {
    if (!dir || !name) {
        return STATUS_INVALID;
    }

    ramdisk_file_t* dir_file = (ramdisk_file_t*)dir->private_data;
    if (!dir_file) {
        return STATUS_INVALID;
    }

    ramdisk_fs_t* fs = (ramdisk_fs_t*)dir->mount->private_data;

    /* Check if already exists */
    if (ramdisk_find_in_dir(fs, dir_file->inode, name)) {
        return STATUS_EXISTS;
    }

    /* Allocate new directory */
    ramdisk_file_t* new_dir = ramdisk_alloc_file(fs);
    if (!new_dir) {
        return STATUS_NOMEM;
    }

    new_dir->type = VFS_TYPE_DIR;
    vfs_strcpy(new_dir->name, name, VFS_MAX_NAME);
    new_dir->mode = mode;
    new_dir->uid = 0;
    new_dir->gid = 0;
    new_dir->parent_inode = dir_file->inode;

    return STATUS_OK;
}

/* Remove file */
static status_t ramdisk_unlink(vfs_node_t* dir, const char* name) {
    if (!dir || !name) {
        return STATUS_INVALID;
    }

    ramdisk_file_t* dir_file = (ramdisk_file_t*)dir->private_data;
    if (!dir_file) {
        return STATUS_INVALID;
    }

    ramdisk_fs_t* fs = (ramdisk_fs_t*)dir->mount->private_data;
    ramdisk_file_t* file = ramdisk_find_in_dir(fs, dir_file->inode, name);

    if (!file) {
        return STATUS_NOTFOUND;
    }

    /* Free data */
    if (file->data) {
        pmm_free_pages((paddr_t)file->data, (file->capacity + PAGE_SIZE - 1) / PAGE_SIZE);
    }

    file->in_use = false;
    fs->file_count--;

    return STATUS_OK;
}

/* Read directory */
static status_t ramdisk_readdir(vfs_node_t* dir, vfs_dirent_t* dirent, uint64_t index) {
    if (!dir || !dirent) {
        return STATUS_INVALID;
    }

    ramdisk_file_t* dir_file = (ramdisk_file_t*)dir->private_data;
    if (!dir_file) {
        return STATUS_INVALID;
    }

    ramdisk_fs_t* fs = (ramdisk_fs_t*)dir->mount->private_data;

    uint64_t current = 0;
    for (uint32_t i = 0; i < RAMDISK_MAX_FILES; i++) {
        if (fs->files[i].in_use && fs->files[i].parent_inode == dir_file->inode) {
            if (current == index) {
                dirent->inode = fs->files[i].inode;
                dirent->type = fs->files[i].type;
                vfs_strcpy(dirent->name, fs->files[i].name, VFS_MAX_NAME);
                return STATUS_OK;
            }
            current++;
        }
    }

    return STATUS_NOTFOUND;
}

/* Register ramdisk filesystem */
status_t ramdisk_register(void) {
    return vfs_register_filesystem(&ramdisk_filesystem);
}
