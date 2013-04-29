/*
 * test-api.c: test public API functions for conformance
 *
 * Copyright (C) 2009-2011 Red Hat Inc.
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

#include <config.h>

#include "heracles.h"

#include "cutest.h"
#include "internal.h"

#include <unistd.h>

#include <libxml/tree.h>

static const char *abs_top_srcdir;
static char *root;
static char *loadpath;

#define die(msg)                                                    \
    do {                                                            \
        fprintf(stderr, "%d: Fatal error: %s\n", __LINE__, msg);    \
        exit(EXIT_FAILURE);                                         \
    } while(0)

static void testGet(CuTest *tc) {
    int r;
    const char *value;
    const char *label;
    struct heracles *hera;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    CuAssertPtrNotNull(tc, hera);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* Make sure we're looking at the right thing */
    r = hera_match(hera, "/heracles/version/save/*", NULL);
    CuAssertTrue(tc, r > 1);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* hera_get returns 1 and the value if exactly one node matches */
    r = hera_get(hera, "/heracles/version/save/*[1]", &value);
    CuAssertIntEquals(tc, 1, r);
    CuAssertPtrNotNull(tc, value);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* hera_get returns 0 and no value when no node matches */
    r = hera_get(hera, "/heracles/version/save/*[ last() + 1 ]", &value);
    CuAssertIntEquals(tc, 0, r);
    CuAssertPtrEquals(tc, NULL, value);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* hera_get should return an error when multiple nodes match */
    r = hera_get(hera, "/heracles/version/save/*", &value);
    CuAssertIntEquals(tc, -1, r);
    CuAssertPtrEquals(tc, NULL, value);
    CuAssertIntEquals(tc, HERA_EMMATCH, hera_error(hera));

    /* hera_label returns 1 and the label if exactly one node matches */
    r = hera_label(hera, "/heracles/version/save/*[1]", &label);
    CuAssertIntEquals(tc, 1, r);
    CuAssertPtrNotNull(tc, label);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* hera_label returns 0 and no label when no node matches */
    r = hera_label(hera, "/heracles/version/save/*[ last() + 1 ]", &label);
    CuAssertIntEquals(tc, 0, r);
    CuAssertPtrEquals(tc, NULL, label);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* hera_label should return an error when multiple nodes match */
    r = hera_label(hera, "/heracles/version/save/*", &label);
    CuAssertIntEquals(tc, -1, r);
    CuAssertPtrEquals(tc, NULL, label);
    CuAssertIntEquals(tc, HERA_EMMATCH, hera_error(hera));

    /* heracles should prepend context if relative path given */
    r = hera_set(hera, "/heracles/context", "/heracles/version");
    r = hera_get(hera, "save/*[1]", &value);
    CuAssertIntEquals(tc, 1, r);
    CuAssertPtrNotNull(tc, value);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* heracles should still work with an empty context */
    r = hera_set(hera, "/heracles/context", "");
    r = hera_get(hera, "/heracles/version", &value);
    CuAssertIntEquals(tc, 1, r);
    CuAssertPtrNotNull(tc, value);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* heracles should ignore trailing slashes in context */
    r = hera_set(hera, "/heracles/context", "/heracles/version/");
    r = hera_get(hera, "save/*[1]", &value);
    CuAssertIntEquals(tc, 1, r);
    CuAssertPtrNotNull(tc, value);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* heracles should create non-existent context path */
    r = hera_set(hera, "/heracles/context", "/context/foo");
    r = hera_set(hera, "bar", "value");
    r = hera_get(hera, "/context/foo/bar", &value);
    CuAssertIntEquals(tc, 1, r);
    CuAssertPtrNotNull(tc, value);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    hera_close(hera);
}

