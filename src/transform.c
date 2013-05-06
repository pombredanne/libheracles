/*
 * transform.c: support for building and running transformers
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

#include <fnmatch.h>
#include <glob.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <selinux/selinux.h>
#include <stdbool.h>

#include "internal.h"
#include "memory.h"
#include "heracles.h"
#include "syntax.h"
#include "transform.h"
#include "errcode.h"
#include "labels.h"

static const int fnm_flags = FNM_PATHNAME;
static const int glob_flags = GLOB_NOSORT;

/* Extension for newly created files */
#define EXT_HERANEW ".heranew"
/* Extension for backup files */
#define EXT_HERASAVE ".herasave"


/*
 * Filters
 */
struct filter *make_filter(struct string *glb, unsigned int include) {
    struct filter *f;
    make_ref(f);
    f->glob = glb;
    f->include = include;
    return f;
}

void free_filter(struct filter *f) {
    if (f == NULL)
        return;
    assert(f->ref == 0);
    unref(f->next, filter);
    unref(f->glob, string);
    free(f);
}

static const char *pathbase(const char *path) {
    const char *p = strrchr(path, SEP);
    return (p == NULL) ? path : p + 1;
}

static bool is_excl(struct tree *f) {
    return streqv(f->label, "excl") && f->value != NULL;
}

static bool is_incl(struct tree *f) {
    return streqv(f->label, "incl") && f->value != NULL;
}

static bool is_regular_file(const char *path) {
    int r;
    struct stat st;

    r = stat(path, &st);
    if (r < 0)
        return false;
    return S_ISREG(st.st_mode);
}

static char *mtime_as_string(struct heracles *hera, const char *fname) {
    int r;
    struct stat st;
    char *result = NULL;

    if (fname == NULL) {
        result = strdup("0");
        ERR_NOMEM(result == NULL, hera);
        goto done;
    }

    r = stat(fname, &st);
    if (r < 0) {
        /* If we fail to stat, silently ignore the error
         * and report an impossible mtime */
        result = strdup("0");
        ERR_NOMEM(result == NULL, hera);
    } else {
        r = xasprintf(&result, "%ld", (long) st.st_mtime);
        ERR_NOMEM(r < 0, hera);
    }
 done:
    return result;
 error:
    FREE(result);
    return NULL;
}

static bool file_current(struct heracles *hera, const char *fname,
                         struct tree *finfo) {
    struct tree *mtime = tree_child(finfo, s_mtime);
    struct tree *file = NULL, *path = NULL;
    int r;
    struct stat st;
    int64_t mtime_i;

    if (mtime == NULL || mtime->value == NULL)
        return false;

    r = xstrtoint64(mtime->value, 10, &mtime_i);
    if (r < 0) {
        /* Ignore silently and err on the side of caution */
        return false;
    }

    r = stat(fname, &st);
    if (r < 0)
        return false;

    if (mtime_i != (int64_t) st.st_mtime)
        return false;

    path = tree_child(finfo, s_path);
    if (path == NULL)
        return false;

    file = tree_find(hera, path->value);
    return (file != NULL && ! file->dirty);
}

/* 
 * Looks for the files in the lens ? 
 * 
 */
static int filter_generate(struct tree *xfm, const char *root,
                           int *nmatches, char ***matches) {
    glob_t globbuf;
    int gl_flags = glob_flags;
    int r;
    int ret = 0;
    char **pathv = NULL;
    int pathc = 0;
    int root_prefix = strlen(root) - 1;

    *nmatches = 0;
    *matches = NULL;
    MEMZERO(&globbuf, 1);

    list_for_each(f, xfm->children) {
        char *globpat = NULL;
        if (! is_incl(f))
            continue;
        pathjoin(&globpat, 2, root, f->value);
        r = glob(globpat, gl_flags, NULL, &globbuf);
        free(globpat);

        if (r != 0 && r != GLOB_NOMATCH)
            goto error;
        gl_flags |= GLOB_APPEND;
    }

    pathc = globbuf.gl_pathc;
    int pathind = 0;

    if (ALLOC_N(pathv, pathc) < 0)
        goto error;

    for (int i=0; i < pathc; i++) {
        const char *path = globbuf.gl_pathv[i] + root_prefix;
        bool include = true;

        list_for_each(e, xfm->children) {
            if (! is_excl(e))
                continue;

            if (strchr(e->value, SEP) == NULL)
                path = pathbase(path);
            if ((r = fnmatch(e->value, path, fnm_flags)) == 0) {
                include = false;
            }
        }

        if (include)
            include = is_regular_file(globbuf.gl_pathv[i]);

        if (include) {
            pathv[pathind] = strdup(globbuf.gl_pathv[i]);
            if (pathv[pathind] == NULL)
                goto error;
            pathind += 1;
        }
    }
    pathc = pathind;

    if (REALLOC_N(pathv, pathc) == -1)
        goto error;

    *matches = pathv;
    *nmatches = pathc;
 done:
    globfree(&globbuf);
    return ret;
 error:
    if (pathv != NULL)
        for (int i=0; i < pathc; i++)
            free(pathv[i]);
    free(pathv);
    ret = -1;
    goto done;
}

