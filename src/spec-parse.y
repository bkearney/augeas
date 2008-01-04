%{

#include "internal.h"
#include "ast.h"
#include "list.h"
#include <stdio.h>

#define YYDEBUG 1

int spec_parse_file(const char *name, struct grammar **grammars,
                    struct map **maps);

/* AST cosntruction */
static struct abbrev *make_abbrev(const char *name, struct literal *literal,
                                  const char *deflt, int lineno);

static struct rule *make_rule(const char *name, struct match *matches, 
                              struct action *actions, int lineno);

static struct match* make_match_list(struct match *head, struct match *tail,
                                     enum match_type type, int lineno);
static struct match *make_literal_match(struct literal *literal, int lineno);
static struct match *make_name_match(const char *name, enum quant quant, 
                                     int lineno);
static struct match *make_match(enum match_type type, int lineno);
static struct match *make_any_match(int epsilon, int lineno);
static struct match *make_field_match(int field, int lineno);
 static struct action *make_action(enum action_scope scope, int id,
                                   struct entry *path, 
                                  struct entry *value, int lineno);
static struct entry *make_entry(enum entry_type type, int lineno);

static struct map *make_map(const char *filename,
                            const char *grammar_name, struct filter *filters,
                            int lineno);

static struct filter *make_filter(const char *glob, int lineno);

static struct grammar *make_grammar(const char *filename,
                                    const char *name, struct abbrev *abbrevs,
                                    struct rule *rules, int lineno);

typedef void *yyscan_t;
%}
%locations
%pure-parser
%parse-param    {struct grammar **grammars}
%parse-param    {struct map     **maps}
%parse-param    {yyscan_t scanner}
%lex-param      {yyscan_t scanner}

%token <intval> T_FIELD    /* $3 */
%token <string> T_QUOTED   /* 'foo' */
%token <string> T_REGEX    /* '/[ \t]+/' */
%token <string> T_NAME     /* [a-z]+ */
%token <string> T_NAME_COLON
%token <string> T_GLOBAL   /* '$seq' or '$file_name' */
%token <intval> T_ANY      /* '...' or '..?' */
%token <intval> T_NUMBER

 /* Keywords */
%token          T_GRAMMAR
%token          T_TOKEN
%token          T_MAP
%token          T_INCLUDE

%union {
  struct map     *map;
  struct filter  *filter;
  struct grammar  *grammar;
  struct abbrev  *abbrev;
  struct rule    *rule;
  struct literal *literal;
  struct match   *match;
  struct action  *action;
  struct entry   *entry;
  enum quant     quant;
  char           *string;
  int            intval;
}

%type <abbrev>  tokens token
%type <rule>    rules rule
%type <literal> literal
%type <string>  token_opts grammar_ref
%type <match>   match match_seq match_prim
%type <quant>   match_quant
%type <action>  actions action
%type <entry>   entry_prim value path
%type <map>     map
%type <grammar> grammar
%type <filter>  filters

%{
/* Lexer */
extern int spec_lex (YYSTYPE * yylval_param,YYLTYPE * yylloc_param ,yyscan_t yyscanner);
int spec_init_lexer(const char *name, yyscan_t *scanner);
int spec_get_lineno (yyscan_t yyscanner );
const char *spec_get_extra (yyscan_t yyscanner );
char *spec_get_text (yyscan_t yyscanner );

void spec_error(YYLTYPE *locp, struct grammar **grammar, struct map **map,
                yyscan_t scanner, const char *s);
%}

%%

start: grammar
       { (*grammars) = $1; }
     | map
       { (*maps) = $1; }
     | start grammar
       { list_append(*grammars, $2); }
     | start map
       { list_append(*maps, $2); }

map: T_MAP '{' grammar_ref filters '}'
{ $$ = make_map(spec_get_extra(scanner), $3, $4, @1.first_line); }

grammar_ref: T_GRAMMAR T_NAME
             { $$ = $2; }

filters: T_INCLUDE T_QUOTED
         { $$ = make_filter($2, @1.first_line); }
       | filters T_INCLUDE T_QUOTED
         { $$=$1; list_append($1, make_filter($3, @1.first_line)); }

/*
 * Grammars from here on out
 */
grammar: T_GRAMMAR T_NAME '{' tokens rules '}'
         { 
           $$ = make_grammar(spec_get_extra(scanner), $2, $4, $5, 
                             @1.first_line); 
         }

