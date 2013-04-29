/*
 * heracles.c: the core data structure for storing key/value pairs
 *
 * Copyright (C) 2007-2011 David Lutterkort
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
#include <config.h>
#include "labels.h"
#include "heracles.h"
#include "internal.h"
#include "memory.h"
#include "syntax.h"
#include "transform.h"
#include "errcode.h"
#include "tree.h"

#include <fnmatch.h>
#include <argz.h>
#include <string.h>
#include <stdarg.h>
#include <locale.h>


#define HERACLES_META_PATHX_FUNC HERACLES_META_TREE "/version/pathx/functions"

static const char *const static_nodes[][2] = {
    { HERACLES_FILES_TREE, NULL },
    { HERACLES_META_TREE "/variables", NULL },
    { HERACLES_META_TREE "/version", PACKAGE_VERSION },
    { HERACLES_META_TREE "/version/save/mode[1]", HERA_SAVE_BACKUP_TEXT },
    { HERACLES_META_TREE "/version/save/mode[2]", HERA_SAVE_NEWFILE_TEXT },
    { HERACLES_META_TREE "/version/save/mode[3]", HERA_SAVE_NOOP_TEXT },
    { HERACLES_META_TREE "/version/save/mode[4]", HERA_SAVE_OVERWRITE_TEXT },
    { HERACLES_META_TREE "/version/defvar/expr", NULL },
    { HERACLES_META_PATHX_FUNC "/count", NULL },
    { HERACLES_META_PATHX_FUNC "/glob", NULL },
    { HERACLES_META_PATHX_FUNC "/label", NULL },
    { HERACLES_META_PATHX_FUNC "/last", NULL },
    { HERACLES_META_PATHX_FUNC "/position", NULL },
    { HERACLES_META_PATHX_FUNC "/regexp", NULL }
};

static const char *const errcodes[] = {
    "No error",                                         /* HERA_NOERROR */
    "Cannot allocate memory",                           /* HERA_ENOMEM */
    "Internal error (please file a bug)",               /* HERA_EINTERNAL */
    "Invalid path expression",                          /* HERA_EPATHX */
    "No match for path expression",                     /* HERA_ENOMATCH */
    "Too many matches for path expression",             /* HERA_EMMATCH */
    "Syntax error in lens definition",                  /* HERA_ESYNTAX */
    "Lens not found",                                   /* HERA_ENOLENS */
    "Multiple transforms",                              /* HERA_EMXFM */
    "Node has no span info",                            /* HERA_ENOSPAN */
    "Cannot move node into its descendant",             /* HERA_EMVDESC */
    "Failed to execute command",                        /* HERA_ECMDRUN */
    "Invalid argument in function call",                /* HERA_EBADARG */
    "Invalid label"                                     /* HERA_ELABEL */
};


static void store_error(const struct heracles *hera, const char *label, const char *value,
                 int nentries, ...) {
    va_list ap;
    struct tree *tree;

    ensure(nentries % 2 == 0, hera);
    tree = tree_path_cr(hera->origin, 3, s_heracles, s_error, label);
    if (tree == NULL)
        return;

    tree_set_value(tree, value);

    va_start(ap, nentries);
    for (int i=0; i < nentries; i += 2) {
        char *l = va_arg(ap, char *);
        char *v = va_arg(ap, char *);
        struct tree *t = tree_child_cr(tree, l);
        if (t != NULL)
            tree_set_value(t, v);
    }
    va_end(ap);
 error:
    return;
}

/* Report pathx errors in /heracles/pathx/error */
static void store_pathx_error(const struct heracles *hera) {
    if (hera->error->code != HERA_EPATHX)
        return;

    store_error(hera, s_pathx, hera->error->minor_details,
                2, s_pos, hera->error->details);
}