static int filter_matches(struct tree *xfm, const char *path) {
    int found = 0;
    list_for_each(f, xfm->children) {
        if (is_incl(f) && fnmatch(f->value, path, fnm_flags) == 0) {
            found = 1;
            break;
        }
    }
    if (! found)
        return 0;
    list_for_each(f, xfm->children) {
        if (is_excl(f) && (fnmatch(f->value, path, fnm_flags) == 0))
            return 0;
    }
    return 1;
}

/*
 * Transformers
 */
struct transform *make_transform(struct lens *lens, struct filter *filter) {
    struct transform *xform;
    make_ref(xform);
    xform->lens = lens;
    xform->filter = filter;
    return xform;
}

void free_transform(struct transform *xform) {
    if (xform == NULL)
        return;
    assert(xform->ref == 0);
    unref(xform->lens, lens);
    unref(xform->filter, filter);
    free(xform);
}

static char *err_path(const char *filename) {
    char *result = NULL;
    if (filename == NULL)
        pathjoin(&result, 2, HERACLES_META_FILES, s_error);
    else
        pathjoin(&result, 3, HERACLES_META_FILES, filename, s_error);
    return result;
}

ATTRIBUTE_FORMAT(printf, 4, 5)
static void err_set(struct heracles *hera,
                    struct tree *err_info, const char *sub,
                    const char *format, ...) {
    int r;
    va_list ap;
    char *value = NULL;
    struct tree *tree = NULL;

    va_start(ap, format);
    r = vasprintf(&value, format, ap);
    va_end(ap);
    if (r < 0)
        value = NULL;
    ERR_NOMEM(r < 0, hera);

    tree = tree_child_cr(err_info, sub);
    ERR_NOMEM(tree == NULL, hera);

    r = tree_set_value(tree, value);
    ERR_NOMEM(r < 0, hera);

 error:
    free(value);
}

/* Record an error in the tree. The error will show up underneath
 * /heracles/FILENAME/error if filename is not NULL, and underneath
 * /heracles/text/PATH otherwise. PATH is the path to the toplevel node in
 * the tree where the lens application happened. When STATUS is NULL, just
 * clear any error associated with FILENAME in the tree.
 */
static int store_error(struct heracles *hera,
                       const char *filename, const char *path,
                       const char *status, int errnum,
                       const struct lns_error *err, const char *text) {
    struct tree *err_info = NULL, *finfo = NULL;
    char *fip = NULL;
    int r;
    int result = -1;

    if (filename != NULL) {
        r = pathjoin(&fip, 2, HERACLES_META_FILES, filename);
    } else {
        r = pathjoin(&fip, 2, HERACLES_META_TEXT, path);
    }
    ERR_NOMEM(r < 0, hera);

    finfo = tree_find_cr(hera, fip);
    ERR_BAIL(hera);

    if (status != NULL) {
        err_info = tree_child_cr(finfo, s_error);
        ERR_NOMEM(err_info == NULL, hera);

        r = tree_set_value(err_info, status);
        ERR_NOMEM(r < 0, hera);

        /* Errors from err_set are ignored on purpose. We try
         * to report as much as we can */
        if (err != NULL) {
            if (err->pos >= 0) {
                size_t line, ofs;
                err_set(hera, err_info, s_pos, "%d", err->pos);
                if (text != NULL) {
                    calc_line_ofs(text, err->pos, &line, &ofs);
                    err_set(hera, err_info, s_line, "%zd", line);
                    err_set(hera, err_info, s_char, "%zd", ofs);
                }
            }
            if (err->path != NULL) {
                err_set(hera, err_info, s_path, "%s%s", path, err->path);
            }
            if (err->lens != NULL) {
                char *fi = format_info(err->lens->info);
                if (fi != NULL) {
                    err_set(hera, err_info, s_lens, "%s", fi);
                    free(fi);
                }
            }
            err_set(hera, err_info, s_message, "%s", err->message);
        } else if (errnum != 0) {
            const char *msg = strerror(errnum);
            err_set(hera, err_info, s_message, "%s", msg);
        }
    } else {
        /* No error, nuke the error node if it exists */
        err_info = tree_child(finfo, s_error);
        if (err_info != NULL) {
            tree_unlink_children(hera, err_info);
            pathx_symtab_remove_descendants(hera->symtab, err_info);
            tree_unlink(err_info);
        }
    }

    tree_clean(finfo);
    result = 0;
 error:
    free(fip);
    return result;
}

