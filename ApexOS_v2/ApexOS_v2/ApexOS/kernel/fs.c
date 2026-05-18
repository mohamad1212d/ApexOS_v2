/* =====================================================
   kernel/fs.c - ApexOS Enhanced Virtual Filesystem
   - Hierarchical directories with parent references
   - Absolute & relative path resolution
   - File permissions, size tracking, timestamps
   - Copy, move, remove operations
===================================================== */
#include "apex.h"
#include "fs.h"

/* ── Inode table ─────────────────────────────────── */
static fs_inode_t inodes[FS_MAX_FILES];

/* ── Internal helpers ────────────────────────────── */

/* Split path into parent dir path + basename.
   e.g. "/home/docs/file.txt" -> parent="/home/docs", base="file.txt" */
static void split_path(const char *path, char *parent_out, char *base_out) {
    int len = (int)kstrlen(path);
    /* find last '/' */
    int last_slash = -1;
    for (int i = 0; i < len; i++)
        if (path[i] == '/') last_slash = i;

    if (last_slash <= 0) {
        /* e.g. "file.txt" or "/file.txt" */
        kstrcpy(parent_out, "/");
        kstrcpy(base_out, (last_slash == 0) ? path + 1 : path);
    } else {
        /* copy parent part */
        for (int i = 0; i < last_slash; i++) parent_out[i] = path[i];
        parent_out[last_slash] = 0;
        /* copy base */
        kstrcpy(base_out, path + last_slash + 1);
    }
}

/* Find inode by name within a parent inode */
static int find_in_parent(int parent_idx, const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!inodes[i].used) continue;
        if (inodes[i].parent == parent_idx &&
            kstrcmp(inodes[i].name, name) == 0)
            return i;
    }
    return -1;
}

/* Walk a path component by component. Returns inode index or -1. */
static int walk_path(const char *path) {
    if (!path || !path[0]) return -1;

    /* root */
    if (path[0] == '/' && path[1] == '\0') return 0;

    /* start from root (inode 0) */
    int cur = 0;
    char buf[FS_MAX_PATH];
    kstrcpy(buf, path);

    /* skip leading slash */
    char *p = buf;
    if (*p == '/') p++;

    while (*p) {
        /* extract next component */
        char comp[FS_MAX_FNAME];
        int ci = 0;
        while (*p && *p != '/') comp[ci++] = *p++;
        comp[ci] = 0;
        if (*p == '/') p++;

        if (comp[0] == '\0') continue; /* double slash */

        int next = find_in_parent(cur, comp);
        if (next < 0) return -1;
        cur = next;
    }
    return cur;
}

/* Allocate a free inode slot */
static int alloc_inode(void) {
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (!inodes[i].used) return i;
    return -1;
}

/* ── Public API ─────────────────────────────────── */

void fs_init(void) {
    kmemset(inodes, 0, sizeof(inodes));

    /* Inode 0 = root "/" */
    kstrcpy(inodes[0].name, "/");
    inodes[0].type   = FS_TYPE_DIR;
    inodes[0].perms  = FS_PERM_READ | FS_PERM_WRITE | FS_PERM_EXEC;
    inodes[0].parent = -1;
    inodes[0].used   = true;

    /* Default directory tree */
    fs_create("/home",    FS_TYPE_DIR);
    fs_create("/docs",    FS_TYPE_DIR);
    fs_create("/system",  FS_TYPE_DIR);
    fs_create("/tmp",     FS_TYPE_DIR);
    fs_create("/net",     FS_TYPE_DIR);

    /* Default files */
    fs_create("/home/readme.txt", FS_TYPE_FILE);
    fs_write("/home/readme.txt",
        "Welcome to ApexOS v2.0\n"
        "=======================\n"
        "Enhanced filesystem with full path support.\n"
        "Network stack: NE2000 Ethernet driver.\n"
        "Type 'help' for all commands.\n");

    fs_create("/home/about.txt", FS_TYPE_FILE);
    fs_write("/home/about.txt",
        "ApexOS - Operating System\n"
        "Version: 2.0.0\n"
        "Architecture: x86 32-bit\n"
        "Language: C + x86 Assembly\n"
        "New: Enhanced FS + Network Stack\n");

    fs_create("/system/config.txt", FS_TYPE_FILE);
    fs_write("/system/config.txt",
        "hostname=apexos\n"
        "ip=192.168.1.100\n"
        "gateway=192.168.1.1\n"
        "dns=8.8.8.8\n");

    fs_create("/net/log.txt", FS_TYPE_FILE);
    fs_write("/net/log.txt", "Network log initialized.\n");
}

int fs_find_path(const char *path) {
    return walk_path(path);
}

fs_inode_t *fs_get(int idx) {
    if (idx < 0 || idx >= FS_MAX_FILES) return NULL;
    if (!inodes[idx].used) return NULL;
    return &inodes[idx];
}

int fs_create(const char *path, fs_type_t type) {
    char parent_path[FS_MAX_PATH];
    char base[FS_MAX_FNAME];
    split_path(path, parent_path, base);

    if (base[0] == '\0') return -1; /* can't create root again */

    int parent_idx = walk_path(parent_path);
    if (parent_idx < 0) return -1; /* parent doesn't exist */
    if (inodes[parent_idx].type != FS_TYPE_DIR) return -1;

    /* Check duplicate */
    if (find_in_parent(parent_idx, base) >= 0) return -1;

    int slot = alloc_inode();
    if (slot < 0) return -1;

    kmemset(&inodes[slot], 0, sizeof(fs_inode_t));
    kstrcpy(inodes[slot].name, base);
    inodes[slot].type            = type;
    inodes[slot].perms           = FS_PERM_DEFAULT;
    inodes[slot].parent          = parent_idx;
    inodes[slot].used            = true;
    inodes[slot].created_ticks   = timer_get_ticks();
    inodes[slot].modified_ticks  = inodes[slot].created_ticks;

    return slot;
}

