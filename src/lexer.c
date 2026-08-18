#include "../include/lexer.h"
#include <ctype.h>

// ============================================================
// Token List helpers
// ============================================================

static void tl_init(TokenList *tl) {
  tl->cap    = 64;
  tl->count  = 0;
  tl->tokens = (Token *)malloc(sizeof(Token) * tl->cap);
}

static void tl_push(TokenList *tl, Token t) {
  if (tl->count >= tl->cap) {
    tl->cap *= 2;
    tl->tokens = (Token *)realloc(tl->tokens, sizeof(Token) * tl->cap);
  }
  tl->tokens[tl->count++] = t;
}

void tokenlist_free(TokenList *tl) {
  for (int i = 0; i < tl->count; i++) {
    Token *t = &tl->tokens[i];
    if (t->type == TOK_STRING || t->type == TOK_IDENT || t->type == TOK_OP) {
      free(t->sval);
    }
  }
  free(tl->tokens);
  tl->tokens = NULL;
  tl->count  = 0;
  tl->cap    = 0;
}

// ============================================================
// Individual token readers
// ============================================================

static void read_num(const char *s, int *i, TokenList *tl, int line) {
  char *end;
  long x = strtol(s + *i, &end, 10);
  if (*end == '.') {
    double d = strtod(s + *i, &end);
    *i = (int)(end - s);
    Token t = { .type = TOK_FLOAT, .line = line, .fval = d };
    tl_push(tl, t);
  } else {
    *i = (int)(end - s);
    Token t = { .type = TOK_INT, .line = line, .ival = x };
    tl_push(tl, t);
  }
}

static void read_ident(const char *s, int *i, TokenList *tl, int line) {
  int start = *i;
  while ((isalnum(s[*i]) || s[*i] == '_') && s[*i] != '\0') {
    (*i)++;
  }
  int len = *i - start;
  char *lex = (char *)malloc(len + 1);
  memcpy(lex, s + start, len);
  lex[len] = '\0';
  Token t = { .type = TOK_IDENT, .line = line, .sval = lex };
  tl_push(tl, t);
}

static void read_string(const char *s, int *i, TokenList *tl, int line,
                         char delim) {
  (*i)++; // Skip opening quote
  // Use a small growable buffer
  int   cap  = 32;
  int   len  = 0;
  char *buf  = (char *)malloc(cap);

  while (s[*i] != delim && s[*i] != '\0') {
    char c = s[*i];
    if (c == '\\') {
      (*i)++;
      switch (s[*i]) {
      case 'n':  c = '\n'; break;
      case 't':  c = '\t'; break;
      case 'r':  c = '\r'; break;
      case '\\': c = '\\'; break;
      case '"':  c = '"';  break;
      case '\'': c = '\''; break;
      default:   c = s[*i]; break;
      }
    }
    if (len + 1 >= cap) {
      cap *= 2;
      buf = (char *)realloc(buf, cap);
    }
    buf[len++] = c;
    (*i)++;
  }
  if (s[*i] == delim) {
    (*i)++; // Skip closing quote
  } else {
    // L-3 fix: report unterminated string instead of silently truncating
    fprintf(stderr, "VClang: warning: unterminated string literal at line %d\n", line);
  }
  buf[len] = '\0';

  Token t = { .type = TOK_STRING, .line = line, .sval = buf };
  tl_push(tl, t);
}

static void read_op(const char *s, int *i, TokenList *tl, int line) {
  char c = s[*i];

  // Single-char punctuation (never part of multi-char ops)
  if (strchr("(){};,", c)) {
    char *lex = (char *)malloc(2);
    lex[0] = c;
    lex[1] = '\0';
    (*i)++;
    Token t = { .type = TOK_OP, .line = line, .sval = lex };
    tl_push(tl, t);
    return;
  }

  // Check for known two-char operators
  char next = s[*i + 1];
  if (next != '\0') {
    int is_two = 0;
    if ((c == '=' && next == '=') || (c == '!' && next == '=') ||
        (c == '<' && next == '=') || (c == '>' && next == '=') ||
        (c == '&' && next == '&') || (c == '|' && next == '|')) {
      is_two = 1;
    }
    if (is_two) {
      char *lex = (char *)malloc(3);
      lex[0] = c;
      lex[1] = next;
      lex[2] = '\0';
      *i += 2;
      Token t = { .type = TOK_OP, .line = line, .sval = lex };
      tl_push(tl, t);
      return;
    }
  }

  // Single-char operator
  char *lex = (char *)malloc(2);
  lex[0] = c;
  lex[1] = '\0';
  (*i)++;
  Token t = { .type = TOK_OP, .line = line, .sval = lex };
  tl_push(tl, t);
}

// ============================================================
// Main tokenizer
// ============================================================

TokenList tokenize(const char *s) {
  TokenList tl;
  tl_init(&tl);

  int i = 0;
  int line_num = 1;

  while (s[i] != '\0') {
    if (s[i] == '\n') line_num++;

    // Whitespace
    if (isspace(s[i])) { i++; continue; }

    // Single-line comment //
    if (s[i] == '/' && s[i + 1] == '/') {
      i += 2;
      while (s[i] != '\0') {
        if (s[i] == '\n') { line_num++; i++; break; }
        i++;
      }
      continue;
    }

    // Triple-quote block comment """..."""
    if (s[i] == '"' && s[i + 1] == '"' && s[i + 2] == '"') {
      i += 3;
      while (s[i] != '\0') {
        if (s[i] == '\n') line_num++;
        if (s[i] == '"' && s[i + 1] == '"' && s[i + 2] == '"') {
          i += 3;
          break;
        }
        i++;
      }
      continue;
    }

    // Numbers
    if (isdigit(s[i])) {
      read_num(s, &i, &tl, line_num);
      continue;
    }

    // Identifiers / keywords
    if (isalpha(s[i]) || s[i] == '_') {
      read_ident(s, &i, &tl, line_num);
      continue;
    }

    // Operators / punctuation
    if (strchr("(){};,+-*/%=<>!&|", s[i])) {
      read_op(s, &i, &tl, line_num);
      continue;
    }

    // String literals
    if (s[i] == '"' || s[i] == '\'') {
      read_string(s, &i, &tl, line_num, s[i]);
      continue;
    }

    // L-4 fix: warn about unknown characters instead of silently skipping
    fprintf(stderr, "VClang: warning: unknown character '%c' (0x%02x) at line %d\n",
            s[i], (unsigned char)s[i], line_num);
    i++;
  }

  // Append EOF sentinel
  Token eof = { .type = TOK_EOF, .line = line_num };
  tl_push(&tl, eof);

  return tl;
}