static void testSet(CuTest *tc) {
    int r;
    const char *value;
    struct heracles *hera;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    CuAssertPtrNotNull(tc, hera);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* hera_set returns 0 for a simple set */
    r = hera_set(hera, "/heracles/testSet", "foo");
    CuAssertIntEquals(tc, 0, r);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* hera_set returns -1 when cannot set due to multiple nodes */
    r = hera_set(hera, "/heracles/version/save/*", "foo");
    CuAssertIntEquals(tc, -1, r);
    CuAssertIntEquals(tc, HERA_EMMATCH, hera_error(hera));

    /* hera_set is able to set the context, even when currently invalid */
    r = hera_set(hera, "/heracles/context", "( /files | /heracles )");
    CuAssertIntEquals(tc, 0, r);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));
    r = hera_get(hera, "/heracles/version", &value);
    CuAssertIntEquals(tc, -1, r);
    CuAssertIntEquals(tc, HERA_EMMATCH, hera_error(hera));
    r = hera_set(hera, "/heracles/context", "/files");
    CuAssertIntEquals(tc, 0, r);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    hera_close(hera);
}

static void testSetM(CuTest *tc) {
    int r;
    struct heracles *hera;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    CuAssertPtrNotNull(tc, hera);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    /* Change base nodes when SUB is NULL */
    r = hera_setm(hera, "/heracles/version/save/*", NULL, "changed");
    CuAssertIntEquals(tc, 4, r);

    r = hera_match(hera, "/heracles/version/save/*[. = 'changed']", NULL);
    CuAssertIntEquals(tc, 4, r);

    /* Only change existing nodes */
    r = hera_setm(hera, "/heracles/version/save", "mode", "again");
    CuAssertIntEquals(tc, 4, r);

    r = hera_match(hera, "/heracles/version/save/*", NULL);
    CuAssertIntEquals(tc, 4, r);

    r = hera_match(hera, "/heracles/version/save/*[. = 'again']", NULL);
    CuAssertIntEquals(tc, 4, r);

    /* Create a new node */
    r = hera_setm(hera, "/heracles/version/save", "mode[last() + 1]", "newmode");
    CuAssertIntEquals(tc, 1, r);

    r = hera_match(hera, "/heracles/version/save/*", NULL);
    CuAssertIntEquals(tc, 5, r);

    r = hera_match(hera, "/heracles/version/save/*[. = 'again']", NULL);
    CuAssertIntEquals(tc, 4, r);

    r = hera_match(hera, "/heracles/version/save/*[last()][. = 'newmode']", NULL);
    CuAssertIntEquals(tc, 1, r);

    /* Noexistent base */
    r = hera_setm(hera, "/heracles/version/save[last()+1]", "mode", "newmode");
    CuAssertIntEquals(tc, 0, r);

    /* Invalid path expressions */
    r = hera_setm(hera, "/heracles/version/save[]", "mode", "invalid");
    CuAssertIntEquals(tc, -1, r);

    r = hera_setm(hera, "/heracles/version/save/*", "mode[]", "invalid");
    CuAssertIntEquals(tc, -1, r);

    hera_close(hera);
}

/* Check that defining a variable leads to a corresponding entry in
 * /heracles/variables and that that entry disappears when the variable is
 * undefined */
static void testDefVarMeta(CuTest *tc) {
    int r;
    struct heracles *hera;
    static const char *const expr = "/heracles/version/save/mode";
    const char *value;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    CuAssertPtrNotNull(tc, hera);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    r = hera_defvar(hera, "var", expr);
    CuAssertIntEquals(tc, 4, r);

    r = hera_match(hera, "/heracles/variables/*", NULL);
    CuAssertIntEquals(tc, 1, r);

    r = hera_get(hera, "/heracles/variables/var", &value);
    CuAssertStrEquals(tc, expr, value);

    r = hera_defvar(hera, "var", NULL);
    CuAssertIntEquals(tc, 0, r);

    r = hera_match(hera, "/heracles/variables/*", NULL);
    CuAssertIntEquals(tc, 0, r);

    hera_close(hera);
}

