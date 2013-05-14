#include <config.h>
#include "tree.h"
#include "heracles.h"
#include "internal.h"
#include "memory.h"
#include "syntax.h"
#include "transform.h"
#include "errcode.h"
#include "labels.h"

#include <fnmatch.h>
#include <argz.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>


int find_one_node(struct pathx *p, struct tree **match) {
    struct error *err = err_of_pathx(p);
    int r = pathx_find_one(p, match);

    if (r == 1)
        return 0;

    if (r == 0) {
        report_error(err, HERA_ENOMATCH, NULL);
    } else {
        /* r > 1 */
        report_error(err, HERA_EMMATCH, NULL);
    }

    return -1;
}

void tree_mark_dirty(struct tree *tree) {
    do {
        tree->dirty = 1;
        tree = tree->parent;
    } while (tree != tree->parent && !tree->dirty);
    tree->dirty = 1;
}

void tree_clean(struct tree *tree) {
    if (tree->dirty) {
        list_for_each(c, tree->children)
            tree_clean(c);
    }
    tree->dirty = 0;
}

struct tree *tree_child(struct tree *tree, const char *label) {
    if (tree == NULL)
        return NULL;

    list_for_each(child, tree->children) {
        if (streqv(label, child->label))
            return child;
    }
    return NULL;
}

struct tree *tree_child_cr(struct tree *tree, const char *label) {
    static struct tree *child = NULL;

    if (tree == NULL)
        return NULL;

    child = tree_child(tree, label);
    if (child == NULL) {
        char *l = strdup(label);
        if (l == NULL)
            return NULL;
        child = tree_append(tree, l, NULL);
    }
    return child;
}

struct tree *tree_path_cr(struct tree *tree, int n, ...) {
    va_list ap;

    va_start(ap, n);
    for (int i=0; i < n; i++) {
        const char *l = va_arg(ap, const char *);
        tree = tree_child_cr(tree, l);
    }
    va_end(ap);
    return tree;
}

struct tree *tree_find(struct heracles *hera, const char *path) {
    struct pathx *p = NULL;
    struct tree *result = NULL;
    int r;

    p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), path, true);
    ERR_BAIL(hera);

    r = pathx_find_one(p, &result);
    BUG_ON(r > 1, hera,
           "Multiple matches for %s when only one was expected",
           path);
 done:
    free_pathx(p);
    return result;
 error:
    result = NULL;
    goto done;
}

struct tree *tree_find_cr(struct heracles *hera, const char *path) {
    struct pathx *p = NULL;
    struct tree *result = NULL;
    int r;

    p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), path, true);
    ERR_BAIL(hera);

    r = pathx_expand_tree(p, &result);
    ERR_BAIL(hera);
    ERR_THROW(r < 0, hera, HERA_EINTERNAL, "pathx_expand_tree failed");
 error:
    free_pathx(p);
    return result;
}

void tree_store_value(struct tree *tree, char **value) {
    if (streqv(tree->value, *value)) {
        free(*value);
        *value = NULL;
        return;
    }
    if (tree->value != NULL) {
        free(tree->value);
        tree->value = NULL;
    }
    if (*value != NULL) {
        tree->value = *value;
        *value = NULL;
    }
    tree_mark_dirty(tree);
}

int tree_set_value(struct tree *tree, const char *value) {
    char *v = NULL;

    if (streqv(tree->value, value))
        return 0;
    if (value != NULL) {
        v = strdup(value);
        if (v == NULL)
            return -1;
    }
    tree_store_value(tree, &v);
    return 0;
}

/* Find the tree stored in HERACLES_CONTEXT */
struct tree *tree_root_ctx(const struct heracles *hera) {
    struct pathx *p = NULL;
    struct tree *match = NULL;
    const char *ctx_path;
    int r;

    p = pathx_hera_parse(hera, hera->origin, NULL, HERACLES_CONTEXT, true);
    ERR_BAIL(hera);

    r = pathx_find_one(p, &match);
    ERR_THROW(r > 1, hera, HERA_EMMATCH,
              "There are %d nodes matching %s, expecting one",
              r, HERACLES_CONTEXT);

    if (match == NULL || match->value == NULL || *match->value == '\0')
        goto error;

    /* Clean via herarun's helper to ensure it's valid */
    ctx_path = cleanpath(match->value);
    free_pathx(p);

    p = pathx_hera_parse(hera, hera->origin, NULL, ctx_path, true);
    ERR_BAIL(hera);