tokens: /* empty */
        { $$ = NULL; } 
      | tokens token
        { list_append($1, $2); $$ = $1; }

token: T_TOKEN T_NAME literal token_opts
        { $$ = make_abbrev($2, $3, $4, @1.first_line); }

token_opts: /* empty */
            { $$ = NULL; }
          | '=' T_QUOTED
            { $$ = $2; }

rules: /* empty */
       { $$ = NULL; }
     | rules rule
       { list_append($1, $2); $$ = $1; }

rule: T_NAME_COLON match
      { $$ = make_rule($1, $2, NULL, @1.first_line); }
    | T_NAME_COLON match '{' actions '}'
      { $$ = make_rule($1, $2, $4, @1.first_line); }

/* Matches describe the structure of the file */
match: match_seq
       { $$  = $1;  }
     | match '|' match_seq
       { $$ = make_match_list($1, $3, ALTERNATIVE, @1.first_line); }

match_seq: match_prim
           { $$ = $1; }
         | match_seq match_prim
           { $$ = make_match_list($1, $2, SEQUENCE, @1.first_line); }

match_prim:  literal
            { $$ = make_literal_match($1, @1.first_line); }
          | T_NAME match_quant
            { $$ = make_name_match($1, $2, @1.first_line); }
          | T_ANY
            { $$ = make_any_match($1, @1.first_line); }
          | T_FIELD
            { $$ = make_field_match($1, @1.first_line); }
          | '(' match ')' match_quant
            { 
              $$ = $2; 
              $$->gid = 1;   /* $2 was enclosed in parens, count as group */
              $$->quant = $4; 
              $$->epsilon = ($$->quant == Q_MAYBE || $$->quant == Q_STAR);
            }


match_quant: /* empty */
             { $$ = Q_ONCE; }
           | '*'
             { $$ = Q_STAR; }
           | '+'
             { $$ = Q_PLUS; }
           | '?'
             { $$ = Q_MAYBE; }

literal: T_QUOTED
         { $$ = make_literal($1, QUOTED, @1.first_line); }
       | T_REGEX
         { $$ = make_literal($1, REGEX, @1.first_line); }

/* Actions describe the transformation from matches into the tree */
actions: action
       { $$ = $1; }
     | actions action
       { $$ = $1; list_append($1, $2); }

action: '@' T_NUMBER '{' path value '}'
        { $$ = make_action(A_GROUP, $2, $4, $5, @1.first_line); }
      | '@' T_FIELD  '{' path value '}'
        { $$ = make_action(A_FIELD, $2, $4, $5, @1.first_line); }

path:  entry_prim
       { $$ = $1; }
    |  entry_prim path
       { $$ = $1; list_append($1, $2); }

value: /* empty */
       { $$ = NULL; }
     | '=' entry_prim
       { $$ = $2; }

entry_prim: T_GLOBAL
            { $$ = make_entry(E_GLOBAL, @1.first_line); $$->text = $1; }
          | T_QUOTED
            { $$ = make_entry(E_CONST, @1.first_line); $$->text = $1; }
          | T_FIELD
            { $$ = make_entry(E_FIELD, @1.first_line); $$->field = $1; }

%%

int spec_parse_file(const char *name, 
                    struct grammar **grammars,
                    struct map **maps) {
  yyscan_t      scanner;
  int r;

  *grammars = NULL;
  *maps = NULL;

  if (spec_init_lexer(name, &scanner) == -1) {
    spec_error(NULL, grammars, maps, NULL, "Could not open input");
    return -1;
  }

  yydebug = getenv("YYDEBUG") != NULL;
  r = spec_parse(grammars, maps, scanner);
  if (r == 1) {
    spec_error(NULL, grammars, maps, NULL, "Parsing failed - syntax error");
    goto error;
  } else if (r == 2) {
    spec_error(NULL, grammars, maps, NULL, "Ran out of memory");
    goto error;
  }
  return 0;

 error:
  // free grammars and maps
  return -1;
}

// FIXME: Nothing here checks for alloc errors.
struct abbrev *make_abbrev(const char *name, struct literal *literal,
                           const char *deflt, int lineno) {
  struct abbrev *result;

  CALLOC(result, 1);
  result->lineno = lineno;
  result->name = name;
  result->literal = literal;
  if (deflt != NULL) {
    if (literal->type == QUOTED) {
      grammar_error(NULL, lineno, "Ignoring default for quoted %s", name);
    } else {
      literal->text = deflt;
    }
  }
  return result;
}