/* Check that defining a variable with defnode leads to a corresponding
 * entry in /heracles/variables and that that entry disappears when the
 * variable is undefined
 */
static void testDefNodeExistingMeta(CuTest *tc) {
    int r, created;
    struct heracles *hera;
    static const char *const expr = "/heracles/version/save/mode";
    const char *value;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    CuAssertPtrNotNull(tc, hera);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    r = hera_defnode(hera, "var", expr, "other", &created);
    CuAssertIntEquals(tc, 4, r);
    CuAssertIntEquals(tc, 0, created);

    r = hera_match(hera, "/heracles/variables/*", NULL);
    CuAssertIntEquals(tc, 1, r);

    r = hera_get(hera, "/heracles/variables/var", &value);
    CuAssertStrEquals(tc, expr, value);

    r = hera_defvar(hera, "var", NULL);
    CuAssertIntEquals(tc, 0, r);

    r = hera_match(hera, "/heracles/variables/*", NULL);
    CuAssertIntEquals(tc, 0, r);

    hera_close(hera);
}

/* Check that defining a variable with defnode leads to a corresponding
 * entry in /heracles/variables and that that entry disappears when the
 * variable is undefined
 */
static void testDefNodeCreateMeta(CuTest *tc) {
    int r, created;
    struct heracles *hera;
    static const char *const expr = "/heracles/version/save/mode[last()+1]";
    static const char *const expr_can = "/heracles/version/save/mode[5]";
    const char *value;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    CuAssertPtrNotNull(tc, hera);
    CuAssertIntEquals(tc, HERA_NOERROR, hera_error(hera));

    r = hera_defnode(hera, "var", expr, "other", &created);
    CuAssertIntEquals(tc, 1, r);
    CuAssertIntEquals(tc, 1, created);

    r = hera_match(hera, "/heracles/variables/*", NULL);
    CuAssertIntEquals(tc, 1, r);

    r = hera_get(hera, "/heracles/variables/var", &value);
    CuAssertStrEquals(tc, expr_can, value);

    r = hera_defvar(hera, "var", NULL);
    CuAssertIntEquals(tc, 0, r);

    r = hera_match(hera, "/heracles/variables/*", NULL);
    CuAssertIntEquals(tc, 0, r);

    hera_close(hera);
}

static void reset_indexes(uint *a, uint *b, uint *c, uint *d, uint *e, uint *f) {
    *a = 0; *b = 0; *c = 0; *d = 0; *e = 0; *f = 0;
}

#define SPAN_TEST_DEF_LAST { .expr = NULL, .ls = 0, .le = 0, \
        .vs = 0, .ve = 0, .ss = 0, .se = 0 }

struct span_test_def {
    const char *expr;
    const char *f;
    int ret;
    int ls;
    int le;
    int vs;
    int ve;
    int ss;
    int se;
};

static const struct span_test_def span_test[] = {
    { .expr = "/files/etc/hosts/1/ipaddr", .f = "hosts", .ret = 0, .ls = 0, .le = 0, .vs = 104, .ve = 113, .ss = 104, .se = 113 },
    { .expr = "/files/etc/hosts/1", .f = "hosts", .ret = 0, .ls = 0, .le = 0, .vs = 0, .ve = 0, .ss = 104, .se = 171 },
    { .expr = "/files/etc/hosts/*[last()]", .f = "hosts", .ret = 0, .ls = 0, .le = 0, .vs = 0, .ve = 0, .ss = 266, .se = 309 },
    { .expr = "/files/etc/hosts/#comment[2]", .f = "hosts", .ret = 0, .ls = 0, .le = 0, .vs = 58, .ve = 103, .ss = 56, .se = 104 },
    { .expr = "/files/etc/hosts", .f = "hosts", .ret = 0, .ls = 0, .le = 0, .vs = 0, .ve = 0, .ss = 0, .se = 309 },
    { .expr = "/files", .f = NULL, .ret = -1, .ls = 0, .le = 0, .vs = 0, .ve = 0, .ss = 0, .se = 0 },
    { .expr = "/random", .f = NULL, .ret = -1, .ls = 0, .le = 0, .vs = 0, .ve = 0, .ss = 0, .se = 0 },
    SPAN_TEST_DEF_LAST
};

