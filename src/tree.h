#include "heracles.h"
#include "internal.h"
#include "memory.h"
#include "syntax.h"
#include "transform.h"
#include "errcode.h"

#include <fnmatch.h>
#include <argz.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#ifndef TREE_STUFF
#define TREE_STUFF

#define TREE_HIDDEN(tree) ((tree)->label == NULL)

int find_one_node(struct pathx *p, struct tree **match); 

void tree_mark_dirty(struct tree *tree);

void tree_clean(struct tree *tree);

struct tree *tree_child(struct tree *tree, const char *label) ;

struct tree *tree_child_cr(struct tree *tree, const char *label) ;

struct tree *tree_path_cr(struct tree *tree, int n, ...) ;

struct tree *tree_find(struct heracles *hera, const char *path) ;

struct tree *tree_find_cr(struct heracles *hera, const char *path) ;

void tree_store_value(struct tree *tree, char **value) ;

int tree_set_value(struct tree *tree, const char *value) ;

struct tree *tree_root_ctx(const struct heracles *hera) ;

struct tree *tree_append(struct tree *parent, char *label, char *value) ;

struct tree *tree_append_s(struct tree *parent, const char *l0, char *v) ;

struct tree *tree_from_transform(struct heracles *hera, const char *modname, struct transform *xfm) ;

void tree_unlink_children(struct heracles *hera, struct tree *tree) ;

void tree_rm_dirty_files(struct heracles *hera, struct tree *tree) ;

void tree_rm_dirty_leaves(struct heracles *hera, struct tree *tree, struct tree *protect) ;

struct tree *tree_set(struct pathx *p, const char *value) ;

int tree_insert(struct pathx *p, const char *label, int before) ;

struct tree *make_tree(char *label, char *value, struct tree *parent, struct tree *children) ;

struct tree *make_tree_origin(struct tree *root) ;

void free_tree_node(struct tree *tree) ;

int free_tree(struct tree *tree) ;

int tree_unlink(struct tree *tree) ;

int tree_rm(struct pathx *p) ;

int tree_replace(struct heracles *hera, const char *path, struct tree *sub) ;

int tree_save(struct heracles *hera, struct tree *tree, const char *path) ;

int tree_equal(const struct tree *t1, const struct tree *t2) ;

#endif