/* Set up the file information in the /heracles tree.
 *
 * NODE must be the path to the file contents, and start with /files.
 * LENS is the lens used to transform the file.
 * Create entries under /heracles/NODE with some metadata about the file.
 *
 * Returns 0 on success, -1 on error
 */
static int add_file_info(struct heracles *hera, const char *node,
                         struct lens *lens, const char *lens_name,
                         const char *filename, bool force_reload) {
    struct tree *file, *tree;
    char *tmp = NULL;
    int r;
    char *path = NULL;
    int result = -1;

    if (lens == NULL)
        return -1;

    r = pathjoin(&path, 2, HERACLES_META_TREE, node);
    ERR_NOMEM(r < 0, hera);

    file = tree_find_cr(hera, path);
    ERR_BAIL(hera);

    /* Set 'path' */
    tree = tree_child_cr(file, s_path);
    ERR_NOMEM(tree == NULL, hera);
    r = tree_set_value(tree, node);
    ERR_NOMEM(r < 0, hera);

    /* Set 'mtime' */
    if (force_reload) {
        tmp = strdup("0");
        ERR_NOMEM(tmp == NULL, hera);
    } else {
        tmp = mtime_as_string(hera, filename);
        ERR_BAIL(hera);
    }
    tree = tree_child_cr(file, s_mtime);
    ERR_NOMEM(tree == NULL, hera);
    tree_store_value(tree, &tmp);

    /* Set 'lens/info' */
    tmp = format_info(lens->info);
    ERR_NOMEM(tmp == NULL, hera);
    tree = tree_path_cr(file, 2, s_lens, s_info);
    ERR_NOMEM(tree == NULL, hera);
    r = tree_set_value(tree, tmp);
    ERR_NOMEM(r < 0, hera);
    FREE(tmp);

    /* Set 'lens' */
    tree = tree->parent;
    r = tree_set_value(tree, lens_name);
    ERR_NOMEM(r < 0, hera);

    tree_clean(file);

    result = 0;
 error:
    free(path);
    free(tmp);
    return result;
}

static char *append_newline(char *text, size_t len) {
    /* Try to append a newline; this is a big hack to work */
    /* around the fact that lenses generally break if the  */
    /* file does not end with a newline. */
    if (len == 0 || text[len-1] != '\n') {
        if (REALLOC_N(text, len+2) == 0) {
            text[len] = '\n';
            text[len+1] = '\0';
        }
    }
    return text;
}

/* Turn the file name FNAME, which starts with hera->root, into
 * a path in the tree underneath /files */
static char *file_name_path(struct heracles *hera, const char *fname) {
    char *path = NULL;

    pathjoin(&path, 2, HERACLES_FILES_TREE, fname + strlen(hera->root) - 1);
    return path;
}

static int load_file(struct heracles *hera, struct lens *lens,
                     const char *lens_name, char *filename) {
    char *text = NULL;
    const char *err_status = NULL;
    struct tree *tree = NULL;
    char *path = NULL;
    struct lns_error *err = NULL;
    struct span *span = NULL;
    int result = -1, r, text_len = 0;

    path = file_name_path(hera, filename);
    ERR_NOMEM(path == NULL, hera);

    r = add_file_info(hera, path, lens, lens_name, filename, false);
    if (r < 0)
        goto done;

    text = xread_file(filename);
    if (text == NULL) {
        err_status = "read_failed";
        goto done;
    }
    text_len = strlen(text);
    text = append_newline(text, text_len);

    struct info *info;
    make_ref(info);
    make_ref(info->filename);
    info->filename->str = strdup(filename);
    info->error = hera->error;
    info->flags = hera->flags;
    info->first_line = 1;

    if (hera->flags & HERA_ENABLE_SPAN) {
        span = make_span(info);
        ERR_NOMEM(span == NULL, info);
    }

    tree = lns_get(info, lens, text, &err);

    unref(info, info);

    if (err != NULL) {
        err_status = "parse_failed";
        goto done;
    }

    tree_replace(hera, path, tree);

    /* top level node span entire file length */
    if (span != NULL && tree != NULL) {
        tree->parent->span = span;
        tree->parent->span->span_start = 0;
        tree->parent->span->span_end = text_len;
    }

    tree = NULL;

    result = 0;
 done:
    store_error(hera, filename + strlen(hera->root) - 1, path, err_status,
                errno, err, text);
 error:
    free_lns_error(err);
    free(path);
    free_tree(tree);
    free(text);
    return result;
}

/* The lens for a transform can be referred to in one of two ways:
 * either by a fully qualified name "Module.lens" or by the special
 * syntax "@Module"; the latter means we should take the lens from the
 * autoload transform for Module
 */
