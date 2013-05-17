/*
 * Copyright (C) 2009-2011 David Lutterkort
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
 * Author: David Lutterkort <lutter@redhat.com>
 */

#ifndef TRANSFORM_H_
#define TRANSFORM_H_

/*
 * Transformers for going from file globs to path names in the tree
 * functions are in transform.c
 */

/* Filters for globbing files */
struct filter {
    unsigned int   ref;
    struct filter *next;
    struct string *glob;
    unsigned int   include : 1;
};

struct filter *make_filter(struct string *glb, unsigned int include);
void free_filter(struct filter *filter);

/* Transformers that actually run lenses on contents of files */
struct transform {
    unsigned int      ref;
    struct lens      *lens;
    struct filter    *filter;
};

struct transform *make_transform(struct lens *lens, struct filter *filter);
void free_transform(struct transform *xform);

#endif


/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
