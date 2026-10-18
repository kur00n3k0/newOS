#include "fs.h"
#include "heap.h"
#include "string.h"

static struct fs_node* root;
static struct fs_node* cwd;

static struct fs_node* node_new(const char* name, enum fs_type type) {
    struct fs_node* n = kcalloc(1, sizeof(*n));
    if (!n) {
        return NULL;
    }
    size_t len = strlen(name);
    memcpy(n->name, name, len);
    n->type = type;
    return n;
}

static void dir_insert(struct fs_node* dir, struct fs_node* n) {
    struct fs_node** link = &dir->child;
    while (*link && strcmp((*link)->name, n->name) < 0) {
        link = &(*link)->next;
    }
    n->next = *link;
    n->parent = dir;
    *link = n;
}

static void dir_unlink(struct fs_node* n) {
    struct fs_node** link = &n->parent->child;
    while (*link != n) {
        link = &(*link)->next;
    }
    *link = n->next;
}

static struct fs_node* dir_find(struct fs_node* dir, const char* name, size_t len) {
    for (struct fs_node* c = dir->child; c; c = c->next) {
        if (strlen(c->name) == len && memcmp(c->name, name, len) == 0) {
            return c;
        }
    }
    return NULL;
}

static int valid_name(const char* name, size_t len) {
    if (len == 0 || len >= FS_NAME_MAX) {
        return 0;
    }
    if ((len == 1 && name[0] == '.') || (len == 2 && name[0] == '.' && name[1] == '.')) {
        return 0;
    }
    return 1;
}

// Walks `len` bytes of `path` starting at the root or cwd.
static int walk(const char* path, size_t len, struct fs_node** out) {
    struct fs_node* cur = (len > 0 && path[0] == '/') ? root : cwd;
    size_t i = 0;

    while (i < len) {
        while (i < len && path[i] == '/') {
            i++;
        }
        size_t start = i;
        while (i < len && path[i] != '/') {
            i++;
        }
        size_t n = i - start;
        if (n == 0 || (n == 1 && path[start] == '.')) {
            continue;
        }
        if (cur->type != FS_DIR) {
            return FS_ENOTDIR;
        }
        if (n == 2 && path[start] == '.' && path[start + 1] == '.') {
            if (cur->parent) {
                cur = cur->parent;
            }
            continue;
        }
        cur = dir_find(cur, path + start, n);
        if (!cur) {
            return FS_ENOENT;
        }
    }

    *out = cur;
    return FS_OK;
}

// Splits "a/b/name" into the directory node "a/b" and the final component.
static int split_parent(const char* path, struct fs_node** dir, const char** name, size_t* name_len) {
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        len--;
    }

    size_t slash = len;
    while (slash > 0 && path[slash - 1] != '/') {
        slash--;
    }

    *name = path + slash;
    *name_len = len - slash;
    if (!valid_name(*name, *name_len)) {
        return FS_EINVAL;
    }

    int err = walk(path, slash, dir);
    if (err) {
        return err;
    }
    return (*dir)->type == FS_DIR ? FS_OK : FS_ENOTDIR;
}

void fs_init(void) {
    root = node_new("", FS_DIR);
    cwd = root;
}

struct fs_node* fs_root(void) { return root; }
struct fs_node* fs_cwd(void) { return cwd; }

int fs_lookup(const char* path, struct fs_node** out) {
    return walk(path, strlen(path), out);
}

int fs_chdir(const char* path) {
    struct fs_node* n;
    int err = fs_lookup(path, &n);
    if (err) {
        return err;
    }
    if (n->type != FS_DIR) {
        return FS_ENOTDIR;
    }
    cwd = n;
    return FS_OK;
}

int fs_mkdir(const char* path) {
    struct fs_node* dir;
    const char* name;
    size_t len;
    int err = split_parent(path, &dir, &name, &len);
    if (err) {
        return err;
    }
    if (dir_find(dir, name, len)) {
        return FS_EEXIST;
    }

    char tmp[FS_NAME_MAX];
    memcpy(tmp, name, len);
    tmp[len] = '\0';
    struct fs_node* n = node_new(tmp, FS_DIR);
    if (!n) {
        return FS_ENOMEM;
    }
    dir_insert(dir, n);
    return FS_OK;
}

int fs_create(const char* path, struct fs_node** out) {
    struct fs_node* dir;
    const char* name;
    size_t len;
    int err = split_parent(path, &dir, &name, &len);
    if (err) {
        return err;
    }

    struct fs_node* existing = dir_find(dir, name, len);
    if (existing) {
        if (existing->type == FS_DIR) {
            return FS_EISDIR;
        }
        *out = existing;
        return FS_OK;
    }

    char tmp[FS_NAME_MAX];
    memcpy(tmp, name, len);
    tmp[len] = '\0';
    struct fs_node* n = node_new(tmp, FS_FILE);
    if (!n) {
        return FS_ENOMEM;
    }
    dir_insert(dir, n);
    *out = n;
    return FS_OK;
}

int fs_remove(const char* path) {
    struct fs_node* n;
    int err = fs_lookup(path, &n);
    if (err) {
        return err;
    }
    if (n == root || n == cwd) {
        return FS_EBUSY;
    }
    if (n->type == FS_DIR && n->child) {
        return FS_ENOTEMPTY;
    }

    dir_unlink(n);
    kfree(n->data);
    kfree(n);
    return FS_OK;
}

int fs_write(struct fs_node* file, const void* data, size_t len, int append) {
    if (file->type != FS_FILE) {
        return FS_EISDIR;
    }

    size_t start = append ? file->size : 0;
    size_t needed = start + len;

    if (needed > file->capacity) {
        size_t cap = file->capacity ? file->capacity : 64;
        while (cap < needed) {
            cap *= 2;
        }
        uint8_t* buf = kmalloc(cap);
        if (!buf) {
            return FS_ENOMEM;
        }
        memcpy(buf, file->data, file->size);
        kfree(file->data);
        file->data = buf;
        file->capacity = cap;
    }

    memcpy(file->data + start, data, len);
    file->size = needed;
    return FS_OK;
}

void fs_path(const struct fs_node* node, char* buf, size_t size) {
    if (!node->parent) {
        buf[0] = '/';
        buf[1] = '\0';
        return;
    }

    // build right-to-left, then shift into place
    char tmp[256];
    size_t pos = sizeof(tmp) - 1;
    tmp[pos] = '\0';
    for (; node->parent; node = node->parent) {
        size_t n = strlen(node->name);
        if (pos < n + 1) {
            break;
        }
        pos -= n;
        memcpy(tmp + pos, node->name, n);
        tmp[--pos] = '/';
    }

    size_t n = strlen(tmp + pos);
    if (n >= size) {
        n = size - 1;
    }
    memcpy(buf, tmp + pos, n);
    buf[n] = '\0';
}

const char* fs_strerror(int err) {
    switch (err) {
    case FS_ENOENT:    return "no such file or directory";
    case FS_ENOTDIR:   return "not a directory";
    case FS_EISDIR:    return "is a directory";
    case FS_EEXIST:    return "already exists";
    case FS_ENOTEMPTY: return "directory not empty";
    case FS_EINVAL:    return "invalid name";
    case FS_EBUSY:     return "busy (root or current directory)";
    case FS_ENOMEM:    return "out of memory";
    default:           return "error";
    }
}
