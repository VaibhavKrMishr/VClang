#ifndef VCLANG_PARSER_H
#define VCLANG_PARSER_H

#include "ast.h"
#include <string.h>

char *peek(val *tokens, int i);
int expect(val *tokens, int *i, char *sym);

val *val_parse_factor(val *tokens, int *i);
val *val_parse_term(val *tokens, int *i);
val *val_parse_add(val *tokens, int *i);
val *val_parse_cmp(val *tokens, int *i);
val *val_parse_logical(val *tokens, int *i);
val *val_parse_expr(val *tokens, int *i);
val *val_parse_block(val *tokens, int *i);
val *val_parse_statement(val *tokens, int *i);
val *val_parse(val *tokens);
val *val_read(char *s);

#endif // VCLANG_PARSER_H
