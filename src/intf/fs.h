#pragma once

#include <stddef.h>
#include <stdint.h>

#define FS_NAME_MAX 28

enum fs_type { FS_FILE, FS_DIR };

enum fs_error {
    FS_OK = 0,
    FS_ENOENT = -1,   // no such file or directory
    FS_ENOTDIR = -2,  // a path component is not a directory
    FS_EISDIR = -3,   // operation needs a file
    FS_EEXIST = -4,
    FS_ENOTEMPTY = -5,
    FS_EINVAL = -6,   // bad name or path
    FS_EBUSY = -7,    // root, or the current directory
    FS_ENOMEM = -8,
};

// In-memory filesystem. Children of a directory form a sorted singly linked list.
struct fs_node {
    char name[FS_NAME_MAX];
    enum fs_type type;
    struct fs_node* parent;
    struct fs_node* child;  // directories: first entry
    struct fs_node* next;   // next sibling
    uint8_t* data;          // files: contents
    size_t size;
    size_t capacity;
};

void fs_init(void);
struct fs_node* fs_root(void);
struct fs_node* fs_cwd(void);

// Path resolution accepts absolute and relative paths with "." and "..".
int fs_lookup(const char* path, struct fs_node** out);
int fs_chdir(const char* path);
int fs_mkdir(const char* path);
int fs_create(const char* path, struct fs_node** out); // like touch: ok if file exists
int fs_remove(const char* path);                       // files and empty directories
int fs_write(struct fs_node* file, const void* data, size_t len, int append);
void fs_path(const struct fs_node* node, char* buf, size_t size);
const char* fs_strerror(int err);