    if (pathx_first(p) == NULL) {
        r = pathx_expand_tree(p, &match);
        if (r < 0)
            goto done;
        r = tree_set_value(match, NULL);
        if (r < 0)
            goto done;
    } else {
        r = pathx_find_one(p, &match);
        ERR_THROW(r > 1, hera, HERA_EMMATCH,
                  "There are %d nodes matching the context %s, expecting one",
                  r, ctx_path);
    }

 done:
    free_pathx(p);
    return match;
 error:
    match = NULL;
    goto done;
}

struct tree *tree_append(struct tree *parent,
                         char *label, char *value) {
    struct tree *result = make_tree(label, value, parent, NULL);
    if (result != NULL)
        list_append(parent->children, result);
    return result;
}

struct tree *tree_append_s(struct tree *parent,
                                  const char *l0, char *v) {
    struct tree *result;
    char *l = strdup(l0);

    if (l == NULL)
        return NULL;
    result = tree_append(parent, l, v);
    if (result == NULL)
        free(l);
    return result;
}

struct tree *tree_from_transform(struct heracles *hera,
                                        const char *modname,
                                        struct transform *xfm) {
    struct tree *meta = tree_child_cr(hera->origin, s_heracles);
    struct tree *load = NULL, *txfm = NULL, *t;
    char *v = NULL;
    int r;

    ERR_NOMEM(meta == NULL, hera);

    load = tree_child_cr(meta, s_load);
    ERR_NOMEM(load == NULL, hera);

    if (modname == NULL)
        modname = "_";

    txfm = tree_append_s(load, modname, NULL);
    ERR_NOMEM(txfm == NULL, hera);

    r = asprintf(&v, "@%s", modname);
    ERR_NOMEM(r < 0, hera);

    t = tree_append_s(txfm, s_lens, v);
    ERR_NOMEM(t == NULL, hera);
    v = NULL;

    list_for_each(f, xfm->filter) {
        const char *l = f->include ? s_incl : s_excl;
        v = strdup(f->glob->str);
        ERR_NOMEM(v == NULL, hera);
        t = tree_append_s(txfm, l, v);
        ERR_NOMEM(t == NULL, hera);
    }
    return txfm;
 error:
    free(v);
    tree_unlink(txfm);
    return NULL;
}

void tree_unlink_children(struct heracles *hera, struct tree *tree) {
    if (tree == NULL)
        return;

    pathx_symtab_remove_descendants(hera->symtab, tree);

    while (tree->children != NULL)
        tree_unlink(tree->children);
}

void tree_rm_dirty_files(struct heracles *hera, struct tree *tree) {
    struct tree *p;

    if (!tree->dirty)
        return;

    if ((p = tree_child(tree, "path")) != NULL) {
        hera_rm(hera, p->value);
        tree_unlink(tree);
    } else {
        struct tree *c = tree->children;
        while (c != NULL) {
            struct tree *next = c->next;
            tree_rm_dirty_files(hera, c);
            c = next;
        }
    }
}

void tree_rm_dirty_leaves(struct heracles *hera, struct tree *tree,
                                 struct tree *protect) {
    if (! tree->dirty)
        return;

    struct tree *c = tree->children;
    while (c != NULL) {
        struct tree *next = c->next;
        tree_rm_dirty_leaves(hera, c, protect);
        c = next;
    }

    if (tree != protect && tree->children == NULL)
        tree_unlink(tree);
}

struct tree *tree_set(struct pathx *p, const char *value) {
    struct tree *tree;
    int r;

    r = pathx_expand_tree(p, &tree);
    if (r == -1)
        return NULL;

    r = tree_set_value(tree, value);
    if (r < 0)
        return NULL;
    return tree;
}


int tree_insert(struct pathx *p, const char *label, int before) {
    struct tree *new = NULL, *match;

    if (strchr(label, SEP) != NULL)
        return -1;

    if (find_one_node(p, &match) < 0)
        goto error;

    new = make_tree(strdup(label), NULL, match->parent, NULL);
    if (new == NULL || new->label == NULL)
        goto error;

    if (before) {
        list_insert_before(new, match, new->parent->children);
    } else {
        new->next = match->next;
        match->next = new;
    }
    return 0;
 error:
    free_tree(new);
    return -1;
}

struct tree *make_tree(char *label, char *value, struct tree *parent,
                       struct tree *children) {
    struct tree *tree;
    if (ALLOC(tree) < 0)
        return NULL;

    tree->label = label;
    tree->value = value;
    tree->parent = parent;
    tree->children = children;
    tree->span = NULL;
    list_for_each(c, tree->children)
        c->parent = tree;
    if (parent != NULL)
        tree_mark_dirty(tree);
    else
        tree->dirty = 1;
    return tree;
}

