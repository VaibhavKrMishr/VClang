#include "../include/lexer.h"
#include <stdlib.h>

// --- Tokenizer ---

// Reads a number (int or float)
val *val_read_num(char *s, int *i) {
  char *end;
  long x = strtol(s + *i, &end, 10);
  // Check for decimal point
  if (*end == '.') {
    double d = strtod(s + *i, &end);
    *i = end - s;
    return val_float(d);
  }
  *i = end - s;
  return val_num(x);
}

// Reads a symbol or keyword or identifier
val *val_read_sym(char *s, int *i) {
  char *part = calloc(1, 1);
  // Allow alphanumeric and underscore for identifiers
  while ((isalnum(s[*i]) || s[*i] == '_') && s[*i] != '\0') {
    part = realloc(part, strlen(part) + 2);
    part[strlen(part) + 1] = '\0';
    part[strlen(part)] = s[*i];
    (*i)++;
  }
  val *v = val_sym(part);
  free(part);
  return v;
}

// Reads a string enclosed in delim
val *val_read_str(char *s, int *i, char delim) {
  char *part = calloc(1, 1);
  (*i)++; // Skip opening quote
  while (s[*i] != delim && s[*i] != '\0') {
    char c = s[*i];
    if (c == '\\') {
      (*i)++;
      if (s[*i] == 'n')
        c = '\n';
      else if (s[*i] == 't')
        c = '\t';
      else if (s[*i] == 'r')
        c = '\r';
      else if (s[*i] == '\\')
        c = '\\';
      else if (s[*i] == '"')
        c = '"';
      else if (s[*i] == '\'')
        c = '\'';
      else
        c = s[*i]; // Literal escape
    }

    part = realloc(part, strlen(part) + 2);
    part[strlen(part) + 1] = '\0';
    part[strlen(part)] = c;
    (*i)++;
  }
  if (s[*i] == delim)
    (*i)++; // Skip closing quote
  val *v = val_str(part);
  free(part);
  return v;
}

// Reads an operator or punctuation
val *val_read_op(char *s, int *i) {
  char *part = calloc(1, 1);
  char c = s[*i];

  // Single char punctuation
  if (strchr("(){};,", c)) {
    part = realloc(part, 2);
    part[0] = c;
    part[1] = '\0';
    (*i)++;
    val *v = val_sym(part);
    free(part);
    return v;
  }

  // Multi-char operators like ==, !=, >=, <=
  while (strchr("+-*/%=<>!&|", s[*i]) && s[*i] != '\0') {
    part = realloc(part, strlen(part) + 2);
    part[strlen(part) + 1] = '\0';
    part[strlen(part)] = s[*i];
    (*i)++;
  }

  val *v = val_sym(part);
  free(part);
  return v;
}

val *val_tokenize(char *s) {
  val *tokens = val_sexpr(); // Use sexpr container for list of tokens
  int i = 0;
  int line_num = 1;
  while (s[i] != '\0') {
    if (s[i] == '\n')
      line_num++;

    if (isspace(s[i])) {
      i++;
      continue;
    }

    if (s[i] == '/' && s[i + 1] == '/') {
      i += 2;
      while (s[i] != '\0') {
        if (s[i] == '\n') {
          line_num++;
          i++;
          break; // End of single-line comment
        }
        i++;
      }
      continue;
    }

    if (s[i] == '"' && s[i + 1] == '"' && s[i + 2] == '"') {
      i += 3;
      while (s[i] != '\0') {
        if (s[i] == '\n')
          line_num++;
        if (s[i] == '"' && s[i + 1] == '"' && s[i + 2] == '"') {
          i += 3;
          break;
        }
        i++;
      }
      continue;
    }

    val *t = NULL;
    if (isdigit(s[i])) {
      t = val_read_num(s, &i);
    } else if (isalpha(s[i]) || s[i] == '_') {
      t = val_read_sym(s, &i);
    } else if (strchr("(){};,+-*/%=<>!&|", s[i])) {
      t = val_read_op(s, &i);
    } else if (s[i] == '"' || s[i] == '\'') {
      t = val_read_str(s, &i, s[i]);
    } else {
      i++; // Unknown?
    }

    if (t != NULL) {
      t->line = line_num;
      val_add(tokens, t);
    }
  }
  return tokens;
}
