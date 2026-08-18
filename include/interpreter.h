#ifndef VCLANG_INTERPRETER_H
#define VCLANG_INTERPRETER_H

#include "ast.h"
#include <ctype.h>

// --- Runtime Value ---
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STR,
    VAL_ERR,
    VAL_BREAK,
    VAL_CONTINUE
} ValueType;

struct Value {
    ValueType type;
    int line;

    union {
        long   num;
        double dec;
        char  *str;
        char  *err;
    };
};

// --- Environment ---
typedef struct Env Env;

struct Env {
    Env     *parent;
    int      count;
    int      cap;
    char   **syms;
    Value   *vals;
};

Env   *env_new(Env *parent);
void   env_del(Env *e);
void   env_reset(Env *e);  // Clear contents but don't free struct (for pooling)
Value  env_get(Env *e, const char *name);
const Value *env_peek(Env *e, const char *name);  // Read-only, no copy
void   env_put(Env *e, const char *name, Value v);
void   env_def(Env *e, const char *name, Value v);

// --- Evaluation ---
Value  eval(Env *e, const ASTNode *node);
Value  builtin_load(Env *e, const char *filename);

#endif // VCLANG_INTERPRETER_H
