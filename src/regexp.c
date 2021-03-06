/*
 * regexp.c:
 *
 * Copyright (C) 2007, 2008 Red Hat Inc.
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

#include "syntax.h"
#include "memory.h"

static const struct string empty_pattern_string = {
    .ref = REF_MAX, .str = (char *) "()"
};

static const struct string *const empty_pattern = &empty_pattern_string;

char *regexp_escape(const struct regexp *r) {
    char *pat = escape(r->pattern->str, -1);

    if (pat == NULL)
        return NULL;

    /* Remove unneeded '()' from pat */
    for (int changed = 1; changed;) {
        changed = 0;
        for (char *p = pat; *p != '\0'; p++) {
            if (*p == '(' && p[1] == ')') {
                memmove(p, p+2, strlen(p+2)+1);
                changed = 1;
            }
        }
    }

    if (pat[0] == '(' && pat[strlen(pat)-1] == ')')
        memmove(pat, pat+1, strlen(pat+1)+1);
    pat[strlen(pat)-1] = '\0';

    return pat;
}

void print_regexp(FILE *out, struct regexp *r) {
    if (r == NULL) {
        fprintf(out, "<NULL>");
        return;
    }

    fputc('/', out);
    if (r->pattern == NULL)
        fprintf(out, "%p", r);
    else
        print_chars(out, r->pattern->str, -1);
    fputc('/', out);
}

struct regexp *make_regexp(struct info *info, char *pat) {
    struct regexp *regexp;

    make_ref(regexp);
    regexp->info = ref(info);

    make_ref(regexp->pattern);
    regexp->pattern->str = pat;
    return regexp;
}

void free_regexp(struct regexp *regexp) {
    if (regexp == NULL)
        return;
    assert(regexp->ref == 0);
    unref(regexp->info, info);
    unref(regexp->pattern, string);
    if (regexp->re != NULL) {
        regfree(regexp->re);
        free(regexp->re);
    }
    free(regexp);
}

int regexp_is_empty_pattern(struct regexp *r) {
    for (char *s = r->pattern->str; *s; s++) {
        if (*s != '(' && *s != ')')
            return 0;
    }
    return 1;
}

struct regexp *make_regexp_literal(struct info *info, const char *text) {
    char *pattern, *p;

    /* Escape special characters in text since it should be taken
       literally */
    CALLOC(pattern, 2*strlen(text)+1);
    p = pattern;
    for (const char *t = text; *t != '\0'; t++) {
        if ((*t == '\\') && t[1]) {
            *p++ = *t++;
            *p++ = *t;
        } else if (strchr(".|{}[]()+*?", *t) != NULL) {
            *p++ = '\\';
            *p++ = *t;
        } else {
            *p++ = *t;
        }
    }
    return make_regexp(info, pattern);
}

struct regexp *
regexp_union(struct info *info, struct regexp *r1, struct regexp *r2) {
    const char *p1 = r1->pattern->str;
    const char *p2 = r2->pattern->str;
    char *s;

    if (asprintf(&s, "(%s)|(%s)", p1, p2) == -1)
        return NULL;
    return make_regexp(info, s);
}

struct regexp *
regexp_union_n(struct info *info, int n, struct regexp **r) {
    size_t len = 0;
    char *pat, *p;

    for (int i=0; i < n; i++)
        if (r[i] != NULL)
            len += strlen(r[i]->pattern->str) + strlen("()|");

    if (len == 0)
        return NULL;

    if (ALLOC_N(pat, len) < 0)
        return NULL;

    p = pat;
    for (int i=0; i < n; i++) {
        if (r[i] == NULL)
            continue;
        if (i > 0)
            *p++ = '|';
        *p++ = '(';
        p = stpcpy(p, r[i]->pattern->str);
        *p++ = ')';
    }
    return make_regexp(info, pat);
}

struct regexp *
regexp_concat(struct info *info, struct regexp *r1, struct regexp *r2) {
    const char *p1 = r1->pattern->str;
    const char *p2 = r2->pattern->str;
    char *s;

    if (asprintf(&s, "(%s)(%s)", p1, p2) == -1)
        return NULL;
    return make_regexp(info, s);
}

struct regexp *
regexp_concat_n(struct info *info, int n, struct regexp **r) {
    size_t len = 0;
    char *pat, *p;

    for (int i=0; i < n; i++)
        if (r[i] != NULL)
            len += strlen(r[i]->pattern->str) + strlen("()");

    if (len == 0)
        return NULL;

    if (ALLOC_N(pat, len+1) < 0)
        return NULL;

    p = pat;
    for (int i=0; i < n; i++) {
        if (r[i] == NULL)
            continue;
        *p++ = '(';
        p = stpcpy(p, r[i]->pattern->str);
        *p++ = ')';
    }
    return make_regexp(info, pat);
}

