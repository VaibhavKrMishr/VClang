#ifndef VCLANG_LEXER_H
#define VCLANG_LEXER_H

#include "ast.h"
#include <ctype.h>
#include <string.h>

val* val_read_num(char *s, int *i);
val* val_read_sym(char *s, int *i);
val* val_read_str(char *s, int *i, char delim);
val* val_read_op(char *s, int *i);
val* val_tokenize(char *s);

#endif // VCLANG_LEXER_H