static struct lens *lens_from_name(struct heracles *hera, const char *name) {
    struct lens *result = NULL;

    if (name[0] == '@') {
        struct module *modl = NULL;
        for (modl = hera->modules;
             modl != NULL && !streqv(modl->name, name + 1);
             modl = modl->next);
        ERR_THROW(modl == NULL, hera, HERA_ENOLENS,
                  "Could not find module %s", name + 1);
        ERR_THROW(modl->autoload == NULL, hera, HERA_ENOLENS,
                  "No autoloaded lens in module %s", name + 1);
        result = modl->autoload->lens;
    } else {
        result = lens_lookup(hera, name);
    }
    ERR_THROW(result == NULL, hera, HERA_ENOLENS,
              "Can not find lens %s", name);
    return result;
 error:
    return NULL;
}

int text_store(struct heracles *hera, const char *lens_path,
               const char *path, const char *text) {
    struct info *info = NULL;
    struct lns_error *err = NULL;
    struct tree *tree = NULL;
    struct span *span = NULL;
    int result = -1;
    const char *err_status = NULL;
    struct lens *lens = NULL;

    lens = lens_from_name(hera, lens_path);
    if (lens == NULL) {
        goto done;
    }

    make_ref(info);
    info->first_line = 1;
    info->last_line = 1;
    info->first_column = 1;
    info->last_column = strlen(text);

    tree = lns_get(info, lens, text, &err);
    if (err != NULL) {
        err_status = "parse_failed";
        goto done;
    }

    unref(info, info);

    tree_replace(hera, path, tree);

    /* top level node span entire file length */
    if (span != NULL && tree != NULL) {
        tree->parent->span = span;
        tree->parent->span->span_start = 0;
        tree->parent->span->span_end = strlen(text);
    }

    tree = NULL;

    result = 0;
done:
    store_error(hera, NULL, path, err_status, errno, err, text);
    free_tree(tree);
    free_lns_error(err);
    return result;
}

const char *xfm_lens_name(struct tree *xfm) {
    struct tree *l = tree_child(xfm, s_lens);

    if (l == NULL)
        return "(unknown)";
    if (l->value == NULL)
        return "(noname)";
    return l->value;
}

static struct lens *xfm_lens(struct heracles *hera,
                             struct tree *xfm, const char **lens_name) {
    struct tree *l = NULL;

    for (l = xfm->children;
         l != NULL && !streqv("lens", l->label);
         l = l->next);

    if (l == NULL || l->value == NULL)
        return NULL;
    *lens_name = l->value;

    return lens_from_name(hera, l->value);
}

static void xfm_error(struct tree *xfm, const char *msg) {
    char *v = strdup(msg);
    char *l = strdup("error");

    if (l == NULL || v == NULL)
        return;
    tree_append(xfm, l, v);
}

int transform_validate(struct heracles *hera, struct tree *xfm) {
    struct tree *l = NULL;

    for (struct tree *t = xfm->children; t != NULL; ) {
        if (streqv(t->label, "lens")) {
            l = t;
        } else if ((is_incl(t) || (is_excl(t) && strchr(t->value, SEP) != NULL))
                       && t->value[0] != SEP) {
            /* Normalize relative paths to absolute ones */
            int r;
            r = REALLOC_N(t->value, strlen(t->value) + 2);
            ERR_NOMEM(r < 0, hera);
            memmove(t->value + 1, t->value, strlen(t->value) + 1);
            t->value[0] = SEP;
        }

        if (streqv(t->label, "error")) {
            struct tree *del = t;
            t = del->next;
            tree_unlink(del);
        } else {
            t = t->next;
        }
    }

    if (l == NULL) {
        xfm_error(xfm, "missing a child with label 'lens'");
        return -1;
    }
    if (l->value == NULL) {
        xfm_error(xfm, "the 'lens' node does not contain a lens name");
        return -1;
    }
    lens_from_name(hera, l->value);
    ERR_BAIL(hera);

    return 0;
 error:
    xfm_error(xfm, hera->error->details);
    return -1;
}

void transform_file_error(struct heracles *hera, const char *status,
                          const char *filename, const char *format, ...) {
    char *ep = err_path(filename);
    struct tree *err;
    char *msg;
    va_list ap;
    int r;

    err = tree_find_cr(hera, ep);
    if (err == NULL)
        return;

    tree_unlink_children(hera, err);
    tree_set_value(err, status);

    err = tree_child_cr(err, s_message);
    if (err == NULL)
        return;

    va_start(ap, format);
    r = vasprintf(&msg, format, ap);
    va_end(ap);
    if (r < 0)
        return;
    tree_set_value(err, msg);
    free(msg);
}

