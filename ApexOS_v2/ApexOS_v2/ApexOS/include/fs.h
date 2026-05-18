/* =====================================================
   include/fs.h - ApexOS Enhanced Virtual Filesystem
   Features: directories, paths, permissions, metadata
===================================================== */
#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Filesystem limits ────────────────────────────── */
#define FS_MAX_FILES     64
#define FS_MAX_FNAME     64
#define FS_MAX_PATH     256
#define FS_MAX_CONTENT 1024
#define FS_MAX_DEPTH      8

/* ── File types ───────────────────────────────────── */
typedef enum {
    FS_TYPE_FILE = 0,
    FS_TYPE_DIR  = 1
} fs_type_t;

/* ── File permissions (Unix-style bitmask) ─────────── */
#define FS_PERM_READ    0x4
#define FS_PERM_WRITE   0x2
#define FS_PERM_EXEC    0x1
#define FS_PERM_DEFAULT (FS_PERM_READ | FS_PERM_WRITE)

/* ── Inode structure ──────────────────────────────── */
typedef struct {
    char      name[FS_MAX_FNAME];      /* filename only (no path) */
    char      content[FS_MAX_CONTENT]; /* file content */
    fs_type_t type;                    /* file or directory */
    uint8_t   perms;                   /* rwx bits */
    uint32_t  size;                    /* content size in bytes */
    uint32_t  created_ticks;           /* creation time (ticks) */
    uint32_t  modified_ticks;          /* last modified (ticks)  */
    int       parent;                  /* parent inode index (-1 = root) */
    bool      used;                    /* slot in use? */
} fs_inode_t;

/* ── Public API ───────────────────────────────────── */
void  fs_init(void);

/* CRUD */
int   fs_create(const char *path, fs_type_t type);
int   fs_remove(const char *path);
int   fs_find_path(const char *path);          /* returns inode index or -1 */
fs_inode_t *fs_get(int idx);

/* Content */
int   fs_write(const char *path, const char *content);
int   fs_append(const char *path, const char *content);
const char *fs_read(const char *path);

/* Directory */
int   fs_list(const char *path, int *out, int max); /* fills out[] with inode indices */

/* Path helpers */
void  fs_resolve(const char *cwd, const char *rel, char *abs_out); /* resolve relative path */
const char *fs_basename(const char *path);
void  fs_dirname(const char *path, char *out);

/* Copy / Move */
int   fs_copy(const char *src, const char *dst);
int   fs_move(const char *src, const char *dst);

/* Stats */
uint32_t fs_total_files(void);
uint32_t fs_total_dirs(void);

#endif /* FS_H */