static void testNodeInfo(CuTest *tc) {
    int ret;
    int i = 0;
    struct heracles *hera;
    struct span_test_def test;
    char *fbase;
    char msg[1024];
    static const char *const expr = "/files/etc/hosts/1/ipaddr";

    char *filename_ac;
    uint label_start, label_end, value_start, value_end, span_start, span_end;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD|HERA_ENABLE_SPAN);
    ret = hera_load(hera);
    CuAssertRetSuccess(tc, ret);

    while(span_test[i].expr != NULL) {
        test = span_test[i];
        i++;
        ret = hera_span(hera, test.expr, &filename_ac, &label_start, &label_end,
                     &value_start, &value_end, &span_start, &span_end);
        sprintf(msg, "span_test %d ret\n", i);
        CuAssertIntEquals_Msg(tc, msg, test.ret, ret);
        sprintf(msg, "span_test %d label_start\n", i);
        CuAssertIntEquals_Msg(tc, msg, test.ls, label_start);
        sprintf(msg, "span_test %d label_end\n", i);
        CuAssertIntEquals_Msg(tc, msg, test.le, label_end);
        sprintf(msg, "span_test %d value_start\n", i);
        CuAssertIntEquals_Msg(tc, msg, test.vs, value_start);
        sprintf(msg, "span_test %d value_end\n", i);
        CuAssertIntEquals_Msg(tc, msg, test.ve, value_end);
        sprintf(msg, "span_test %d span_start\n", i);
        CuAssertIntEquals_Msg(tc, msg, test.ss, span_start);
        sprintf(msg, "span_test %d span_end\n", i);
        CuAssertIntEquals_Msg(tc, msg, test.se, span_end);
        if (filename_ac != NULL) {
            fbase = basename(filename_ac);
        } else {
            fbase = NULL;
        }
        sprintf(msg, "span_test %d filename\n", i);
        CuAssertStrEquals_Msg(tc, msg, test.f, fbase);
        free(filename_ac);
        filename_ac = NULL;
        reset_indexes(&label_start, &label_end, &value_start, &value_end,
                      &span_start, &span_end);
    }

    /* hera_span returns -1 and when no node matches */
    ret = hera_span(hera, "/files/etc/hosts/*[ last() + 1 ]", &filename_ac,
            &label_start, &label_end, &value_start, &value_end,
            &span_start, &span_end);
    CuAssertIntEquals(tc, -1, ret);
    CuAssertPtrEquals(tc, NULL, filename_ac);
    CuAssertIntEquals(tc, HERA_ENOMATCH, hera_error(hera));

    /* hera_span should return an error when multiple nodes match */
    ret = hera_span(hera, "/files/etc/hosts/*", &filename_ac,
            &label_start, &label_end, &value_start, &value_end,
            &span_start, &span_end);
    CuAssertIntEquals(tc, -1, ret);
    CuAssertPtrEquals(tc, NULL, filename_ac);
    CuAssertIntEquals(tc, HERA_EMMATCH, hera_error(hera));

    /* hera_span returns -1 if nodes span are not loaded */
    hera_close(hera);
    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    ret = hera_load(hera);
    CuAssertRetSuccess(tc, ret);
    ret = hera_span(hera, expr, &filename_ac, &label_start, &label_end,
                 &value_start, &value_end, &span_start, &span_end);
    CuAssertIntEquals(tc, -1, ret);
    CuAssertPtrEquals(tc, NULL, filename_ac);
    CuAssertIntEquals(tc, HERA_ENOSPAN, hera_error(hera));
    reset_indexes(&label_start, &label_end, &value_start, &value_end,
                  &span_start, &span_end);

    hera_close(hera);
}

