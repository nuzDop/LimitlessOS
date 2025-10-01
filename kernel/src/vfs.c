/*
 * Virtual File System Implementation
 * Unified filesystem interface
 */

#include "kernel.h"
#include "microkernel.h"
#include "vfs.h"

/* Global VFS state */
static struct {
    bool initialized;
    struct list_head filesystems;
    struct list_head mounts;
    vfs_mount_t* root_mount;
    uint32_t lock;
} vfs_state = {0};

/* String utilities */
static size_t vfs_strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

void vfs_strcpy(char* dest, const char* src, size_t max) {
    size_t i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int vfs_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int vfs_strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

/* Initialize VFS */
status_t vfs_init(void) {
    if (vfs_state.initialized) {
        return STATUS_EXISTS;
    }

    list_init(&vfs_state.filesystems);
    list_init(&vfs_state.mounts);
    vfs_state.root_mount = NULL;
    vfs_state.lock = 0;
    vfs_state.initialized = true;

    KLOG_INFO("VFS", "Virtual File System initialized");
    return STATUS_OK;
}

/* Shutdown VFS */
void vfs_shutdown(void) {
    if (!vfs_state.initialized) {
        return;
    }

    /* Unmount all filesystems */
    struct list_head* pos;
    struct list_head* tmp;
    list_for_each_safe(pos, tmp, &vfs_state.mounts) {
        vfs_mount_t* mount = list_entry(pos, vfs_mount_t, list_node);
        list_del(pos);
        if (mount->private_data) {
            pmm_free_page((paddr_t)mount->private_data);
        }
        pmm_free_page((paddr_t)mount);
    }

    vfs_state.initialized = false;
    KLOG_INFO("VFS", "Virtual File System shutdown");
}

/* Register filesystem */
status_t vfs_register_filesystem(vfs_filesystem_t* fs) {
    if (!fs || !fs->name[0] || !fs->ops) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&vfs_state.lock, 1);

    /* Check for duplicates */
    struct list_head* pos;
    list_for_each(pos, &vfs_state.filesystems) {
        vfs_filesystem_t* existing = list_entry(pos, vfs_filesystem_t, list_node);
        if (vfs_strcmp(existing->name, fs->name) == 0) {
            __sync_lock_release(&vfs_state.lock);
            return STATUS_EXISTS;
        }
    }

    list_add(&fs->list_node, &vfs_state.filesystems);
    __sync_lock_release(&vfs_state.lock);

    KLOG_INFO("VFS", "Registered filesystem: %s", fs->name);
    return STATUS_OK;
}

/* Find filesystem by name */
vfs_filesystem_t* vfs_find_filesystem(const char* name) {
    if (!name) {
        return NULL;
    }

    struct list_head* pos;
    list_for_each(pos, &vfs_state.filesystems) {
        vfs_filesystem_t* fs = list_entry(pos, vfs_filesystem_t, list_node);
        if (vfs_strcmp(fs->name, name) == 0) {
            return fs;
        }
    }

    return NULL;
}

/* Mount filesystem */
status_t vfs_mount(const char* device, const char* mount_point, const char* fs_type, uint32_t flags) {
    if (!mount_point || !fs_type) {
        return STATUS_INVALID;
    }

    /* Find filesystem type */
    vfs_filesystem_t* fs = vfs_find_filesystem(fs_type);
    if (!fs) {
        KLOG_ERROR("VFS", "Unknown filesystem type: %s", fs_type);
        return STATUS_NOTFOUND;
    }

    /* Allocate mount structure */
    vfs_mount_t* mount = (vfs_mount_t*)pmm_alloc_page();
    if (!mount) {
        return STATUS_NOMEM;
    }

    /* Initialize mount */
    vfs_strcpy(mount->mount_point, mount_point, VFS_MAX_PATH);
    if (device) {
        vfs_strcpy(mount->device, device, VFS_MAX_PATH);
    } else {
        mount->device[0] = '\0';
    }
    mount->fs = fs;
    mount->flags = flags;
    mount->private_data = NULL;
    mount->root = NULL;

    /* Call filesystem mount operation */
    if (fs->ops->mount) {
        status_t status = fs->ops->mount(mount, device, flags);
        if (FAILED(status)) {
            pmm_free_page((paddr_t)mount);
            KLOG_ERROR("VFS", "Failed to mount %s: %d", fs_type, status);
            return status;
        }
    }

    /* Get root node */
    if (fs->ops->get_root) {
        mount->root = fs->ops->get_root(mount);
    }

    __sync_lock_test_and_set(&vfs_state.lock, 1);

    /* Add to mount list */
    list_add(&mount->list_node, &vfs_state.mounts);

    /* Set as root mount if mounting at / */
    if (vfs_strcmp(mount_point, "/") == 0) {
        vfs_state.root_mount = mount;
    }

    __sync_lock_release(&vfs_state.lock);

    KLOG_INFO("VFS", "Mounted %s at %s (type: %s)", device ? device : "none", mount_point, fs_type);
    return STATUS_OK;
}