struct pathx *pathx_hera_parse(const struct heracles *hera,
                              struct tree *tree,
                              struct tree *root_ctx,
                              const char *path, bool need_nodeset) {
    struct pathx *result;
    struct error *err = err_of_hera(hera);

    if (tree == NULL)
        tree = hera->origin;

    pathx_parse(tree, err, path, need_nodeset, hera->symtab, root_ctx, &result);
    return result;
}
/* Save user locale and switch to C locale */
#if HAVE_USELOCALE
static void save_locale(struct heracles *hera) {
    if (hera->c_locale == NULL) {
        hera->c_locale = newlocale(LC_ALL_MASK, "C", NULL);
        ERR_NOMEM(hera->c_locale == NULL, hera);
    }

    hera->user_locale = uselocale(hera->c_locale);
 error:
    return;
}
#else
static void save_locale(ATTRIBUTE_UNUSED struct heracles *hera) { }
#endif

#if HAVE_USELOCALE
static void restore_locale(struct heracles *hera) {
    uselocale(hera->user_locale);
    hera->user_locale = NULL;
}
#else
static void restore_locale(ATTRIBUTE_UNUSED struct heracles *hera) { }
#endif

/* Clean up old error messages every time we enter through the public
 * API. Since we make internal calls through the public API, we keep a
 * count of how many times a public API call was made, and only reset when
 * that count is 0. That requires that all public functions enclose their
 * work within a matching pair of api_entry/api_exit calls.
 */
void api_entry(const struct heracles *hera) {
    struct error *err = ((struct heracles *) hera)->error;

    ((struct heracles *) hera)->api_entries += 1;

    if (hera->api_entries > 1)
        return;

    reset_error(err);
    save_locale((struct heracles *) hera);
}

void api_exit(const struct heracles *hera) {
    assert(hera->api_entries > 0);
    ((struct heracles *) hera)->api_entries -= 1;
    if (hera->api_entries == 0) {
        store_pathx_error(hera);
        restore_locale((struct heracles *) hera);
    }
}

static int init_root(struct heracles *hera, const char *root0) {
    if (root0 == NULL)
        root0 = getenv(HERACLES_ROOT_ENV);
    if (root0 == NULL || root0[0] == '\0')
        root0 = "/";

    hera->root = strdup(root0);
    if (hera->root == NULL)
        return -1;

    if (hera->root[strlen(hera->root)-1] != SEP) {
        if (REALLOC_N(hera->root, strlen(hera->root) + 2) < 0)
            return -1;
        strcat((char *) hera->root, "/");
    }
    return 0;
}

static int init_loadpath(struct heracles *hera, const char *loadpath) {
    int r;

    hera->modpathz = NULL;
    hera->nmodpath = 0;
    if (loadpath != NULL) {
        r = argz_add_sep(&hera->modpathz, &hera->nmodpath,
                         loadpath, PATH_SEP_CHAR);
        if (r != 0)
            return -1;
    }
    char *env = getenv(HERACLES_LENS_ENV);
    if (env != NULL) {
        r = argz_add_sep(&hera->modpathz, &hera->nmodpath,
                         env, PATH_SEP_CHAR);
        if (r != 0)
            return -1;
    }
    if (!(hera->flags & HERA_NO_STDINC)) {
        r = argz_add(&hera->modpathz, &hera->nmodpath, HERACLES_LENS_DIR);
        if (r != 0)
            return -1;
        r = argz_add(&hera->modpathz, &hera->nmodpath,
                     HERACLES_LENS_DIST_DIR);
        if (r != 0)
            return -1;
    }
    /* Clean up trailing slashes */
    if (hera->nmodpath > 0) {
        argz_stringify(hera->modpathz, hera->nmodpath, PATH_SEP_CHAR);
        char *s, *t;
        const char *e = hera->modpathz + strlen(hera->modpathz);
        for (s = hera->modpathz, t = hera->modpathz; s < e; s++) {
            char *p = s;
            if (*p == '/') {
                while (*p == '/') p += 1;
                if (*p == '\0' || *p == PATH_SEP_CHAR)
                    s = p;
            }
            if (t != s)
                *t++ = *s;
            else
                t += 1;
        }
        if (t != s) {
            *t = '\0';
        }
        s = hera->modpathz;
        hera->modpathz = NULL;
        r = argz_create_sep(s, PATH_SEP_CHAR, &hera->modpathz,
                            &hera->nmodpath);
        free(s);
        if (r != 0)
            return -1;
    }
    return 0;
}

