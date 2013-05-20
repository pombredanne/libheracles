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

struct heracles *hera_init(const char *loadpath, unsigned int flags) {
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

    result->flags = flags;

    result->origin->children->label = strdup(s_heracles);

    /* We are now initialized enough that we can dare return RESULT even
     * when we encounter errors if the caller so wishes */
    close_on_error = !(flags & HERA_NO_ERR_CLOSE);

    r = init_loadpath(result, loadpath);
    ERR_NOMEM(r < 0, result);

    /* We report the root dir in HERACLES_META_ROOT, but we only use the
       value we store internally, to avoid any problems with
       HERACLES_META_ROOT getting changed. */
    if (interpreter_init(result) == -1)
        goto error;

    return result;

 error:
    if (close_on_error) {
        hera_close(result);
        result = NULL;
    }
    return result;
}

void hera_close(struct heracles *hera) {
    if (hera == NULL)
        return;

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

/*
 * Error reporting API
 */

/***********************************************************************
 *                       Heracles added stuff                          *
 ***********************************************************************/
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

struct tree * hera_get(struct lens *lens, char *text, struct lns_error *err) {
    struct tree *tree = NULL;
    struct info *info;
    make_ref(info);
    info->flags = 0;
    info->first_line = 1;
    info->filename = NULL;

    int text_len = strlen(text);
    text = append_newline(text, text_len);

    tree = lns_get(info, lens, text, &err);

    unref(info, info);

    return tree;
}

char * hera_put(struct lens *lens, struct tree *tree, char *text, struct lns_error *err)
{
    struct memstream ms;

    init_memstream(&ms);
    lns_put(ms.stream, lens, tree, text, &err);
    close_memstream(&ms);
    return ms.buf;
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

/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
