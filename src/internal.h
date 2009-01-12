/*
 * internal.h: Useful definitions
 *
 * Copyright (C) 2007, 2008 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 * Author: David Lutterkort <dlutter@redhat.com>
 */

#ifndef INTERNAL_H_
#define INTERNAL_H_

#define DEBUG

#include "list.h"
#include "datadir.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

/*
 * Various parameters about env vars, special tree nodes etc.
 */

/* Define: AUGEAS_LENS_DIR
 * The default location for lens definitions */
#define AUGEAS_LENS_DIR DATADIR "/augeas/lenses"

/* Define: AUGEAS_ROOT_ENV
 * The env var that points to the chroot holding files we may modify.
 * Mostly useful for testing */
#define AUGEAS_ROOT_ENV "AUGEAS_ROOT"

/* Define: AUGEAS_FILES_TREE
 * The root for actual file contents */
#define AUGEAS_FILES_TREE "/files"

/* Define: AUGEAS_META_TREE
 * Augeas reports some information in this subtree */
#define AUGEAS_META_TREE "/augeas"

/* Define: AUGEAS_META_FILES
 * Information about files */
#define AUGEAS_META_FILES AUGEAS_META_TREE AUGEAS_FILES_TREE

/* Define: AUGEAS_META_ROOT
 * The root directory */
#define AUGEAS_META_ROOT AUGEAS_META_TREE "/root"

/* Define: AUGEAS_META_SAVE_MODE
 * How we save files. One of 'backup', 'overwrite' or 'newfile' */
#define AUGEAS_META_SAVE_MODE AUGEAS_META_TREE "/save"

/* Define: AUGEAS_CLONE_IF_RENAME_FAILS
 * Control what save does when renaming the temporary file to its final
 * destination fails with EXDEV or EBUSY: when this tree node exists, copy
 * the file contents. If it is not present, simply give up and report an
 * error.  */
#define AUGEAS_COPY_IF_RENAME_FAILS \
    AUGEAS_META_SAVE_MODE "/copy_if_rename_fails"

/* A hierarchy where we record certain 'events', e.g. which tree
 * nodes actually gotsaved into files */
#define AUGEAS_EVENTS AUGEAS_META_TREE "/events"

#define AUGEAS_EVENTS_SAVED AUGEAS_EVENTS "/saved"

/* Define: AUGEAS_LENS_ENV
 * Name of env var that contains list of paths to search for additional
   spec files */
#define AUGEAS_LENS_ENV "AUGEAS_LENS_LIB"

/* Define: MAX_ENV_SIZE
 * Fairly arbitrary bound on the length of the path we
 *  accept from AUGEAS_SPEC_ENV */
#define MAX_ENV_SIZE 4096

/* Define: PATH_SEP_CHAR
 * Character separating paths in a list of paths */
#define PATH_SEP_CHAR ':'

/* Constants for setting the save mode via the augeas path at
 * AUGEAS_META_SAVE_MODE */
#define AUG_SAVE_BACKUP_TEXT "backup"
#define AUG_SAVE_NEWFILE_TEXT "newfile"
#define AUG_SAVE_NOOP_TEXT "noop"
#define AUG_SAVE_OVERWRITE_TEXT "overwrite"

#ifdef __GNUC__

#ifndef __GNUC_PREREQ
#define __GNUC_PREREQ(maj,min) 0
#endif

/**
 * ATTRIBUTE_UNUSED:
 *
 * Macro to flag conciously unused parameters to functions
 */
#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__((__unused__))
#endif

/**
 * ATTRIBUTE_FORMAT
 *
 * Macro used to check printf/scanf-like functions, if compiling
 * with gcc.
 */
#ifndef ATTRIBUTE_FORMAT
#define ATTRIBUTE_FORMAT(args...) __attribute__((__format__ (args)))
#endif

#ifndef ATTRIBUTE_PURE
#define ATTRIBUTE_PURE __attribute__((pure))
#endif