static void init_save_mode(struct heracles *hera) {
    const char *v = HERA_SAVE_OVERWRITE_TEXT;

    if (hera->flags & HERA_SAVE_NEWFILE) {
        v = HERA_SAVE_NEWFILE_TEXT;
    } else if (hera->flags & HERA_SAVE_BACKUP) {
        v = HERA_SAVE_BACKUP_TEXT;
    } else if (hera->flags & HERA_SAVE_NOOP) {
        v = HERA_SAVE_NOOP_TEXT;
    }

    hera_set(hera, HERACLES_META_SAVE_MODE, v);
}

struct heracles *hera_init(const char *root, const char *loadpath,
                        unsigned int flags) {
    struct heracles *result;
    struct tree *tree_root = make_tree(NULL, NULL, NULL, NULL);
    int r;
    bool close_on_error = true;

    if (tree_root == NULL)
        return NULL;

    if (ALLOC(result) < 0)
        goto error;
    if (ALLOC(result->error) < 0)
        goto error;
    if (make_ref(result->error->info) < 0)
        goto error;
    result->error->info->error = result->error;
    result->error->info->filename = dup_string("(unknown file)");
    if (result->error->info->filename == NULL)
        goto error;
    result->error->hera = result;

    result->origin = make_tree_origin(tree_root);
    if (result->origin == NULL) {
        free_tree(tree_root);
        goto error;
    }

    api_entry(result);

    result->flags = flags;

    r = init_root(result, root);
    ERR_NOMEM(r < 0, result);

    result->origin->children->label = strdup(s_heracles);

    /* We are now initialized enough that we can dare return RESULT even
     * when we encounter errors if the caller so wishes */
    close_on_error = !(flags & HERA_NO_ERR_CLOSE);

    r = init_loadpath(result, loadpath);
    ERR_NOMEM(r < 0, result);

    /* We report the root dir in HERACLES_META_ROOT, but we only use the
       value we store internally, to avoid any problems with
       HERACLES_META_ROOT getting changed. */
    hera_set(result, HERACLES_META_ROOT, result->root);
    ERR_BAIL(result);

    /* Set the default path context */
    hera_set(result, HERACLES_CONTEXT, HERA_CONTEXT_DEFAULT);
    ERR_BAIL(result);

    for (int i=0; i < ARRAY_CARDINALITY(static_nodes); i++) {
        hera_set(result, static_nodes[i][0], static_nodes[i][1]);
        ERR_BAIL(result);
    }

    init_save_mode(result);
    ERR_BAIL(result);

    const char *v = (flags & HERA_ENABLE_SPAN) ? HERA_ENABLE : HERA_DISABLE;
    hera_set(result, HERACLES_SPAN_OPTION, v);
    ERR_BAIL(result);

    if (interpreter_init(result) == -1)
        goto error;

    list_for_each(modl, result->modules) {
        struct transform *xform = modl->autoload;
        if (xform == NULL)
            continue;
        tree_from_transform(result, modl->name, xform);
        ERR_BAIL(result);
    }
    if (!(result->flags & HERA_NO_LOAD))
        if (hera_load(result) < 0)
            goto error;

    api_exit(result);
    return result;

 error:
    if (close_on_error) {
        hera_close(result);
        result = NULL;
    }
    if (result != NULL && result->api_entries > 0)
        api_exit(result);
    return result;
}

static void tree_mark_files(struct tree *tree) {
    if (tree_child(tree, "path") != NULL) {
        tree_mark_dirty(tree);
    } else {
        list_for_each(c, tree->children) {
            tree_mark_files(c);
        }
    }
}

