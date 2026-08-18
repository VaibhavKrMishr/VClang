#ifndef VCLANG_LEXER_H
#define VCLANG_LEXER_H

#include "memory.h"

typedef enum {
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
    TOK_IDENT,
    TOK_OP,
    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    int       line;
    union {
        long   ival;
        double fval;
        char  *sval;    // TOK_STRING, TOK_IDENT, TOK_OP (heap-allocated)
    };
} Token;

typedef struct {
    Token *tokens;
    int    count;
    int    cap;
} TokenList;

TokenList tokenize(const char *source);
void      tokenlist_free(TokenList *tl);

#endif // VCLANG_LEXER_H