#ifndef ATTRIBUTE_RETURN_CHECK
#if __GNUC_PREREQ (3, 4)
#define ATTRIBUTE_RETURN_CHECK __attribute__((__warn_unused_result__))
#else
#define ATTRIBUTE_RETURN_CHECK
#endif
#endif

#else
#define ATTRIBUTE_UNUSED
#define ATTRIBUTE_FORMAT(...)
#define ATTRIBUTE_PURE
#define ATTRIBUTE_RETURN_CHECK
#endif                                   /* __GNUC__ */

/* String equality tests, suggested by Jim Meyering. */
#define STREQ(a,b) (strcmp((a),(b)) == 0)
#define STRCASEEQ(a,b) (strcasecmp((a),(b)) == 0)
#define STRCASEEQLEN(a,b,n) (strncasecmp((a),(b),(n)) == 0)
#define STRNEQ(a,b) (strcmp((a),(b)) != 0)
#define STRCASENEQ(a,b) (strcasecmp((a),(b)) != 0)
#define STREQLEN(a,b,n) (strncmp((a),(b),(n)) == 0)
#define STRNEQLEN(a,b,n) (strncmp((a),(b),(n)) != 0)

ATTRIBUTE_PURE
static inline int streqv(const char *a, const char *b) {
    if (a == NULL || b == NULL)
        return a == b;
    return STREQ(a,b);
}

/* Path length and comparison */

#define SEP '/'

/* Length of PATH without any trailing '/' */
ATTRIBUTE_PURE
static inline int pathlen(const char *path) {
    int len = strlen(path);

    if (len > 0 && path[len-1] == SEP)
        len--;

    return len;
}

/* Return 1 if P1 is a prefix of P2. P1 as a string must have length <= P2 */
ATTRIBUTE_PURE
static inline int pathprefix(const char *p1, const char *p2) {
    if (p1 == NULL || p2 == NULL)
        return 0;
    int l1 = pathlen(p1);

    return STREQLEN(p1, p2, l1) && (p2[l1] == '\0' || p2[l1] == SEP);
}

static inline int pathendswith(const char *path, const char *basenam) {
    const char *p = strrchr(path, SEP);
    if (p == NULL)
        return 0;
    return streqv(p+1, basenam);
}

/* Join NSEG path components (passed as const char *) into one PATH.
   Allocate as needed. Return 0 on success, -1 on failure */
int pathjoin(char **path, int nseg, ...);

/* Call calloc to allocate an array of N instances of *VAR */
#define CALLOC(Var,N) do { (Var) = calloc ((N), sizeof (*(Var))); } while (0)

#define MEMZERO(ptr, n) memset((ptr), 0, (n) * sizeof(*(ptr)));

/**
 * TODO:
 *
 * macro to flag unimplemented blocks
 */
#define TODO 								\
    fprintf(stderr, "%s:%d Unimplemented block\n",			\
            __FILE__, __LINE__);