int hera_load(struct heracles *hera) {
    const char *option = NULL;
    struct tree *meta = tree_child_cr(hera->origin, s_heracles);
    struct tree *meta_files = tree_child_cr(meta, s_files);
    struct tree *files = tree_child_cr(hera->origin, s_files);
    struct tree *load = tree_child_cr(meta, s_load);
    struct tree *vars = tree_child_cr(meta, s_vars);

    api_entry(hera);

    ERR_NOMEM(load == NULL, hera);

    /* To avoid unnecessary loads of files, we reload an existing file in
     * several steps:
     * (1) mark all file nodes under /heracles/files as dirty (and only those)
     * (2) process all files matched by a lens; we check (in
     *     transform_load) if the file has been modified. If it has, we
     *     reparse it. Either way, we clear the dirty flag. We also need to
     *     reread the file if part or all of it has been modified in the
     *     tree but not been saved yet
     * (3) remove all files from the tree that still have a dirty entry
     *     under /heracles/files. Those files are not processed by any lens
     *     anymore
     * (4) Remove entries from /heracles/files and /files that correspond
     *     to directories without any files of interest
     */

    /* update flags according to option value */
    if (hera_get(hera, HERACLES_SPAN_OPTION, &option) == 1) {
        if (strcmp(option, HERA_ENABLE) == 0) {
            hera->flags |= HERA_ENABLE_SPAN;
        } else {
            hera->flags &= ~HERA_ENABLE_SPAN;
        }
    }

    tree_clean(meta_files);
    tree_mark_files(meta_files);

    list_for_each(xfm, load->children) {
        if (transform_validate(hera, xfm) == 0)
            transform_load(hera, xfm);
    }

    /* This makes it possible to spot 'directories' that are now empty
     * because we removed their file contents */
    tree_clean(files);

    tree_rm_dirty_files(hera, meta_files);
    tree_rm_dirty_leaves(hera, meta_files, meta_files);
    tree_rm_dirty_leaves(hera, files, files);

    tree_clean(hera->origin);

    list_for_each(v, vars->children) {
        hera_defvar(hera, v->label, v->value);
        ERR_BAIL(hera);
    }

    api_exit(hera);
    return 0;
 error:
    api_exit(hera);
    return -1;
}
int hera_get(const struct heracles *hera, const char *path, const char **value) {
    struct pathx *p = NULL;
    struct tree *match;
    int r;

    api_entry(hera);

    p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), path, true);
    ERR_BAIL(hera);

    if (value != NULL)
        *value = NULL;

    r = pathx_find_one(p, &match);
    ERR_BAIL(hera);
    ERR_THROW(r > 1, hera, HERA_EMMATCH, "There are %d nodes matching %s",
              r, path);

    if (r == 1 && value != NULL)
        *value = match->value;
    free_pathx(p);

    api_exit(hera);
    return r;
 error:
    free_pathx(p);
    api_exit(hera);
    return -1;
}

int hera_label(const struct heracles *hera, const char *path, const char **label) {
    struct pathx *p = NULL;
    struct tree *match;
    int r;

    api_entry(hera);

    p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), path, true);
    ERR_BAIL(hera);

    if (label != NULL)
        *label = NULL;

    r = pathx_find_one(p, &match);
    ERR_BAIL(hera);
    ERR_THROW(r > 1, hera, HERA_EMMATCH, "There are %d nodes matching %s",
              r, path);

    if (r == 1 && label != NULL)
        *label = match->label;
    free_pathx(p);

    api_exit(hera);
    return r;
 error:
    free_pathx(p);
    api_exit(hera);
    return -1;
}

static void record_var_meta(struct heracles *hera, const char *name,
                            const char *expr) {
    /* Record the definition of the variable */
    struct tree *tree = tree_path_cr(hera->origin, 2, s_heracles, s_vars);
    ERR_NOMEM(tree == NULL, hera);
    if (expr == NULL) {
        tree = tree_child(tree, name);
        if (tree != NULL)
            tree_unlink(tree);
    } else {
        tree = tree_child_cr(tree, name);
        ERR_NOMEM(tree == NULL, hera);
        tree_set_value(tree, expr);
    }
 error:
    return;
}

