/*
 * heracles.h: public headers for heracles
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

#include <stdio.h>

#ifndef HERACLES_H_
#define HERACLES_H_

typedef struct heracles heracles;

/* Enum: hera_flags
 *
 * Flags to influence the behavior of Augeas. Pass a bitmask of these flags
 * to HERA_INIT.
 */
enum hera_flags {
    HERA_NONE = 0,
    HERA_SAVE_BACKUP  = (1 << 0),  /* Keep the original file with a
                                     .herasave extension */
    HERA_SAVE_NEWFILE = (1 << 1),  /* Save changes into a file with
                                     extension .heranew, and do not
                                     overwrite the original file. Takes
                                     precedence over HERA_SAVE_BACKUP */
    HERA_TYPE_CHECK   = (1 << 2),  /* Typecheck lenses; since it can be very
                                     expensive it is not done by default */
    HERA_NO_STDINC    = (1 << 3),   /* Do not use the builtin load path for
                                     modules */
    HERA_SAVE_NOOP    = (1 << 4),  /* Make save a no-op process, just record
                                     what would have changed */
    HERA_NO_LOAD      = (1 << 5),  /* Do not load the tree from HERA_INIT */
    HERA_NO_MODL_AUTOLOAD = (1 << 6),
    HERA_ENABLE_SPAN  = (1 << 7),  /* Track the span in the input of nodes */
    HERA_NO_ERR_CLOSE = (1 << 8),  /* Do not close automatically when
                                     encountering error during hera_init */
    HERA_TRACE_MODULE_LOADING = (1 << 9) /* For use by heraparse -t */
};