static struct tree *file_info(struct heracles *hera, const char *fname) {
    char *path = NULL;
    struct tree *result = NULL;
    int r;

    r = pathjoin(&path, 2, HERACLES_META_FILES, fname);
    ERR_NOMEM(r < 0, hera);

    result = tree_find(hera, path);
    ERR_BAIL(hera);
 error:
    free(path);
    return result;
}

int transform_load(struct heracles *hera, struct tree *xfm) {
    int nmatches = 0;
    char **matches;
    const char *lens_name;
    struct lens *lens = xfm_lens(hera, xfm, &lens_name);
    int r;

    if (lens == NULL) {
        // FIXME: Record an error and return 0
        return -1;
    }
    r = filter_generate(xfm, hera->root, &nmatches, &matches);
    if (r == -1)
        return -1;
    for (int i=0; i < nmatches; i++) {
        const char *filename = matches[i] + strlen(hera->root) - 1;
        struct tree *finfo = file_info(hera, filename);
        if (finfo != NULL && !finfo->dirty &&
            tree_child(finfo, s_lens) != NULL) {
            const char *s = xfm_lens_name(finfo);
            char *fpath = file_name_path(hera, matches[i]);
            transform_file_error(hera, "mxfm_load", filename,
              "Lenses %s and %s could be used to load this file",
                                 s, lens_name);
            hera_rm(hera, fpath);
            free(fpath);
        } else if (!file_current(hera, matches[i], finfo)) {
            load_file(hera, lens, lens_name, matches[i]);
        }
        if (finfo != NULL)
            finfo->dirty = 0;
        FREE(matches[i]);
    }
    lens_release(lens);
    free(matches);
    return 0;
}

int transform_applies(struct tree *xfm, const char *path) {
    if (STRNEQLEN(path, HERACLES_FILES_TREE, strlen(HERACLES_FILES_TREE))
        || path[strlen(HERACLES_FILES_TREE)] != SEP)
        return 0;
    return filter_matches(xfm, path + strlen(HERACLES_FILES_TREE));
}

static int transfer_file_attrs(FILE *from, FILE *to,
                               const char **err_status) {
    struct stat st;
    int ret = 0;
    int selinux_enabled = (is_selinux_enabled() > 0);
    security_context_t con = NULL;

    int from_fd = fileno(from);
    int to_fd = fileno(to);

    ret = fstat(from_fd, &st);
    if (ret < 0) {
        *err_status = "replace_stat";
        return -1;
    }
    if (selinux_enabled) {
        if (fgetfilecon(from_fd, &con) < 0 && errno != ENOTSUP) {
            *err_status = "replace_getfilecon";
            return -1;
        }
    }

    if (fchown(to_fd, st.st_uid, st.st_gid) < 0) {
        *err_status = "replace_chown";
        return -1;
    }
    if (fchmod(to_fd, st.st_mode) < 0) {
        *err_status = "replace_chmod";
        return -1;
    }
    if (selinux_enabled && con != NULL) {
        if (fsetfilecon(to_fd, con) < 0 && errno != ENOTSUP) {
            *err_status = "replace_setfilecon";
            return -1;
        }
        freecon(con);
    }
    return 0;
}

/* Try to rename FROM to TO. If that fails with an error other than EXDEV
 * or EBUSY, return -1. If the failure is EXDEV or EBUSY (which we assume
 * means that FROM or TO is a bindmounted file), and COPY_IF_RENAME_FAILS
 * is true, copy the contents of FROM into TO and delete FROM.
 *
 * If COPY_IF_RENAME_FAILS and UNLINK_IF_RENAME_FAILS are true, and the above
 * copy mechanism is used, it will unlink the TO path and open with O_EXCL
 * to ensure we only copy *from* a bind mount rather than into an attacker's
 * mount placed at TO (e.g. for .herasave).
 *
 * Return 0 on success (either rename succeeded or we copied the contents
 * over successfully), -1 on failure.
 */