int hera_defvar(heracles *hera, const char *name, const char *expr) {
    struct pathx *p = NULL;
    int result = -1;

    api_entry(hera);

    if (expr == NULL) {
        result = pathx_symtab_undefine(&(hera->symtab), name);
    } else {
        p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), expr, false);
        ERR_BAIL(hera);
        result = pathx_symtab_define(&(hera->symtab), name, p);
    }
    ERR_BAIL(hera);

    record_var_meta(hera, name, expr);
    ERR_BAIL(hera);
 error:
    free_pathx(p);
    api_exit(hera);
    return result;
}

int hera_defnode(heracles *hera, const char *name, const char *expr,
                const char *value, int *created) {
    struct pathx *p;
    int result = -1;
    int r, cr;
    struct tree *tree;

    api_entry(hera);

    if (expr == NULL)
        goto error;
    if (created == NULL)
        created = &cr;

    p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), expr, false);
    ERR_BAIL(hera);

    if (pathx_first(p) == NULL) {
        r = pathx_expand_tree(p, &tree);
        if (r < 0)
            goto done;
        *created = 1;
    } else {
        *created = 0;
    }

    if (*created) {
        r = tree_set_value(tree, value);
        if (r < 0)
            goto done;
        result = pathx_symtab_assign_tree(&(hera->symtab), name, tree);
        char *e = path_of_tree(tree);
        ERR_NOMEM(e == NULL, hera)
        record_var_meta(hera, name, e);
        free(e);
        ERR_BAIL(hera);
    } else {
        result = pathx_symtab_define(&(hera->symtab), name, p);
        record_var_meta(hera, name, expr);
        ERR_BAIL(hera);
    }

 done:
    free_pathx(p);
    api_exit(hera);
    return result;
 error:
    api_exit(hera);
    return -1;
}



int hera_set(struct heracles *hera, const char *path, const char *value) {
    struct pathx *p;
    int result;

    api_entry(hera);

    /* Get-out clause, in case context is broken */
    struct tree *root_ctx = NULL;
    if (STRNEQ(path, HERACLES_CONTEXT))
        root_ctx = tree_root_ctx(hera);

    p = pathx_hera_parse(hera, hera->origin, root_ctx, path, true);
    ERR_BAIL(hera);

    result = tree_set(p, value) == NULL ? -1 : 0;
    free_pathx(p);

    api_exit(hera);
    return result;
 error:
    api_exit(hera);
    return -1;
}

int hera_setm(struct heracles *hera, const char *base,
             const char *sub, const char *value) {
    struct pathx *bx = NULL, *sx = NULL;
    struct tree *bt, *st;
    int result, r;

    api_entry(hera);

    bx = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), base, true);
    ERR_BAIL(hera);

    if (sub != NULL && STREQ(sub, "."))
        sub = NULL;

    result = 0;
    for (bt = pathx_first(bx); bt != NULL; bt = pathx_next(bx)) {
        if (sub != NULL) {
            /* Handle subnodes of BT */
            sx = pathx_hera_parse(hera, bt, NULL, sub, true);
            ERR_BAIL(hera);
            if (pathx_first(sx) != NULL) {
                /* Change existing subnodes matching SUB */
                for (st = pathx_first(sx); st != NULL; st = pathx_next(sx)) {
                    r = tree_set_value(st, value);
                    ERR_NOMEM(r < 0, hera);
                    result += 1;
                }
            } else {
                /* Create a new subnode matching SUB */
                r = pathx_expand_tree(sx, &st);
                if (r == -1)
                    goto error;
                r = tree_set_value(st, value);
                ERR_NOMEM(r < 0, hera);
                result += 1;
            }
            free_pathx(sx);
            sx = NULL;
        } else {
            /* Set nodes matching BT directly */
            r = tree_set_value(bt, value);
            ERR_NOMEM(r < 0, hera);
            result += 1;
        }
    }

 done:
    api_exit(hera);
    return result;
 error:
    result = -1;
    goto done;
}

int hera_insert(struct heracles *hera, const char *path, const char *label,
               int before) {
    struct pathx *p = NULL;
    int result = -1;

    api_entry(hera);

    p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), path, true);
    ERR_BAIL(hera);

    result = tree_insert(p, label, before);
 error:
    free_pathx(p);
    api_exit(hera);
    return result;
}