struct rule *make_rule(const char *name, struct match *matches, 
                       struct action *actions, int lineno) {
  struct rule *result;

  CALLOC(result, 1);
  result->lineno = lineno;
  result->name = name;
  result->matches = matches;
  result->actions = actions;
  list_for_each(a, actions) {
    a->rule = result;
  }
  return result;
}

struct match *make_match_list(struct match *head, struct match *tail, 
                              enum match_type type, int lineno) {
  struct match *result;

  if (head->type != type) {
    result = make_match(type, lineno);
    result->matches = head;
  } else {
    result = head;
  }
  list_append(result->matches, tail);
  return result;
}

struct match *make_literal_match(struct literal *literal, int lineno) {
  struct match *result;

  if (literal == NULL)
    internal_error(NULL, lineno, "NULL literal\n");

  result = make_match(LITERAL, lineno);
  result->literal = literal;
  return result;
}

struct match *make_name_match(const char *name, enum quant quant, int lineno) {
  struct match *result;

  if (name == NULL)
    internal_error(NULL, lineno, "NULL name\n");

  result = make_match(NAME, lineno);
  result->name = name;
  result->quant = quant;
  result->epsilon = (quant == Q_MAYBE || quant == Q_STAR);
  return result;
}

struct match *make_match(enum match_type type, int lineno) {
  struct match *result;

  CALLOC(result, 1);
  result->type = type;
  result->lineno = lineno;
  return result;
}

struct match *make_any_match(int epsilon, int lineno) {
  struct match *result;

  result = make_match(ANY, lineno);
  result->literal = make_literal(NULL, REGEX, lineno);
  result->epsilon = epsilon;
  return result;
}

struct match *make_field_match(int field, int lineno) {
  struct match *result;

  result = make_match(FIELD, lineno);
  result->field = field;
  return result;
}

static struct action *make_action(enum action_scope scope, int id,
                                  struct entry *path, 
                                  struct entry *value, int lineno) {
  struct action *result;

  CALLOC(result, 1);
  result->lineno = lineno;
  result->scope = scope;
  result->id = id;
  
  result->path = path;
  if (path != NULL)
    path->action = result;
  
  result->value = value;
  if (value != NULL)
    value->action = result;

  return result;
}

static struct entry *make_entry(enum entry_type type, int lineno) {
  struct entry *result;

  CALLOC(result, 1);
  result->lineno = lineno;
  result->type = type;
  return result;
}

static struct map *make_map(const char *filename,
                            const char *grammar_name, struct filter *filters,
                            int lineno) {
  struct map *result;

  CALLOC(result, 1);
  result->filename = strdup(filename);
  result->lineno = lineno;
  result->grammar_name = grammar_name;
  result->filters = filters;
  return result;
}

static struct filter *make_filter(const char *glob, int lineno) {
  struct filter *result;
  
  CALLOC(result, 1);
  result->lineno = lineno;
  result->glob = glob;
  return result;
}

static struct grammar *make_grammar(const char *filename,
                                    const char *name, struct abbrev *abbrevs,
                                    struct rule *rules, int lineno) {
  struct grammar *result;

  CALLOC(result, 1);
  result->filename = strdup(filename);
  result->lineno = lineno;
  result->name = name;
  result->abbrevs = abbrevs;
  result->rules = rules;
  return result;
}

void spec_error(YYLTYPE *locp, 
                struct grammar **grammar, 
                struct map     **map,
                yyscan_t scanner,
                const char *s) {
  if (scanner != NULL) {
    int line;
    line = spec_get_lineno(scanner);
    grammar_error(spec_get_extra(scanner), line, "%s reading %s", s,
                  spec_get_text(scanner));
  } else {
    int line = 0;
    const char *filename = "unknown";
    if (*grammar != NULL) {
      line = (*grammar)->lineno;
      filename = (*grammar)->filename;
    } else if (*map != NULL) {
      line = (*map)->lineno;
      filename = (*map)->filename;
    }
    if (locp != NULL)
      line = locp->first_line;
    grammar_error(filename, line, "error: %s\n", s);
  }
}