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

heracles *hera_init(const char *loadpath, unsigned int flags);
void hera_close(heracles *hera);
int hera_set(struct heracles *hera, const char *path, const char *value); 
int hera_get(const struct heracles *hera, const char *path, const char **value);
int hera_rm(struct heracles *hera, const char *path);
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

/******************************************************************
 *                    Heracles Added stuff here                   *
 ******************************************************************/

struct error;
struct lns_error;
struct lens;
struct tree;

void reset_error(struct error *err);

struct tree * _hera_get(struct lens *lens, char *text, struct lns_error *err); 
char * _hera_put(struct lens *lens, struct tree *tree, char *text, struct lns_error *err);
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