int hera_rm(struct heracles *hera, const char *path) {
    struct pathx *p = NULL;
    int result;

    api_entry(hera);

    p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), path, true);
    ERR_BAIL(hera);

    result = tree_rm(p);
    free_pathx(p);
    ERR_BAIL(hera);

    api_exit(hera);
    return result;
 error:
    api_exit(hera);
    return -1;
}

int hera_span(struct heracles *hera, const char *path, char **filename,
        uint *label_start, uint *label_end, uint *value_start, uint *value_end,
        uint *span_start, uint *span_end) {
    struct pathx *p = NULL;
    int result = -1;
    struct tree *tree = NULL;
    struct span *span;

    api_entry(hera);

    p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), path, true);
    ERR_BAIL(hera);

    tree = pathx_first(p);
    ERR_BAIL(hera);

    ERR_THROW(tree == NULL, hera, HERA_ENOMATCH, "No node matching %s", path);
    ERR_THROW(tree->span == NULL, hera, HERA_ENOSPAN, "No span info for %s", path);
    ERR_THROW(pathx_next(p) != NULL, hera, HERA_EMMATCH, "Multiple nodes match %s", path);

    span = tree->span;

    if (label_start != NULL)
        *label_start = span->label_start;

    if (label_end != NULL)
        *label_end = span->label_end;

    if (value_start != NULL)
        *value_start = span->value_start;

    if (value_end != NULL)
        *value_end = span->value_end;

    if (span_start != NULL)
        *span_start = span->span_start;

    if (span_end != NULL)
        *span_end = span->span_end;

    /* We are safer here, make sure we have a filename */
    if (filename != NULL) {
        if (span->filename == NULL || span->filename->str == NULL) {
            *filename = strdup("");
        } else {
            *filename = strdup(span->filename->str);
        }
        ERR_NOMEM(*filename == NULL, hera);
    }

    result = 0;
 error:
    free_pathx(p);
    api_exit(hera);
    return result;
}

int hera_mv(struct heracles *hera, const char *src, const char *dst) {
    struct pathx *s = NULL, *d = NULL;
    struct tree *ts, *td, *t;
    int r, ret;

    api_entry(hera);

    ret = -1;
    s = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), src, true);
    ERR_BAIL(hera);

    d = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), dst, true);
    ERR_BAIL(hera);

    r = find_one_node(s, &ts);
    if (r < 0)
        goto error;

    r = pathx_expand_tree(d, &td);
    if (r == -1)
        goto error;

    /* Don't move SRC into its own descendent */
    t = td;
    do {
        ERR_THROW(t == ts, hera, HERA_EMVDESC,
                  "destination %s is a descendant of %s", dst, src);
        t = t->parent;
    } while (t != hera->origin);

    free_tree(td->children);

    td->children = ts->children;
    list_for_each(c, td->children) {
        c->parent = td;
    }
    free(td->value);
    td->value = ts->value;

    ts->value = NULL;
    ts->children = NULL;

    tree_unlink(ts);
    tree_mark_dirty(td);

    ret = 0;
 error:
    free_pathx(s);
    free_pathx(d);
    api_exit(hera);
    return ret;
}

int hera_rename(struct heracles *hera, const char *src, const char *lbl) {
    struct pathx *s = NULL;
    struct tree *ts;
    int ret;
    int count = 0;

    api_entry(hera);

    ret = -1;
    ERR_THROW(strchr(lbl, '/') != NULL, hera, HERA_ELABEL,
              "Label %s contains a /", lbl);

    s = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), src, true);
    ERR_BAIL(hera);

    for (ts = pathx_first(s); ts != NULL; ts = pathx_next(s)) {
        free(ts->label);
        ts->label = strdup(lbl);
        tree_mark_dirty(ts);
        count ++;
    }

    free_pathx(s);
    api_exit(hera);
    return count;
 error:
    free_pathx(s);
    api_exit(hera);
    return ret;
}