static int clone_file(const char *from, const char *to,
                      const char **err_status, int copy_if_rename_fails,
                      int unlink_if_rename_fails) {
    FILE *from_fp = NULL, *to_fp = NULL;
    char buf[BUFSIZ];
    size_t len;
    int to_fd = -1, to_oflags, r;
    int result = -1;

    if (rename(from, to) == 0)
        return 0;
    if ((errno != EXDEV && errno != EBUSY) || !copy_if_rename_fails) {
        *err_status = "rename";
        return -1;
    }

    /* rename not possible, copy file contents */
    if (!(from_fp = fopen(from, "r"))) {
        *err_status = "clone_open_src";
        goto done;
    }

    if (unlink_if_rename_fails) {
        r = unlink(to);
        if (r < 0) {
            *err_status = "clone_unlink_dst";
            goto done;
        }
    }

    to_oflags = unlink_if_rename_fails ? O_EXCL : O_TRUNC;
    if ((to_fd = open(to, O_WRONLY|O_CREAT|to_oflags, S_IRUSR|S_IWUSR)) < 0) {
        *err_status = "clone_open_dst";
        goto done;
    }
    if (!(to_fp = fdopen(to_fd, "w"))) {
        *err_status = "clone_fdopen_dst";
        goto done;
    }

    if (transfer_file_attrs(from_fp, to_fp, err_status) < 0)
        goto done;

    while ((len = fread(buf, 1, BUFSIZ, from_fp)) > 0) {
        if (fwrite(buf, 1, len, to_fp) != len) {
            *err_status = "clone_write";
            goto done;
        }
    }
    if (ferror(from_fp)) {
        *err_status = "clone_read";
        goto done;
    }
    if (fflush(to_fp) != 0) {
        *err_status = "clone_flush";
        goto done;
    }
    if (fsync(fileno(to_fp)) < 0) {
        *err_status = "clone_sync";
        goto done;
    }
    result = 0;
 done:
    if (from_fp != NULL)
        fclose(from_fp);
    if (to_fp != NULL) {
        if (fclose(to_fp) != 0) {
            *err_status = "clone_fclose_dst";
            result = -1;
        }
    } else if (to_fd >= 0 && close(to_fd) < 0) {
        *err_status = "clone_close_dst";
        result = -1;
    }
    if (result != 0)
        unlink(to);
    if (result == 0)
        unlink(from);
    return result;
}

static char *strappend(const char *s1, const char *s2) {
    size_t len = strlen(s1) + strlen(s2);
    char *result = NULL, *p;

    if (ALLOC_N(result, len + 1) < 0)
        return NULL;

    p = stpcpy(result, s1);
    stpcpy(p, s2);
    return result;
}

static int file_saved_event(struct heracles *hera, const char *path) {
    const char *saved = strrchr(HERACLES_EVENTS_SAVED, SEP) + 1;
    struct pathx *px;
    struct tree *dummy;
    int r;

    px = pathx_hera_parse(hera, hera->origin, NULL,
                         HERACLES_EVENTS_SAVED "[last()]", true);
    ERR_BAIL(hera);

    if (pathx_find_one(px, &dummy) == 1) {
        r = tree_insert(px, saved, 0);
        if (r < 0)
            goto error;
    }

    if (! tree_set(px, path))
        goto error;

    free_pathx(px);
    return 0;
 error:
    free_pathx(px);
    return -1;
}

/*
 * Save TREE->CHILDREN into the file PATH using the lens from XFORM. Errors
 * are noted in the /heracles/files hierarchy in HERA->ORIGIN under
 * PATH/error.
 *
 * Writing the file happens by first writing into a temp file, transferring all
 * file attributes of PATH to the temp file, and then renaming the temp file
 * back to PATH.
 *
 * Temp files are created alongside the destination file to enable the rename,
 * which may be the canonical path (PATH_canon) if PATH is a symlink.
 *
 * If the HERA_SAVE_NEWFILE flag is set, instead rename to PATH.heranew rather
 * than PATH.  If HERA_SAVE_BACKUP is set, move the original to PATH.herasave.
 * (Always PATH.hera{new,save} irrespective of whether PATH is a symlink.)
 *
 * If the rename fails, and the entry HERACLES_COPY_IF_FAILURE exists in
 * HERA->ORIGIN, PATH is instead overwritten by copying file contents.
 *
 * The table below shows the locations for each permutation.
 *
 * PATH       save flag    temp file           dest file      backup?
 * regular    -            PATH.heranew.XXXX    PATH           -
 * regular    BACKUP       PATH.heranew.XXXX    PATH           PATH.herasave
 * regular    NEWFILE      PATH.heranew.XXXX    PATH.heranew    -
 * symlink    -            PATH_canon.XXXX     PATH_canon     -
 * symlink    BACKUP       PATH_canon.XXXX     PATH_canon     PATH.herasave
 * symlink    NEWFILE      PATH.heranew.XXXX    PATH.heranew    -
 *
 * Return 0 on success, -1 on failure.
 */