struct regexp *
regexp_minus(struct info *info, struct regexp *r1, struct regexp *r2) {
    const char *p1 = r1->pattern->str;
    const char *p2 = r2->pattern->str;
    struct regexp *result = NULL;
    struct fa *fa = NULL, *fa1 = NULL, *fa2 = NULL;
    int r;
    char *s = NULL;
    size_t s_len;

    r = fa_compile(p1, strlen(p1), &fa1);
    if (r != REG_NOERROR)
        goto error;

    r = fa_compile(p2, strlen(p2), &fa2);
    if (r != REG_NOERROR)
        goto error;

    fa = fa_minus(fa1, fa2);
    if (fa == NULL)
        goto error;

    r = fa_as_regexp(fa, &s, &s_len);
    if (r < 0)
        goto error;

    if (s == NULL) {
        /* FA is the empty set, which we can't represent as a regexp */
        goto error;
    }

    result = make_regexp(info, s);
    s = NULL;

 done:
    fa_free(fa);
    fa_free(fa1);
    fa_free(fa2);
    free(s);
    return result;
 error:
    unref(result, regexp);
    goto done;
}


struct regexp *
regexp_iter(struct info *info, struct regexp *r, int min, int max) {
    const char *p;
    char *s;
    int ret = 0;

    if (r == NULL)
        return NULL;

    p = r->pattern->str;
    if ((min == 0 || min == 1) && max == -1) {
        char q = (min == 0) ? '*' : '+';
        ret = asprintf(&s, "(%s)%c", p, q);
    } else if (min == max) {
        ret = asprintf(&s, "(%s){%d}", p, min);
    } else {
        ret = asprintf(&s, "(%s){%d,%d}", p, min, max);
    }
    return (ret == -1) ? NULL : make_regexp(info, s);
}

struct regexp *
regexp_maybe(struct info *info, struct regexp *r) {
    const char *p;
    char *s;
    int ret;

    if (r == NULL)
        return NULL;
    p = r->pattern->str;
    ret = asprintf(&s, "(%s)?", p);
    return (ret == -1) ? NULL : make_regexp(info, s);
}

struct regexp *regexp_make_empty(struct info *info) {
    struct regexp *regexp;

    make_ref(regexp);
    if (regexp != NULL) {
        regexp->info = ref(info);
        /* Casting away the CONST for EMPTY_PATTERN is ok since it
           is protected against changes because REF == REF_MAX */
        regexp->pattern = (struct string *) empty_pattern;
    }
    return regexp;
}

int regexp_compile(struct regexp *r) {
    /* See the GNU regex manual or regex.h in gnulib for
     * an explanation of these flags. They are set so that the regex
     * matcher interprets regular expressions the same way that libfa
     * does
     */
    static const reg_syntax_t syntax =
        RE_CONTEXT_INDEP_OPS|RE_CONTEXT_INVALID_OPS|RE_DOT_NOT_NULL
        |RE_INTERVALS|RE_NO_BK_BRACES|RE_NO_BK_PARENS|RE_NO_BK_REFS
        |RE_NO_BK_VBAR|RE_NO_EMPTY_RANGES
        |RE_NO_POSIX_BACKTRACKING|RE_CONTEXT_INVALID_DUP|RE_NO_GNU_OPS;
    reg_syntax_t old_syntax = re_syntax_options;
    const char *c = NULL;

    if (r->re == NULL)
        CALLOC(r->re, 1);

    re_syntax_options = syntax;
    c = re_compile_pattern(r->pattern->str, strlen(r->pattern->str), r->re);
    re_syntax_options = old_syntax;

    r->re->regs_allocated = REGS_REALLOCATE;
    if (c != NULL) {
        char *p = escape(r->pattern->str, -1);
        syntax_error(r->info, "invalid regexp /%s/: %s", p, c);
        free(p);
        return -1;
    }
    return 0;
}

int regexp_match(struct regexp *r,
                 const char *string, const int size,
                 const int start, struct re_registers *regs) {
    if (r->re == NULL) {
        if (regexp_compile(r) == -1)
            return -3;
    }
    return re_match(r->re, string, size, start, regs);
}

int regexp_matches_empty(struct regexp *r) {
    return regexp_match(r, "", 0, 0, NULL) == 0;
}

int regexp_nsub(struct regexp *r) {
    if (r->re == NULL)
        if (regexp_compile(r) == -1)
            return -1;
    return r->re->re_nsub;
}

void regexp_release(struct regexp *regexp) {
    if (regexp != NULL && regexp->re != NULL) {
        regfree(regexp->re);
        FREE(regexp->re);
    }
}

/*
 * Local variables:
 *  indent-tabs-mode: nil
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