int hera_match(const struct heracles *hera, const char *pathin, char ***matches) {
    struct pathx *p = NULL;
    struct tree *tree;
    int cnt = 0;

    api_entry(hera);

    if (matches != NULL)
        *matches = NULL;

    if (STREQ(pathin, "/")) {
        pathin = "/*";
    }

    p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), pathin, true);
    ERR_BAIL(hera);

    for (tree = pathx_first(p); tree != NULL; tree = pathx_next(p)) {
        if (! TREE_HIDDEN(tree))
            cnt += 1;
    }
    ERR_BAIL(hera);

    if (matches == NULL)
        goto done;

    if (ALLOC_N(*matches, cnt) < 0)
        goto error;

    int i = 0;
    for (tree = pathx_first(p); tree != NULL; tree = pathx_next(p)) {
        if (TREE_HIDDEN(tree))
            continue;
        (*matches)[i] = path_of_tree(tree);
        if ((*matches)[i] == NULL) {
            goto error;
        }
        i += 1;
    }
    ERR_BAIL(hera);
 done:
    free_pathx(p);
    api_exit(hera);
    return cnt;

 error:
    if (matches != NULL) {
        if (*matches != NULL) {
            for (i=0; i < cnt; i++)
                free((*matches)[i]);
            free(*matches);
        }
    }
    free_pathx(p);
    api_exit(hera);
    return -1;
}



/* Reset the flags based on what is set in the tree. */
static int update_save_flags(struct heracles *hera) {
    const char *savemode ;

    hera_get(hera, HERACLES_META_SAVE_MODE, &savemode);
    if (savemode == NULL)
        return -1;

    hera->flags &= ~(HERA_SAVE_BACKUP|HERA_SAVE_NEWFILE|HERA_SAVE_NOOP);
    if (STREQ(savemode, HERA_SAVE_NEWFILE_TEXT)) {
        hera->flags |= HERA_SAVE_NEWFILE;
    } else if (STREQ(savemode, HERA_SAVE_BACKUP_TEXT)) {
        hera->flags |= HERA_SAVE_BACKUP;
    } else if (STREQ(savemode, HERA_SAVE_NOOP_TEXT)) {
        hera->flags |= HERA_SAVE_NOOP ;
    } else if (STRNEQ(savemode, HERA_SAVE_OVERWRITE_TEXT)) {
        return -1;
    }

    return 0;
}

static int unlink_removed_files(struct heracles *hera,
                                struct tree *files, struct tree *meta) {
    /* Find all nodes that correspond to a file and might have to be
     * unlinked. A node corresponds to a file if it has a child labelled
     * 'path', and we only consider it if there are no errors associated
     * with it */
    static const char *const file_nodes =
        "descendant-or-self::*[path][count(error) = 0]";

    int result = 0;

    if (! files->dirty)
        return 0;

    for (struct tree *tm = meta->children; tm != NULL;) {
        struct tree *tf = tree_child(files, tm->label);
        struct tree *next = tm->next;
        if (tf == NULL) {
            /* Unlink all files in tm */
            struct pathx *px = NULL;
            if (pathx_parse(tm, err_of_hera(hera), file_nodes, true,
                            hera->symtab, NULL, &px) != PATHX_NOERROR) {
                result = -1;
                continue;
            }
            for (struct tree *t = pathx_first(px);
                 t != NULL;
                 t = pathx_next(px)) {
                remove_file(hera, t);
            }
            free_pathx(px);
        } else if (tf->dirty && ! tree_child(tm, "path")) {
            if (unlink_removed_files(hera, tf, tm) < 0)
                result = -1;
        }
        tm = next;
    }
    return result;
}