/* Find mount for path */
vfs_mount_t* vfs_find_mount(const char* path) {
    if (!path) {
        return NULL;
    }

    vfs_mount_t* best_match = NULL;
    size_t best_match_len = 0;

    struct list_head* pos;
    list_for_each(pos, &vfs_state.mounts) {
        vfs_mount_t* mount = list_entry(pos, vfs_mount_t, list_node);
        size_t mp_len = vfs_strlen(mount->mount_point);

        if (vfs_strncmp(path, mount->mount_point, mp_len) == 0) {
            if (mp_len > best_match_len) {
                best_match = mount;
                best_match_len = mp_len;
            }
        }
    }

    return best_match ? best_match : vfs_state.root_mount;
}

/* Normalize path (remove ., .., //) */
void vfs_normalize_path(const char* path, char* out_path) {
    if (!path || !out_path) {
        return;
    }

    char temp[VFS_MAX_PATH];
    char* segments[128];
    int segment_count = 0;

    /* Copy input */
    vfs_strcpy(temp, path, VFS_MAX_PATH);

    /* Split by / */
    char* p = temp;
    char* start = p;

    if (*p == '/') {
        p++;
        start = p;
    }

    while (*p) {
        if (*p == '/') {
            *p = '\0';
            if (start != p && vfs_strcmp(start, ".") != 0) {
                if (vfs_strcmp(start, "..") == 0) {
                    if (segment_count > 0) {
                        segment_count--;
                    }
                } else {
                    segments[segment_count++] = start;
                }
            }
            start = p + 1;
        }
        p++;
    }

    /* Handle last segment */
    if (start != p && vfs_strcmp(start, ".") != 0) {
        if (vfs_strcmp(start, "..") == 0) {
            if (segment_count > 0) {
                segment_count--;
            }
        } else {
            segments[segment_count++] = start;
        }
    }

    /* Build output path */
    if (segment_count == 0) {
        vfs_strcpy(out_path, "/", VFS_MAX_PATH);
    } else {
        out_path[0] = '/';
        out_path[1] = '\0';
        for (int i = 0; i < segment_count; i++) {
            if (i > 0) {
                out_path[vfs_strlen(out_path)] = '/';
                out_path[vfs_strlen(out_path) + 1] = '\0';
            }
            vfs_strcpy(out_path + vfs_strlen(out_path), segments[i], VFS_MAX_PATH - vfs_strlen(out_path));
        }
    }
}

/* Resolve path to node */
status_t vfs_resolve_path(const char* path, vfs_node_t** out_node) {
    if (!path || !out_node) {
        return STATUS_INVALID;
    }

    char normalized[VFS_MAX_PATH];
    vfs_normalize_path(path, normalized);

    /* Find appropriate mount */
    vfs_mount_t* mount = vfs_find_mount(normalized);
    if (!mount || !mount->root) {
        return STATUS_NOTFOUND;
    }

    /* If path is mount point, return root */
    if (vfs_strcmp(normalized, mount->mount_point) == 0 ||
        vfs_strcmp(normalized, "/") == 0) {
        *out_node = mount->root;
        vfs_node_ref(mount->root);
        return STATUS_OK;
    }

    /* Walk path components */
    vfs_node_t* current = mount->root;
    vfs_node_ref(current);

    char* p = normalized + vfs_strlen(mount->mount_point);
    if (*p == '/') p++;

    char component[VFS_MAX_NAME];
    int comp_idx = 0;

    while (*p) {
        if (*p == '/' || *p == '\0') {
            if (comp_idx > 0) {
                component[comp_idx] = '\0';

                /* Lookup component */
                if (!current->dir_ops || !current->dir_ops->lookup) {
                    vfs_node_unref(current);
                    return STATUS_NOTFOUND;
                }

                vfs_node_t* next = NULL;
                status_t status = current->dir_ops->lookup(current, component, &next);
                vfs_node_unref(current);

                if (FAILED(status) || !next) {
                    return STATUS_NOTFOUND;
                }

                current = next;
                comp_idx = 0;
            }

            if (*p == '\0') break;
            p++;
        } else {
            if (comp_idx < VFS_MAX_NAME - 1) {
                component[comp_idx++] = *p;
            }
            p++;
        }
    }

    *out_node = current;
    return STATUS_OK;
}

/* Allocate node */
vfs_node_t* vfs_node_alloc(void) {
    vfs_node_t* node = (vfs_node_t*)pmm_alloc_page();
    if (!node) {
        return NULL;
    }

    node->inode = 0;
    node->type = VFS_TYPE_FILE;
    node->mode = 0;
    node->uid = 0;
    node->gid = 0;
    node->size = 0;
    node->atime = 0;
    node->mtime = 0;
    node->ctime = 0;
    node->nlink = 1;
    node->ref_count = 1;
    node->file_ops = NULL;
    node->dir_ops = NULL;
    node->fs = NULL;
    node->mount = NULL;
    node->private_data = NULL;
    node->lock = 0;

    return node;
}

