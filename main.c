#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LEX_USE_XMACRO
#define LEX_TYPE_NAME_OFFSET 4
#define LEX_IMPLEMENTATION
#include "lex.h"

#define IDENTATION 2

typedef struct {
  char *items;
  size_t count, capacity;
} StringBuffer;

#define SB_DEFAULT_CAP 8

#define sb_ensure_cap(sb, needed) \
  do { \
    size_t target_capacity = (sb)->count + (needed) + 1; \
    if (target_capacity > (sb)->capacity) { \
      while ((sb)->capacity < target_capacity) \
        (sb)->capacity += ((sb)->capacity)? ((sb)->capacity >> 2) : SB_DEFAULT_CAP; \
      (sb)->items = realloc((sb)->items, (sb)->capacity); \
      (sb)->items[target_capacity] = '\0'; \
    } \
  } while(0);

#define sb_append(sb, cstr) \
  do { \
    size_t cstr_len = strlen(cstr); \
    sb_ensure_cap(sb, cstr_len); \
    strncpy(&(sb)->items[(sb)->count], (cstr), cstr_len); \
    (sb)->count += cstr_len; \
  } while(0);

#define sb_append_fmt(sb, fmt, ...) \
  do { \
    size_t len = snprintf(NULL, 0, fmt, __VA_ARGS__); \
    sb_ensure_cap(sb, len); \
    sprintf(&(sb)->items[(sb)->count], fmt, __VA_ARGS__); \
    (sb)->count += len; \
  } while(0);

size_t hts_rule_term(LexCursor cursor) {
  return lex_match_chars(cursor, "{};=#.;");
}

size_t hts_rule_bloq(LexCursor cursor) {
  if (lex_curch(cursor) != '[')
    return LEX_NO_MATCH;


  size_t length = 0;
  int count = 0;
  do {
    if (lex_curch(cursor) == '\0') {
      return LEX_NO_MATCH;
    }
    
    if (lex_curch(cursor) == '[') count++;
    if (lex_curch(cursor) == ']') count--;

    length++;
    lex_curmove(&cursor, 1);
  } while(count > 0);

  return length;
}

#define HTS(X) \
  X(HTS_WS,      lex_builtin_rule_ws,              .skip = true) \
  X(HTS_COMMENT, lex_builtin_rule_clike_mlcomment, .skip=true) \
  X(HTS_ID,      lex_builtin_rule_id_kebab) \
  X(HTS_TERM,    hts_rule_term) \
  X(HTS_STRING,  lex_builtin_rule_string) \
  X(HTS_BLOQ,    hts_rule_bloq)

typedef LEX_ENUMX(HTS) HTSTokens;

LexType HTS_TYPES[HTS_COUNT] = LEX_TYPEX(HTS);

[[noreturn]]
void report_error(Lex l, const char* msg) {
  size_t column = lex_curcol(l.cursor);
  size_t line_start = lex_curline_start(l.cursor);
  size_t line_end = lex_curline_end(l.cursor);
  
  fprintf(stderr, "%.*s\n", (int)(line_end - line_start), lex_view(l, line_start));
  fprintf(stderr, "%*s Error: %s\n", (int)column, "^", msg);
  exit(1);
}

// #id.class1.class2.class3
void parse_selector(Lex *lex, StringBuffer *sb) {
  LexToken tk, id;

  if (lex_skip(lex, HTS_TERM, "#")) {
    if (lex_consume(lex, &id, HTS_ID)) {
      sb_append_fmt(sb, " id=\"%s\"", lex_tkstr_tmp(id));
    } else {
      report_error(*lex, "Element id should be a valid identifier");
    }
  }

  bool open = false;
  while (lex_skip(lex, HTS_TERM, ".")) {
    if (!open) {
      sb_append(sb, " class=\"");
    }

    if (lex_consume(lex, &id, HTS_ID))
      sb_append_fmt(sb, "%s%s", open? " ": "", lex_tkstr_tmp(id));
    
    open = true;
  }

  if (open) sb_append(sb, "\"");
}

void parse_attribs(Lex *lex, StringBuffer *sb) {
  Lex b = LEX_BRANCH(lex);

  for (;;) {
    LexToken id, value;
    if (lex_consume(&b, &id, HTS_ID) && lex_skip(&b, HTS_TERM, "=")) {
      sb_append_fmt(sb, " %s", lex_tkstr_tmp(id));

      if (!lex_consume(&b, &value, HTS_STRING))
        report_error(*lex, "Expecting attribute value as string");

      sb_append_fmt(sb, "=\"%.*s\"", (int)lex_tklen(value) - 2, lex_tkstr(value) + 1);

      lex_skip(&b, HTS_TERM, ";"); // optional
      
      LEX_MERGE_BRANCH(lex, b);
      continue;
    }

    return;
  }
}

bool try_parse_tag(Lex *lex, StringBuffer *sb, int level) {
  LexToken tag;
  if (lex_consume(lex, &tag, HTS_ID)) {
    sb_append_fmt(sb, "%s%*s<%s", level > 0? "\n" : "", level * IDENTATION, "", lex_tkstr_tmp(tag));
    parse_selector(lex, sb);

    if (lex_skip(lex, HTS_TERM, "{")) { // Open tag body
      // Parse Attribs
      parse_attribs(lex, sb);
      sb_append(sb, ">");
     
      // Parse children tags
      int children_count = 0;
      while (try_parse_tag(lex, sb, level + 1)) 
        children_count++;

      // Close tag
      if (!lex_skip(lex, HTS_TERM, "}"))
        report_error(*lex, "Expected }");
      
      if (children_count > 0) {
        sb_append_fmt(sb, "\n%*s</%s>", level * IDENTATION, "", lex_tkstr_tmp(tag));
      } else {
        sb_append_fmt(sb, "</%s>", lex_tkstr_tmp(tag));
      }
    } else if (lex_consume(lex, NULL, HTS_STRING) || lex_consume(lex, NULL, HTS_BLOQ)) { // Open text only body
      sb_append_fmt(sb, ">%.*s</%s>", (int)lex_tklen(lex->tk) - 2, lex_tkstr(lex->tk) + 1, lex_tkstr_tmp(tag));
    } else {
      report_error(*lex, "Invalid tag body, expecting '{', '[' or string");
      exit(1);
    }

    return true;
  }

  if (lex_consume(lex, NULL, HTS_STRING) || lex_consume(lex, NULL, HTS_BLOQ)) { // Text child
    sb_append_fmt(sb, "%.*s", (int)lex_tklen(lex->tk) - 2, lex_tkstr(lex->tk) + 1);
  }

  return false;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage %s <file.hts>\n  Prints transpiled 'hts' code to the stdout as raw html.\n", argv[0]);
    exit(1);
  }

  const char *input_file = argv[1];
  const char *source = lex_read_file(input_file, NULL);
  if (!source) {
    fprintf(stderr, "Invalid input file '%s'\n", input_file);
    exit(1);
  }

  StringBuffer sb = {0};
  sb_append(&sb, "<!DOCTYPE html>\n");

  // Parse document
  Lex lex = lex_init(LEX_TYPEARRAY(HTS_TYPES), source);
  while (try_parse_tag(&lex, &sb, 0));

  fprintf(stdout, "%s", sb.items);
  fflush(stdout);
}