int hera_save(struct heracles *hera) {
    int ret = 0;
    struct tree *meta = tree_child_cr(hera->origin, s_heracles);
    struct tree *meta_files = tree_child_cr(meta, s_files);
    struct tree *files = tree_child_cr(hera->origin, s_files);
    struct tree *load = tree_child_cr(meta, s_load);

    api_entry(hera);

    if (update_save_flags(hera) < 0)
        goto error;

    if (files == NULL || meta == NULL || load == NULL)
        goto error;

    hera_rm(hera, HERACLES_EVENTS_SAVED);

    list_for_each(xfm, load->children)
        transform_validate(hera, xfm);

    if (files->dirty) {
        if (tree_save(hera, files->children, HERACLES_FILES_TREE) == -1)
            ret = -1;

        /* Remove files whose entire subtree was removed. */
        if (meta_files != NULL) {
            if (unlink_removed_files(hera, files, meta_files) < 0)
                ret = -1;
        }
    }
    if (!(hera->flags & HERA_SAVE_NOOP)) {
        tree_clean(hera->origin);
    }

    api_exit(hera);
    return ret;
 error:
    api_exit(hera);
    return -1;
}

int hera_transform(struct heracles *hera, const char *lens,
                  const char *file, int excl) {
    struct tree *meta = tree_child_cr(hera->origin, s_heracles);
    struct tree *load = tree_child_cr(meta, s_load);

    int r = 0, result = -1;
    struct tree *xfm = NULL, *lns = NULL, *t = NULL;
    const char *filter = NULL;
    char *p;
    int exists;
    char *lensname = NULL, *xfmname = NULL;

    api_entry(hera);

    ERR_NOMEM(meta == NULL || load == NULL, hera);

    ARG_CHECK(STREQ("", lens), hera, "hera_transform: LENS must not be empty");
    ARG_CHECK(STREQ("", file), hera, "hera_transform: FILE must not be empty");

    if ((p = strrchr(lens, '.'))) {
        lensname = strdup(lens);
        xfmname = strndup(lens, p - lens);
        ERR_NOMEM(lensname == NULL || xfmname == NULL, hera);
    } else {
        r = xasprintf(&lensname, "%s.lns", lens);
        xfmname = strdup(lens);
        ERR_NOMEM(r < 0 || xfmname == NULL, hera);
    }

    xfm = tree_child_cr(load, xfmname);
    ERR_NOMEM(xfm == NULL, hera);

    lns = tree_child_cr(xfm, s_lens);
    ERR_NOMEM(lns == NULL, hera);

    tree_store_value(lns, &lensname);

    exists = 0;

    filter = excl ? s_excl : s_incl;
    list_for_each(c, xfm->children) {
        if (c->value != NULL && STREQ(c->value, file)
            && streqv(c->label, filter)) {
            exists = 1;
            break;
        }
    }
    if (! exists) {
        t = tree_append_s(xfm, filter, NULL);
        ERR_NOMEM(t == NULL, hera);
        r = tree_set_value(t, file);
        ERR_NOMEM(r < 0, hera);
    }

    result = 0;
 error:
    free(lensname);
    free(xfmname);
    api_exit(hera);
    return result;
}

void hera_close(struct heracles *hera) {
    if (hera == NULL)
        return;

    /* There's no point in bothering with api_entry/api_exit here */
    free_tree(hera->origin);
    unref(hera->modules, module);
    if (hera->error->exn != NULL) {
        hera->error->exn->ref = 0;
        free_value(hera->error->exn);
        hera->error->exn = NULL;
    }
    free((void *) hera->root);
    free(hera->modpathz);
    free_symtab(hera->symtab);
    unref(hera->error->info, info);
    free(hera->error->details);
    free(hera->error);
    free(hera);
}

int __hera_load_module_file(struct heracles *hera, const char *filename) {
    api_entry(hera);
    int r = load_module_file(hera, filename);
    api_exit(hera);
    return r;
}
/*
 * Error reporting API
 */
int hera_error(struct heracles *hera) {
    return hera->error->code;
}

const char *hera_error_message(struct heracles *hera) {
    hera_errcode_t errcode = hera->error->code;

    if (errcode >= ARRAY_CARDINALITY(errcodes))
        errcode = HERA_EINTERNAL;
    return errcodes[errcode];
}

const char *hera_error_minor_message(struct heracles *hera) {
    return hera->error->minor_details;
}

const char *hera_error_details(struct heracles *hera) {
    return hera->error->details;
}

/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