#define FIXME(msg, args ...)                            \
    do {                                                \
        fprintf(stderr, "%s:%d Fixme: ",                \
                __FILE__, __LINE__);                    \
      fprintf(stderr, msg, ## args);                    \
      fputc('\n', stderr);                              \
    } while(0)

/*
 * Internal data structures
 */

// internal.c

/* Function: escape
 * Escape nonprintable characters within TEXT, similar to how it's done in
 * C string literals. Caller must free the returned string.
 */
char *escape(const char *text, int cnt);

/* Function: unescape */
char *unescape(const char *s, int len);

/* Function: print_chars */
int print_chars(FILE *out, const char *text, int cnt);

/* Function: print_pos
 * Print a pretty representation of being at position POS within TEXT */
void print_pos(FILE *out, const char *text, int pos);
char *format_pos(const char *text, int pos);

/* Function: read_file
 * Read the contents of file PATH and return them as one long string. The
 * caller must free the result. Return NULL if any error occurs.
 */
char* read_file(const char *path);

/* Define: AUG_NO_DEFAULT_LOAD
 * A hidden flag used by augparse to suppress loading of all the modules
 * on the path */
#define AUG_NO_DEFAULT_LOAD (1 << 15)

/* Struct: augeas
 * The data structure representing a connection to Augeas. */
struct augeas {
    struct tree      *origin;     /* Actual tree root is origin->children */
    const char       *root;       /* Filesystem root for all files */
                                  /* always ends with '/' */
    unsigned int      flags;      /* Flags passed to AUG_INIT */
    struct module    *modules;    /* Loaded modules */
    size_t            nmodpath;
    char             *modpathz;   /* The search path for modules as a
                                     glibc argz vector */
};

/* Struct: tree
 * An entry in the global config tree. The data structure allows associating
 * values with interior nodes, but the API currently marks that as an error.
 *
 * To make dealing with parents uniform, even for the root, we create
 * standalone trees with a fake root, called origin. That root is generally
 * not referenced from anywhere. Standalone trees should be created with
 * MAKE_TREE_ORIGIN.
 */
struct tree {
    struct tree *next;
    struct tree *parent;     /* Points to self for root */
    char        *label;      /* Last component of PATH */
    struct tree *children;   /* List of children through NEXT */
    char        *value;
    int          dirty;
};

#define ROOT_P(t) ((t) != NULL && (t)->parent == (t)->parent->parent)

/* Function: make_tree
 * Allocate a new tree node with the given LABEL, VALUE, and CHILDREN,
 * which are not copied. The new tree is marked as dirty
 */
struct tree *make_tree(char *label, char *value,
                       struct tree *parent, struct tree *children);

/* Mark a tree as a standalone tree; this creates a fake parent for ROOT,
 * so that even ROOT has a parent. A new node with only child ROOT is
 * returned on success, and NULL on failure.
 */
struct tree  *make_tree_origin(struct tree *root);

int aug_tree_replace(struct augeas *aug, const char *path, struct tree *sub);

int tree_rm(struct tree *origin, const char *path);
struct tree *tree_set(struct tree *tree, const char *path, const char *value);
int tree_insert(struct tree *origin, const char *path, const char *label,
                int before);
int free_tree(struct tree *tree);
int print_tree(const struct tree *tree, FILE *out, const char *path,
               int pr_hidden);
int tree_equal(const struct tree *t1, const struct tree *t2);

/* Struct: memstream
 * Wrappers to simulate OPEN_MEMSTREAM where that's not available. The
 * STREAM member is opened by INIT_MEMSTREAM and closed by
 * CLOSE_MEMSTREAM. The BUF is allocated automatically, but can not be used
 * until after CLOSE_MEMSTREAM has been called. It is the callers
 * responsibility to free up BUF.
 */
struct memstream {
    FILE   *stream;
    char   *buf;
    size_t size;
};

/* Function: init_memstream
 * Initialize a memstream. On systems that have OPEN_MEMSTREAM, it is used
 * to open MS->STREAM. On systems without OPEN_MEMSTREAM, MS->STREAM is
 * backed by a temporary file.
 *
 * MS must be allocated in advance; INIT_MEMSTREAM initializes it.
 */
int init_memstream(struct memstream *ms);

/* Function: close_memstream
 * Close a memstream. After calling this, MS->STREAM can not be used
 * anymore and a string representing whatever was written to it is
 * available in MS->BUF. The caller must free MS->BUF.
 *
 * The caller must free the MEMSTREAM structure.
 */
int close_memstream(struct memstream *ms);

/*
 * Path expressions
 */

struct path;

struct path *make_path(const struct tree *root, const char *path);
struct tree *path_first(struct path *path);
struct tree *path_next(struct path *path, struct tree *cur);
int path_find_one(struct path *path, struct tree **match);
int path_expand_tree(struct path *path, struct tree **tree);
void free_path(struct path *path);

#endif


/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
