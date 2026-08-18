#ifndef VCLANG_PARSER_H
#define VCLANG_PARSER_H

#include "ast.h"
#include "lexer.h"

ASTNode *parse(Arena *a, const TokenList *tl);
ASTNode *parse_source(Arena *a, const char *source);

#endif // VCLANG_PARSER_H
