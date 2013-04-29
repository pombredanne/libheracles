# ifndef TREE_LABELS
# define TREE_LABELS
/* Some popular labels that we use in /heracles */
#define s_heracles "heracles"
#define s_files  "files"
#define s_load   "load"
#define s_pathx  "pathx"
#define s_error  "error"
#define s_pos    "pos"
#define s_vars   "variables"
#define s_lens   "lens"
#define s_excl   "excl"
#define s_incl   "incl"

/* Loaded files are tracked underneath METATREE. When a file with name
 * FNAME is loaded, certain entries are made under METATREE / FNAME:
 *   path      : path where tree for FNAME is put
 *   mtime     : time of last modification of the file as reported by stat(2)
 *   lens/info : information about where the applied lens was loaded from
 *   lens/id   : unique hexadecimal id of the lens
 *   error     : indication of errors during processing FNAME, or NULL
 *               if processing succeeded
 *   error/pos : position in file where error occured (for get errors)
 *   error/path: path to tree node where error occurred (for put errors)
 *   error/message : human-readable error message
 */

#define s_path "path"
#define s_info "info"
#define s_mtime "mtime"

/* These are all put underneath "error" */
#define s_message "message"
#define s_line    "line"
#define s_char    "char"
#endif