static void testMv(CuTest *tc) {
    struct heracles *hera;
    int r;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    CuAssertPtrNotNull(tc, hera);

    r = hera_set(hera, "/a/b/c", "value");
    CuAssertRetSuccess(tc, r);

    r = hera_mv(hera, "/a/b/c", "/a/b/c/d");
    CuAssertIntEquals(tc, -1, r);
    CuAssertIntEquals(tc, HERA_EMVDESC, hera_error(hera));

    hera_close(hera);
}


static void testRename(CuTest *tc) {
    struct heracles *hera;
    int r;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    CuAssertPtrNotNull(tc, hera);

    r = hera_set(hera, "/a/b/c", "value");
    CuAssertRetSuccess(tc, r);

    r = hera_rename(hera, "/a/b/c", "d");
    CuAssertIntEquals(tc, 1, r);

    r = hera_set(hera, "/a/e/d", "value2");
    CuAssertRetSuccess(tc, r);

    // Multiple rename
    r = hera_rename(hera, "/a//d", "x");
    CuAssertIntEquals(tc, 2, r);

    // Label with a /
    r = hera_rename(hera, "/a/e/x", "a/b");
    CuAssertIntEquals(tc, -1, r);
    CuAssertIntEquals(tc, HERA_ELABEL, hera_error(hera));

    hera_close(hera);
}

static void testToXml(CuTest *tc) {
    struct heracles *hera;
    int r;
    xmlNodePtr xmldoc;
    const xmlChar *value;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    r = hera_load(hera);
    CuAssertRetSuccess(tc, r);

    r = hera_to_xml(hera, "/files/etc/passwd", &xmldoc, 0);
    CuAssertRetSuccess(tc, r);

    value = xmlGetProp(xmldoc, BAD_CAST "match");
    CuAssertStrEquals(tc, "/files/etc/passwd", (const char*)value);

    xmldoc = xmlFirstElementChild(xmldoc);
    value = xmlGetProp(xmldoc, BAD_CAST "label");
    CuAssertStrEquals(tc, "passwd", (const char*)value);

    value = xmlGetProp(xmldoc, BAD_CAST "path");
    CuAssertStrEquals(tc, "/files/etc/passwd", (const char*)value);

    xmldoc = xmlFirstElementChild(xmldoc);
    value = xmlGetProp(xmldoc, BAD_CAST "label");
    CuAssertStrEquals(tc, "root", (const char*)value);

    /* Bug #239 */
    r = hera_set(hera, "/heracles/context", "/files/etc/passwd");
    CuAssertRetSuccess(tc, r);
    r = hera_to_xml(hera, ".", &xmldoc, 0);
    CuAssertRetSuccess(tc, r);
    xmldoc = xmlFirstElementChild(xmldoc);
    value = xmlGetProp(xmldoc, BAD_CAST "label");
    CuAssertStrEquals(tc, "passwd", (const char*)value);

    hera_close(hera);
}

