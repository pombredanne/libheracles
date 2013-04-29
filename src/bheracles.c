#include <heracles.h>
#include <heracles.h>
#include "lens.h"
#include "internal.h"
#include "parser.h"
#include "errcode.h"
#include "ref.h"

struct heracles {
    void * modules;
    struct heracles_error error;
    size_t  nmodpath;
    char   *modpathz;
    struct heracles_error * error;
}


struct heracles_error {
    hera_errcode_t  code;
    int            minor;
    char          *details;       /* Human readable explanation */
    const char    *minor_details; /* Human readable version of MINOR */
    /* A dummy info of last resort; this can be used in places where
     * a struct info is needed but none available
     */
    struct info   *info;
    /* Bit of a kludge to get at struct heracles, but since struct error
     * is now available in a lot of places (through struct info), this
     * gives a convenient way to get at the overall state
     */
    const struct heracles *hera;
    /* A preallocated exception so that we can throw something, even
     * under OOM conditions
     */
    struct value *exn;
};

typedef void *yyscan_t;


struct heracles_error {
    int code;
    char * text;
}

int heracles_load_module(char * name, struct module ** module)
{
  yyscan_t          scanner;
  struct state      state;
  struct string  *sname = NULL;
  struct info    info;
  int result = -1;
  int r;
  struct term *term = NULL;

  r = make_ref(sname);
  ERR_NOMEM(r < 0, hera);

  sname->str = strdup(name);
  ERR_NOMEM(sname->str == NULL, hera);

  MEMZERO(&info, 1);
  info.ref = UINT_MAX;
  info.filename = sname;
  info.error = hera->error;

  MEMZERO(&state, 1);
  state.info = &info;
  state.comment_depth = 0;

  if (heral_init_lexer(&state, &scanner) < 0) {
    heral_error(&info, term, NULL, "file not found");
    goto error;
  }

  yydebug = getenv("YYDEBUG") != NULL;
  r = heral_parse(term, scanner);
  heral_close_lexer(scanner);
  heral_lex_destroy(scanner);
  if (r == 1) {
    heral_error(&info, term, NULL, "syntax error");
    goto error;
  } else if (r == 2) {
    heral_error(&info, term, NULL, "parser ran out of memory");
    ERR_NOMEM(1, hera);
  }

  if (! typecheck(term, hera))
    goto error;

  *module = compile(term, hera);
  
  result = 0;

 error:
  unref(sname, string);
  unrer(term, term);
  // free TERM
  return result;
}

}
int heracles_init() {}
int heracles_parse_file(struct lens * lens, char * filename, struct tree ** tree) {}