#ifdef __cplusplus
extern "C" {
#endif

/* Function: hera_init
 *
 * Initialize the library.
 *
 * Use ROOT as the filesystem root. If ROOT is NULL, use the value of the
 * environment variable HERACLES_ROOT. If that doesn't exist eitehr, use "/".
 *
 * LOADPATH is a colon-spearated list of directories that modules should be
 * searched in. This is in addition to the standard load path and the
 * directories in HERACLES_LENS_LIB
 *
 * FLAGS is a bitmask made up of values from HERA_FLAGS. The flag
 * HERA_NO_ERR_CLOSE can be used to get more information on why
 * initialization failed. If it is set in FLAGS, the caller must check that
 * hera_error returns HERA_NOERROR before using the returned heracles handle
 * for any other operation. If the handle reports any error, the caller
 * should only call the hera_error functions an hera_close on this handle.
 *
 * Returns:
 * a handle to the Augeas tree upon success. If initialization fails,
 * returns NULL if HERA_NO_ERR_CLOSE is not set in FLAGS. If
 * HERA_NO_ERR_CLOSE is set, might return an Augeas handle even on
 * failure. In that case, caller must check for errors using heracles_error,
 * and, if an error is reported, only use the handle with the hera_error
 * functions and hera_close.
 */
heracles *hera_init(const char *root, const char *loadpath, unsigned int flags);

/* Function: hera_defvar
 *
 * Define a variable NAME whose value is the result of evaluating EXPR. If
 * a variable NAME already exists, its name will be replaced with the
 * result of evaluating EXPR.  Context will not be applied to EXPR.
 *
 * If EXPR is NULL, the variable NAME will be removed if it is defined.
 *
 * Path variables can be used in path expressions later on by prefixing
 * them with '$'.
 *
 * Returns -1 on error; on success, returns 0 if EXPR evaluates to anything
 * other than a nodeset, and the number of nodes if EXPR evaluates to a
 * nodeset
 */
int hera_defvar(heracles *hera, const char *name, const char *expr);

/* Function: hera_defnode
 *
 * Define a variable NAME whose value is the result of evaluating EXPR,
 * which must be non-NULL and evaluate to a nodeset. If a variable NAME
 * already exists, its name will be replaced with the result of evaluating
 * EXPR.
 *
 * If EXPR evaluates to an empty nodeset, a node is created, equivalent to
 * calling HERA_SET(HERA, EXPR, VALUE) and NAME will be the nodeset containing
 * that single node.
 *
 * If CREATED is non-NULL, it is set to 1 if a node was created, and 0 if
 * it already existed.
 *
 * Returns -1 on error; on success, returns the number of nodes in the
 * nodeset
 */
int hera_defnode(heracles *hera, const char *name, const char *expr,
                const char *value, int *created);

/* Function: hera_get
 *
 * Lookup the value associated with PATH. VALUE can be NULL, in which case
 * it is ignored. If VALUE is not NULL, it is used to return a pointer to
 * the value associated with PATH if PATH matches exactly one node. If PATH
 * matches no nodes or more than one node, *VALUE is set to NULL. Note that
 * it is perfectly legal for nodes to have a NULL value, and that that by
 * itself does not indicate an error.
 *
 * The string *VALUE must not be freed by the caller, and is valid as long
 * as its node remains unchanged.
 *
 * Returns:
 * 1 if there is exactly one node matching PATH, 0 if there is none,
 * and a negative value if there is more than one node matching PATH, or if
 * PATH is not a legal path expression.
 */
int hera_get(const heracles *hera, const char *path, const char **value);

/* Function: hera_label
 *
 * Lookup the label associated with PATH. LABEL can be NULL, in which case
 * it is ignored. If LABEL is not NULL, it is used to return a pointer to
 * the value associated with PATH if PATH matches exactly one node. If PATH
 * matches no nodes or more than one node, *LABEL is set to NULL.
 *
 * The string *LABEL must not be freed by the caller, and is valid as long
 * as its node remains unchanged.
 *
 * Returns:
 * 1 if there is exactly one node matching PATH, 0 if there is none,
 * and a negative value if there is more than one node matching PATH, or if
 * PATH is not a legal path expression.
 */
int hera_label(const heracles *hera, const char *path, const char **label);

/* Function: hera_set
 *
 * Set the value associated with PATH to VALUE. VALUE is copied into the
 * internal data structure, and the caller is responsible for freeing
 * it. Intermediate entries are created if they don't exist.
 *
 * Returns:
 * 0 on success, -1 on error. It is an error if more than one node
 * matches PATH.
 */
int hera_set(heracles *hera, const char *path, const char *value);

/* Function: hera_setm
 *
 * Set the value of multiple nodes in one operation. Find or create a node
 * matching SUB by interpreting SUB as a path expression relative to each
 * node matching BASE. SUB may be NULL, in which case all the nodes
 * matching BASE will be modified.
 *
 * Returns:
 * number of modified nodes on success, -1 on error
 */
int hera_setm(heracles *hera, const char *base, const char *sub, const char *value);

/* Function: hera_span
 *
 * Get the span according to input file of the node associated with PATH. If
 * the node is associated with a file, the filename, label and value start and
 * end positions are set, and return value is 0. The caller is responsible for
 * freeing returned filename. If an argument for return value is NULL, then the
 * corresponding value is not set. If the node associated with PATH doesn't
 * belong to a file or is doesn't exists, filename and span are not set and
 * return value is -1.
 *
 * Returns:
 * 0 on success with filename, label_start, label_stop, value_start, value_end,
 *   span_start, span_end
 * -1 on error
 */

int hera_span(heracles *hera, const char *path, char **filename,
        unsigned int *label_start, unsigned int *label_end,
        unsigned int *value_start, unsigned int *value_end,
        unsigned int *span_start, unsigned int *span_end);

/* Function: hera_insert
 *
 * Create a new sibling LABEL for PATH by inserting into the tree just
 * before PATH if BEFORE == 1 or just after PATH if BEFORE == 0.
 *
 * PATH must match exactly one existing node in the tree, and LABEL must be
 * a label, i.e. not contain a '/', '*' or end with a bracketed index
 * '[N]'.
 *
 * Returns:
 * 0 on success, and -1 if the insertion fails.
 */
int hera_insert(heracles *hera, const char *path, const char *label, int before);

/* Function: hera_rm
 *
 * Remove path and all its children. Returns the number of entries removed.
 * All nodes that match PATH, and their descendants, are removed.
 */
int hera_rm(heracles *hera, const char *path);

/* Function: hera_mv
 *
 * Move the node SRC to DST. SRC must match exactly one node in the
 * tree. DST must either match exactly one node in the tree, or may not
 * exist yet. If DST exists already, it and all its descendants are
 * deleted. If DST does not exist yet, it and all its missing ancestors are
 * created.
 *
 * Note that the node SRC always becomes the node DST: when you move /a/b
 * to /x, the node /a/b is now called /x, no matter whether /x existed
 * initially or not.
 *
 * Returns:
 * 0 on success and -1 on failure.
 */
int hera_mv(heracles *hera, const char *src, const char *dst);

/* Function: hera_rename
 *
 * Rename the label of all nodes matching SRC to LBL.
 *
 * Returns:
 * The number of nodes renamed on success and -1 on failure.
 */
int hera_rename(heracles *hera, const char *src, const char *lbl);

/* Function: hera_match
 *
 * Returns:
 * the number of matches of the path expression PATH in HERA. If
 * MATCHES is non-NULL, an array with the returned number of elements will
 * be allocated and filled with the paths of the matches. The caller must
 * free both the array and the entries in it. The returned paths are
 * sufficiently qualified to make sure that they match exactly one node in
 * the current tree.
 *
 * If MATCHES is NULL, nothing is allocated and only the number
 * of matches is returned.
 *
 * Returns -1 on error, or the total number of matches (which might be 0).
 *
 * Path expressions:
 * Path expressions use a very simple subset of XPath: the path PATH
 * consists of a number of segments, separated by '/'; each segment can
 * either be a '*', matching any tree node, or a string, optionally
 * followed by an index in brackets, matching tree nodes labelled with
 * exactly that string. If no index is specified, the expression matches
 * all nodes with that label; the index can be a positive number N, which
 * matches exactly the Nth node with that label (counting from 1), or the
 * special expression 'last()' which matches the last node with the given
 * label. All matches are done in fixed positions in the tree, and nothing
 * matches more than one path segment.
 *
 */
int hera_match(const heracles *hera, const char *path, char ***matches);

/* Function: hera_save
 *
 * Write all pending changes to disk.
 *
 * Returns:
 * -1 if an error is encountered,
 * 0 on success. Only files that had any changes made to them are written.
 *
 * If HERA_SAVE_NEWFILE is set in the FLAGS passed to HERA_INIT, create
 * changed files as new files with the extension ".heranew", and leave the
 * original file unmodified.
 *
 * Otherwise, if HERA_SAVE_BACKUP is set in the FLAGS passed to HERA_INIT,
 * move the original file to a new file with extension ".herasave".
 *
 * If neither of these flags is set, overwrite the original file.
 */
int hera_save(heracles *hera);

/* Function: hera_text_store
 *
 * Use the value of node NODE as a string and transform it into a tree
 * using the lens LENS and store it in the tree at PATH, which will be
 * overwritten. PATH and NODE are path expressions.
 *
 * Returns:
 * 0 on success, or a negative value on failure
 */
int hera_text_store(heracles *hera, const char *lens, const char *node,
                   const char *path);

/* Function: hera_text_retrieve
 *
 * Transform the tree at PATH into a string using lens LENS and store it in
 * the node NODE_OUT, assuming the tree was initially generated using the
 * value of node NODE_IN. PATH, NODE_IN, and NODE_OUT are path expressions.
 *
 * Returns:
 * 0 on success, or a negative value on failure
 */
int hera_text_retrieve(struct heracles *hera, const char *lens,
                      const char *node_in, const char *path,
                      const char *node_out);

/* Function: hera_print
 *
 * Print each node matching PATH and its descendants to OUT.
 *
 * Returns:
 * 0 on success, or a negative value on failure
 */
int hera_print(const heracles *hera, FILE *out, const char *path);

/*
 * Function: hera_transform
 *
 * Add a transform for FILE using LENS.
 * EXCL specifies if this the file is to be included (0)
 * or excluded (1) from the LENS.
 * The LENS maybe be a module name or a full lens name.
 * If a module name is given, then lns will be the lens assumed.
 *
 * Returns:
 * 1 on success, -1 on failure
 */
int hera_transform(heracles *hera, const char *lens, const char *file, int excl);

/*
 * Function: hera_srun
 *
 * Run one or more newline-separated commands. The output of the commands
 * will be printed to OUT. Running just 'help' will print what commands are
 * available. Commands accepted by this are identical to what heratool
 * accepts.
 *
 * Returns:
 * the number of executed commands on success, -1 on failure, and -2 if a
 * 'quit' command was encountered
 */
int hera_srun(heracles *hera, FILE *out, const char *text);

/* Function: hera_close
 *
 * Close this Augeas instance and free any storage associated with
 * it. After running HERA_CLOSE, HERA is invalid and can not be used for any
 * more operations.
 */
void hera_close(heracles *hera);

/*
 * Error reporting
 */

typedef enum {
    HERA_NOERROR,        /* No error */
    HERA_ENOMEM,         /* Out of memory */
    HERA_EINTERNAL,      /* Internal error (bug) */
    HERA_EPATHX,         /* Invalid path expression */
    HERA_ENOMATCH,       /* No match for path expression */
    HERA_EMMATCH,        /* Too many matches for path expression */
    HERA_ESYNTAX,        /* Syntax error in lens file */
    HERA_ENOLENS,        /* Lens lookup failed */
    HERA_EMXFM,          /* Multiple transforms */
    HERA_ENOSPAN,        /* No span for this node */
    HERA_EMVDESC,        /* Cannot move node into its descendant */
    HERA_ECMDRUN,        /* Failed to execute command */
    HERA_EBADARG,        /* Invalid argument in funcion call */
    HERA_ELABEL          /* Invalid label */
} hera_errcode_t;

/* Return the error code from the last API call */
int hera_error(heracles *hera);

/* Return a human-readable message for the error code */
const char *hera_error_message(heracles *hera);

/* Return a human-readable message elaborating the error code; might be
 * NULL. For example, when the error code is HERA_EPATHX, this will explain
 * how the path expression is invalid */
const char *hera_error_minor_message(heracles *hera);

/* Return details about the error, which might be NULL. For example, for
 * HERA_EPATHX, indicates where in the path expression the error
 * occurred. The returned value can only be used until the next API call
 */
const char *hera_error_details(heracles *hera);


#ifdef __cplusplus
}
#endif

#endif


/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