struct tree *make_tree_origin(struct tree *root) {
    struct tree *origin = NULL;

    origin = make_tree(NULL, NULL, NULL, root);
    if (origin == NULL)
        return NULL;

    origin->parent = origin;
    return origin;
}

/* Free one tree node */
void free_tree_node(struct tree *tree) {
    if (tree == NULL)
        return;

    if (tree->span != NULL)
        free_span(tree->span);
    free(tree->label);
    free(tree->value);
    free(tree);
}

/* Recursively free the whole tree TREE and all its siblings */
int free_tree(struct tree *tree) {
    int cnt = 0;

    while (tree != NULL) {
        struct tree *del = tree;
        tree = del->next;
        cnt += free_tree(del->children);
        free_tree_node(del);
        cnt += 1;
    }

    return cnt;
}

int tree_unlink(struct tree *tree) {
    int result = 0;

    assert (tree->parent != NULL);
    list_remove(tree, tree->parent->children);
    tree_mark_dirty(tree->parent);
    result = free_tree(tree->children) + 1;
    free_tree_node(tree);
    return result;
}

int tree_rm(struct pathx *p) {
    struct tree *tree, **del;
    int cnt = 0, ndel = 0, i;

    for (tree = pathx_first(p); tree != NULL; tree = pathx_next(p)) {
        if (! TREE_HIDDEN(tree))
            ndel += 1;
    }

    if (ndel == 0)
        return 0;

    if (ALLOC_N(del, ndel) < 0) {
        free(del);
        return -1;
    }

    for (i = 0, tree = pathx_first(p); tree != NULL; tree = pathx_next(p)) {
        if (TREE_HIDDEN(tree))
            continue;
        pathx_symtab_remove_descendants(pathx_get_symtab(p), tree);
        del[i] = tree;
        i += 1;
    }

    for (i = 0; i < ndel; i++)
        cnt += tree_unlink(del[i]);
    free(del);

    return cnt;
}

int tree_replace(struct heracles *hera, const char *path, struct tree *sub) {
    struct tree *parent;
    struct pathx *p = NULL;
    int r;

    p = pathx_hera_parse(hera, hera->origin, tree_root_ctx(hera), path, true);
    ERR_BAIL(hera);

    r = tree_rm(p);
    if (r == -1)
        goto error;

    parent = tree_set(p, NULL);
    if (parent == NULL)
        goto error;

    list_append(parent->children, sub);
    list_for_each(s, sub) {
        s->parent = parent;
    }
    free_pathx(p);
    return 0;
 error:
    free_pathx(p);
    return -1;
}

int tree_save(struct heracles *hera, struct tree *tree,
                     const char *path) {
    int result = 0;
    struct tree *meta = tree_child_cr(hera->origin, s_heracles);
    struct tree *load = tree_child_cr(meta, s_load);

    // FIXME: We need to detect subtrees that aren't saved by anything

    if (load == NULL)
        return -1;

    list_for_each(t, tree) {
        if (t->dirty) {
            char *tpath = NULL;
            struct tree *transform = NULL;
            if (asprintf(&tpath, "%s/%s", path, t->label) == -1) {
                result = -1;
                continue;
            }
            list_for_each(xfm, load->children) {
                if (transform_applies(xfm, tpath)) {
                    if (transform == NULL || transform == xfm) {
                        transform = xfm;
                    } else {
                        const char *filename =
                            tpath + strlen(HERACLES_FILES_TREE) + 1;
                        transform_file_error(hera, "mxfm_save", filename,
                           "Lenses %s and %s could be used to save this file",
                                             xfm_lens_name(transform),
                                             xfm_lens_name(xfm));
                        ERR_REPORT(hera, HERA_EMXFM,
                                   "Path %s transformable by lens %s and %s",
                                   tpath,
                                   xfm_lens_name(transform),
                                   xfm_lens_name(xfm));
                        result = -1;
                    }
                }
            }
            if (transform != NULL) {
                int r = transform_save(hera, transform, tpath, t);
                if (r == -1)
                    result = -1;
            } else {
                if (tree_save(hera, t->children, tpath) == -1)
                    result = -1;
            }
            free(tpath);
        }
    }
    return result;
}

int tree_equal(const struct tree *t1, const struct tree *t2) {
    while (t1 != NULL && t2 != NULL) {
        if (!streqv(t1->label, t2->label))
            return 0;
        if (!streqv(t1->value, t2->value))
            return 0;
        if (! tree_equal(t1->children, t2->children))
            return 0;
        t1 = t1->next;
        t2 = t2->next;
    }
    return t1 == t2;
}