int transform_save(struct heracles *hera, struct tree *xfm,
                   const char *path, struct tree *tree) {
    int   fd;
    FILE *fp = NULL, *heraorig_canon_fp = NULL;
    char *heratemp = NULL, *heranew = NULL, *heraorig = NULL, *herasave = NULL;
    char *heraorig_canon = NULL, *heradest = NULL;
    int   heraorig_exists;
    int   copy_if_rename_fails = 0;
    char *text = NULL;
    const char *filename = path + strlen(HERACLES_FILES_TREE) + 1;
    const char *err_status = NULL;
    char *dyn_err_status = NULL;
    struct lns_error *err = NULL;
    const char *lens_name;
    struct lens *lens = xfm_lens(hera, xfm, &lens_name);
    int result = -1, r;
    bool force_reload;

    errno = 0;

    if (lens == NULL) {
        err_status = "lens_name";
        goto done;
    }

    copy_if_rename_fails =
        hera_get(hera, HERACLES_COPY_IF_RENAME_FAILS, NULL) == 1;

    if (asprintf(&heraorig, "%s%s", hera->root, filename) == -1) {
        heraorig = NULL;
        goto done;
    }

    heraorig_canon = canonicalize_file_name(heraorig);
    heraorig_exists = 1;
    if (heraorig_canon == NULL) {
        if (errno == ENOENT) {
            heraorig_canon = heraorig;
            heraorig_exists = 0;
        } else {
            err_status = "canon_heraorig";
            goto done;
        }
    }

    if (access(heraorig_canon, R_OK) == 0) {
        heraorig_canon_fp = fopen(heraorig_canon, "r");
        text = xfread_file(heraorig_canon_fp);
    } else {
        text = strdup("");
    }

    if (text == NULL) {
        err_status = "put_read";
        goto done;
    }

    text = append_newline(text, strlen(text));

    /* Figure out where to put the .heranew and temp file. If no .heranew file
       then put the temp file next to heraorig_canon, else next to .heranew. */
    if (hera->flags & HERA_SAVE_NEWFILE) {
        if (xasprintf(&heranew, "%s" EXT_HERANEW, heraorig) < 0) {
            err_status = "heranew_oom";
            goto done;
        }
        heradest = heranew;
    } else {
        heradest = heraorig_canon;
    }

    if (xasprintf(&heratemp, "%s.XXXXXX", heradest) < 0) {
        err_status = "heratemp_oom";
        goto done;
    }

    // FIXME: We might have to create intermediate directories
    // to be able to write heranew, but we have no idea what permissions
    // etc. they should get. Just the process default ?
    fd = mkstemp(heratemp);
    if (fd < 0) {
        err_status = "mk_heratemp";
        goto done;
    }
    fp = fdopen(fd, "w");
    if (fp == NULL) {
        err_status = "open_heratemp";
        goto done;
    }

    if (heraorig_exists) {
        if (transfer_file_attrs(heraorig_canon_fp, fp, &err_status) != 0) {
            err_status = "xfer_attrs";
            goto done;
        }
    } else {
        /* Since mkstemp is used, the temp file will have secure permissions
         * instead of those implied by umask, so change them for new files */
        mode_t curumsk = umask(022);
        umask(curumsk);

        if (fchmod(fileno(fp), 0666 - curumsk) < 0) {
            err_status = "create_chmod";
            return -1;
        }
    }

    if (tree != NULL)
        lns_put(fp, lens, tree->children, text, &err);

    if (ferror(fp)) {
        err_status = "error_heratemp";
        goto done;
    }

    if (fflush(fp) != 0) {
        err_status = "flush_heratemp";
        goto done;
    }

    if (fsync(fileno(fp)) < 0) {
        err_status = "sync_heratemp";
        goto done;
    }

    if (fclose(fp) != 0) {
        err_status = "close_heratemp";
        fp = NULL;
        goto done;
    }

    fp = NULL;

    if (err != NULL) {
        err_status = err->pos >= 0 ? "parse_skel_failed" : "put_failed";
        unlink(heratemp);
        goto done;
    }

    {
        char *new_text = xread_file(heratemp);
        int same = 0;
        if (new_text == NULL) {
            err_status = "read_heratemp";
            goto done;
        }
        same = STREQ(text, new_text);
        FREE(new_text);
        if (same) {
            result = 0;
            unlink(heratemp);
            goto done;
        } else if (hera->flags & HERA_SAVE_NOOP) {
            result = 1;
            unlink(heratemp);
            goto done;
        }
    }

    if (!(hera->flags & HERA_SAVE_NEWFILE)) {
        if (heraorig_exists && (hera->flags & HERA_SAVE_BACKUP)) {
            r = xasprintf(&herasave, "%s" EXT_HERASAVE, heraorig);
            if (r == -1) {
                herasave = NULL;
                goto done;
            }

            r = clone_file(heraorig_canon, herasave, &err_status, 1, 1);
            if (r != 0) {
                dyn_err_status = strappend(err_status, "_herasave");
                goto done;
            }
        }
    }

    r = clone_file(heratemp, heradest, &err_status, copy_if_rename_fails, 0);
    if (r != 0) {
        dyn_err_status = strappend(err_status, "_heratemp");
        goto done;
    }

    result = 1;

 done:
    force_reload = hera->flags & HERA_SAVE_NEWFILE;
    r = add_file_info(hera, path, lens, lens_name, heraorig, force_reload);
    if (r < 0) {
        err_status = "file_info";
        result = -1;
    }
    if (result > 0) {
        r = file_saved_event(hera, path);
        if (r < 0) {
            err_status = "saved_event";
            result = -1;
        }
    }
    {
        const char *emsg =
            dyn_err_status == NULL ? err_status : dyn_err_status;
        store_error(hera, filename, path, emsg, errno, err, text);
    }
    free(dyn_err_status);
    lens_release(lens);
    free(text);
    free(heratemp);
    free(heranew);
    if (heraorig_canon != heraorig)
        free(heraorig_canon);
    free(heraorig);
    free(herasave);
    free_lns_error(err);

    if (fp != NULL)
        fclose(fp);
    if (heraorig_canon_fp != NULL)
        fclose(heraorig_canon_fp);
    return result;
}