int fs_remove(const char *path) {
    int idx = walk_path(path);
    if (idx <= 0) return -1; /* can't remove root */

    fs_inode_t *node = &inodes[idx];

    /* If directory, check it's empty */
    if (node->type == FS_TYPE_DIR) {
        for (int i = 0; i < FS_MAX_FILES; i++) {
            if (inodes[i].used && inodes[i].parent == idx)
                return -2; /* not empty */
        }
    }

    kmemset(node, 0, sizeof(fs_inode_t));
    return 0;
}

int fs_write(const char *path, const char *content) {
    int idx = walk_path(path);
    if (idx < 0) return -1;
    if (inodes[idx].type != FS_TYPE_FILE) return -1;

    size_t len = kstrlen(content);
    if (len >= FS_MAX_CONTENT) len = FS_MAX_CONTENT - 1;
    kmemcpy(inodes[idx].content, content, len);
    inodes[idx].content[len]     = 0;
    inodes[idx].size             = (uint32_t)len;
    inodes[idx].modified_ticks   = timer_get_ticks();
    return 0;
}

int fs_append(const char *path, const char *content) {
    int idx = walk_path(path);
    if (idx < 0) return -1;
    if (inodes[idx].type != FS_TYPE_FILE) return -1;

    size_t cur = kstrlen(inodes[idx].content);
    size_t add = kstrlen(content);
    if (cur + add >= FS_MAX_CONTENT) add = FS_MAX_CONTENT - cur - 1;
    kmemcpy(inodes[idx].content + cur, content, add);
    inodes[idx].content[cur + add]  = 0;
    inodes[idx].size                = (uint32_t)(cur + add);
    inodes[idx].modified_ticks      = timer_get_ticks();
    return 0;
}

const char *fs_read(const char *path) {
    int idx = walk_path(path);
    if (idx < 0) return NULL;
    if (inodes[idx].type != FS_TYPE_FILE) return NULL;
    return inodes[idx].content;
}

int fs_list(const char *path, int *out, int max) {
    int dir_idx = walk_path(path);
    if (dir_idx < 0) return -1;
    if (inodes[dir_idx].type != FS_TYPE_DIR) return -1;

    int count = 0;
    for (int i = 0; i < FS_MAX_FILES && count < max; i++) {
        if (inodes[i].used && inodes[i].parent == dir_idx)
            out[count++] = i;
    }
    return count;
}

/* Resolve a relative path against cwd into abs_out */
void fs_resolve(const char *cwd, const char *rel, char *abs_out) {
    if (!rel || rel[0] == '/') {
        /* already absolute */
        kstrcpy(abs_out, rel ? rel : "/");
        return;
    }
    /* combine cwd + "/" + rel */
    kstrcpy(abs_out, cwd);
    if (abs_out[kstrlen(abs_out)-1] != '/')
        kstrcat(abs_out, "/");
    kstrcat(abs_out, rel);

    /* Simplify: collapse "." and ".." */
    /* simple stack-based normaliser */
    char tmp[FS_MAX_PATH];
    char segs[FS_MAX_DEPTH][FS_MAX_FNAME];
    int  depth = 0;

    kstrcpy(tmp, abs_out);
    char *p = tmp;
    if (*p == '/') p++;

    while (*p) {
        char comp[FS_MAX_FNAME];
        int ci = 0;
        while (*p && *p != '/') comp[ci++] = *p++;
        comp[ci] = 0;
        if (*p == '/') p++;

        if (comp[0] == '\0' || kstrcmp(comp, ".") == 0) continue;
        if (kstrcmp(comp, "..") == 0) {
            if (depth > 0) depth--;
        } else {
            if (depth < FS_MAX_DEPTH)
                kstrcpy(segs[depth++], comp);
        }
    }

    /* rebuild */
    abs_out[0] = '/'; abs_out[1] = 0;
    for (int i = 0; i < depth; i++) {
        if (i > 0) kstrcat(abs_out, "/");
        kstrcat(abs_out, segs[i]);
    }
    if (abs_out[0] == 0) kstrcpy(abs_out, "/");
}

const char *fs_basename(const char *path) {
    const char *last = path;
    for (const char *p = path; *p; p++)
        if (*p == '/') last = p + 1;
    return last;
}

void fs_dirname(const char *path, char *out) {
    int len = (int)kstrlen(path);
    int last_slash = -1;
    for (int i = 0; i < len; i++)
        if (path[i] == '/') last_slash = i;
    if (last_slash <= 0) {
        kstrcpy(out, "/");
    } else {
        for (int i = 0; i < last_slash; i++) out[i] = path[i];
        out[last_slash] = 0;
    }
}

int fs_copy(const char *src, const char *dst) {
    int src_idx = walk_path(src);
    if (src_idx < 0) return -1;
    if (inodes[src_idx].type != FS_TYPE_FILE) return -1;

    int dst_idx = walk_path(dst);
    if (dst_idx < 0) {
        /* create destination */
        dst_idx = fs_create(dst, FS_TYPE_FILE);
        if (dst_idx < 0) return -1;
    }
    return fs_write(dst, inodes[src_idx].content);
}

int fs_move(const char *src, const char *dst) {
    if (fs_copy(src, dst) < 0) return -1;
    return fs_remove(src);
}

uint32_t fs_total_files(void) {
    uint32_t n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (inodes[i].used && inodes[i].type == FS_TYPE_FILE) n++;
    return n;
}

uint32_t fs_total_dirs(void) {
    uint32_t n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (inodes[i].used && inodes[i].type == FS_TYPE_DIR) n++;
    return n;
}