static void testTextStore(CuTest *tc) {
    static const char *const hosts = "192.168.0.1 rtr.example.com router\n";
    /* Not acceptable for Hosts.lns - missing canonical and \n */
    static const char *const hosts_bad = "192.168.0.1";
    const char *v;

    struct heracles *hera;
    int r;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    CuAssertPtrNotNull(tc, hera);

    r = hera_set(hera, "/raw/hosts", hosts);
    CuAssertRetSuccess(tc, r);

    r = hera_text_store(hera, "Hosts.lns", "/raw/hosts", "/t1");
    CuAssertRetSuccess(tc, r);

    r = hera_match(hera, "/t1/*", NULL);
    CuAssertIntEquals(tc, 1, r);

    /* Test bad lens name */
    r = hera_text_store(hera, "Notthere.lns", "/raw/hosts", "/t2");
    CuAssertIntEquals(tc, -1, r);
    CuAssertIntEquals(tc, HERA_ENOLENS, hera_error(hera));

    r = hera_match(hera, "/t2", NULL);
    CuAssertIntEquals(tc, 0, r);

    /* Test parse error */
    r = hera_set(hera, "/raw/hosts_bad", hosts_bad);
    CuAssertRetSuccess(tc, r);

    r = hera_text_store(hera, "Hosts.lns", "/raw/hosts_bad", "/t3");
    CuAssertIntEquals(tc, -1, r);

    r = hera_match(hera, "/t3", NULL);
    CuAssertIntEquals(tc, 0, r);

    r = hera_get(hera, "/heracles/text/t3/error", &v);
    CuAssertIntEquals(tc, 1, r);
    CuAssertStrEquals(tc, "parse_failed", v);

    r = hera_text_store(hera, "Hosts.lns", "/raw/hosts", "/t3");
    CuAssertRetSuccess(tc, r);

    r = hera_match(hera, "/heracles/text/t3/error", NULL);
    CuAssertIntEquals(tc, 0, r);

    /* Test invalid PATH */
    r = hera_text_store(hera, "Hosts.lns", "/raw/hosts", "[garbage]");
    CuAssertIntEquals(tc, -1, r);
    CuAssertIntEquals(tc, HERA_EPATHX, hera_error(hera));

    r = hera_match(hera, "/t2", NULL);
    CuAssertIntEquals(tc, 0, r);
}

static void testTextRetrieve(CuTest *tc) {
    static const char *const hosts = "192.168.0.1 rtr.example.com router\n";
    const char *hosts_out;
    struct heracles *hera;
    int r;

    hera = hera_init(root, loadpath, HERA_NO_STDINC|HERA_NO_LOAD);
    CuAssertPtrNotNull(tc, hera);

    r = hera_set(hera, "/raw/hosts", hosts);
    CuAssertRetSuccess(tc, r);

    r = hera_text_store(hera, "Hosts.lns", "/raw/hosts", "/t1");
    CuAssertRetSuccess(tc, r);

    r = hera_text_retrieve(hera, "Hosts.lns", "/raw/hosts", "/t1", "/out/hosts");
    CuAssertRetSuccess(tc, r);

    r = hera_get(hera, "/out/hosts", &hosts_out);
    CuAssertIntEquals(tc, 1, r);

    CuAssertStrEquals(tc, hosts, hosts_out);
}

int main(void) {
    char *output = NULL;
    CuSuite* suite = CuSuiteNew();
    CuSuiteSetup(suite, NULL, NULL);

    SUITE_ADD_TEST(suite, testGet);
    SUITE_ADD_TEST(suite, testSet);
    SUITE_ADD_TEST(suite, testSetM);
    SUITE_ADD_TEST(suite, testDefVarMeta);
    SUITE_ADD_TEST(suite, testDefNodeExistingMeta);
    SUITE_ADD_TEST(suite, testDefNodeCreateMeta);
    SUITE_ADD_TEST(suite, testNodeInfo);
    SUITE_ADD_TEST(suite, testMv);
    SUITE_ADD_TEST(suite, testRename);
    SUITE_ADD_TEST(suite, testToXml);
    SUITE_ADD_TEST(suite, testTextStore);
    SUITE_ADD_TEST(suite, testTextRetrieve);

    abs_top_srcdir = getenv("abs_top_srcdir");
    if (abs_top_srcdir == NULL)
        die("env var abs_top_srcdir must be set");

    if (asprintf(&root, "%s/tests/root", abs_top_srcdir) < 0) {
        die("failed to set root");
    }

    if (asprintf(&loadpath, "%s/lenses", abs_top_srcdir) < 0) {
        die("failed to set loadpath");
    }

    CuSuiteRun(suite);
    CuSuiteSummary(suite, &output);
    CuSuiteDetails(suite, &output);
    printf("%s\n", output);
    free(output);
    return suite->failCount;
}

/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