/* Free node */
void vfs_node_free(vfs_node_t* node) {
    if (!node) {
        return;
    }

    if (node->private_data) {
        pmm_free_page((paddr_t)node->private_data);
    }

    pmm_free_page((paddr_t)node);
}

/* Reference node */
void vfs_node_ref(vfs_node_t* node) {
    if (node) {
        __sync_fetch_and_add(&node->ref_count, 1);
    }
}

/* Unreference node */
void vfs_node_unref(vfs_node_t* node) {
    if (!node) {
        return;
    }

    if (__sync_sub_and_fetch(&node->ref_count, 1) == 0) {
        vfs_node_free(node);
    }
}

/* Open file */
status_t vfs_open(const char* path, uint32_t flags, uint32_t mode, vfs_file_t** out_file) {
    if (!path || !out_file) {
        return STATUS_INVALID;
    }

    vfs_node_t* node = NULL;
    status_t status = vfs_resolve_path(path, &node);

    /* Handle O_CREAT */
    if (FAILED(status) && (flags & VFS_O_CREAT)) {
        /* Extract parent directory and filename */
        const char* last_slash = path;
        for (const char* p = path; *p; p++) {
            if (*p == '/') last_slash = p;
        }

        /* Get parent directory */
        char parent_path[256] = {0};
        size_t parent_len = last_slash - path;
        if (parent_len == 0) {
            parent_path[0] = '/';
            parent_path[1] = '\0';
        } else {
            for (size_t i = 0; i < parent_len && i < 255; i++) {
                parent_path[i] = path[i];
            }
        }

        const char* filename = last_slash + 1;

        /* Resolve parent directory */
        vfs_node_t* parent = NULL;
        status = vfs_resolve_path(parent_path, &parent);
        if (FAILED(status)) {
            return STATUS_NOTFOUND;
        }

        /* Create file in parent directory */
        if (parent->dir_ops && parent->dir_ops->create) {
            status = parent->dir_ops->create(parent, filename, mode, &node);
            vfs_node_unref(parent);

            if (FAILED(status)) {
                return status;
            }

            /* node is now the newly created file */
        } else {
            vfs_node_unref(parent);
            return STATUS_NOSUPPORT;
        }
    }

    if (FAILED(status)) {
        return status;
    }

    /* Call node open if available */
    if (node->file_ops && node->file_ops->open) {
        status = node->file_ops->open(node, flags);
        if (FAILED(status)) {
            vfs_node_unref(node);
            return status;
        }
    }

    /* Allocate file descriptor */
    vfs_file_t* file = (vfs_file_t*)pmm_alloc_page();
    if (!file) {
        vfs_node_unref(node);
        return STATUS_NOMEM;
    }

    file->node = node;
    file->offset = 0;
    file->flags = flags;
    file->ref_count = 1;

    *out_file = file;
    return STATUS_OK;
}

/* Close file */
status_t vfs_close(vfs_file_t* file) {
    if (!file) {
        return STATUS_INVALID;
    }

    if (file->node) {
        if (file->node->file_ops && file->node->file_ops->close) {
            file->node->file_ops->close(file->node);
        }
        vfs_node_unref(file->node);
    }

    pmm_free_page((paddr_t)file);
    return STATUS_OK;
}

/* Read from file */
ssize_t vfs_read(vfs_file_t* file, void* buffer, size_t size) {
    if (!file || !file->node || !buffer) {
        return -1;
    }

    if (!file->node->file_ops || !file->node->file_ops->read) {
        return -1;
    }

    ssize_t result = file->node->file_ops->read(file->node, buffer, size, file->offset);
    if (result > 0) {
        file->offset += result;
    }

    return result;
}

/* Write to file */
ssize_t vfs_write(vfs_file_t* file, const void* buffer, size_t size) {
    if (!file || !file->node || !buffer) {
        return -1;
    }

    if (!file->node->file_ops || !file->node->file_ops->write) {
        return -1;
    }

    ssize_t result = file->node->file_ops->write(file->node, buffer, size, file->offset);
    if (result > 0) {
        file->offset += result;
    }

    return result;
}

/* File statistics */
status_t vfs_fstat(vfs_file_t* file, vfs_stat_t* stat) {
    if (!file || !file->node || !stat) {
        return STATUS_INVALID;
    }

    stat->inode = file->node->inode;
    stat->mode = file->node->mode;
    stat->type = file->node->type;
    stat->uid = file->node->uid;
    stat->gid = file->node->gid;
    stat->size = file->node->size;
    stat->atime = file->node->atime;
    stat->mtime = file->node->mtime;
    stat->ctime = file->node->ctime;
    stat->nlink = file->node->nlink;
    stat->blocks = (file->node->size + 4095) / 4096;
    stat->block_size = 4096;

    return STATUS_OK;
}

/* Check if path is absolute */
bool vfs_is_absolute_path(const char* path) {
    return path && path[0] == '/';
}