int text_retrieve(struct heracles *hera, const char *lens_name,
                  const char *path, struct tree *tree,
                  const char *text_in, char **text_out) {
    struct memstream ms;
    bool ms_open = false;
    const char *err_status = NULL;
    char *dyn_err_status = NULL;
    struct lns_error *err = NULL;
    struct lens *lens = NULL;
    int result = -1, r;

    MEMZERO(&ms, 1);
    errno = 0;

    lens = lens_from_name(hera, lens_name);
    if (lens == NULL) {
        err_status = "lens_name";
        goto done;
    }

    r = init_memstream(&ms);
    if (r < 0) {
        err_status = "init_memstream";
        goto done;
    }
    ms_open = true;

    if (tree != NULL)
        lns_put(ms.stream, lens, tree->children, text_in, &err);

    r = close_memstream(&ms);
    ms_open = false;
    if (r < 0) {
        err_status = "close_memstream";
        goto done;
    }

    *text_out = ms.buf;
    ms.buf = NULL;

    if (err != NULL) {
        err_status = err->pos >= 0 ? "parse_skel_failed" : "put_failed";
        goto done;
    }

    result = 0;

 done:
    {
        const char *emsg =
            dyn_err_status == NULL ? err_status : dyn_err_status;
        store_error(hera, NULL, path, emsg, errno, err, text_in);
    }
    free(dyn_err_status);
    lens_release(lens);
    if (result < 0) {
        free(*text_out);
        *text_out = NULL;
    }
    free_lns_error(err);

    if (ms_open)
        close_memstream(&ms);
    return result;
}

int remove_file(struct heracles *hera, struct tree *tree) {
    char *path = NULL;
    const char *filename = NULL;
    const char *err_status = NULL;
    char *dyn_err_status = NULL;
    char *herasave = NULL, *heraorig = NULL, *heraorig_canon = NULL;
    int r;

    path = path_of_tree(tree);
    if (path == NULL) {
        err_status = "path_of_tree";
        goto error;
    }
    filename = path + strlen(HERACLES_META_FILES);

    if ((heraorig = strappend(hera->root, filename + 1)) == NULL) {
        err_status = "root_file";
        goto error;
    }

    heraorig_canon = canonicalize_file_name(heraorig);
    if (heraorig_canon == NULL) {
        if (errno == ENOENT) {
            goto done;
        } else {
            err_status = "canon_heraorig";
            goto error;
        }
    }

    r = file_saved_event(hera, path + strlen(HERACLES_META_TREE));
    if (r < 0) {
        err_status = "saved_event";
        goto error;
    }

    if (hera->flags & HERA_SAVE_NOOP)
        goto done;

    if (hera->flags & HERA_SAVE_BACKUP) {
        /* Move file to one with extension .herasave */
        r = asprintf(&herasave, "%s" EXT_HERASAVE, heraorig_canon);
        if (r == -1) {
            herasave = NULL;
                goto error;
        }

        r = clone_file(heraorig_canon, herasave, &err_status, 1, 1);
        if (r != 0) {
            dyn_err_status = strappend(err_status, "_herasave");
            goto error;
        }
    } else {
        /* Unlink file */
        r = unlink(heraorig_canon);
        if (r < 0) {
            err_status = "unlink_orig";
            goto error;
        }
    }
    tree_unlink(tree);
 done:
    free(path);
    free(heraorig);
    free(heraorig_canon);
    free(herasave);
    return 0;
 error:
    {
        const char *emsg =
            dyn_err_status == NULL ? err_status : dyn_err_status;
        store_error(hera, filename, path, emsg, errno, NULL, NULL);
    }
    free(path);
    free(heraorig);
    free(heraorig_canon);
    free(herasave);
    free(dyn_err_status);
    return -1;
}

/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
