#ifndef VCLANG_INTERPRETER_H
#define VCLANG_INTERPRETER_H

#include "ast.h"
#include <ctype.h>

// --- Runtime Value ---
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STR,
    VAL_ARRAY,
    VAL_FUNC,
    VAL_RETURN,
    VAL_ERR,
    VAL_BREAK,
    VAL_CONTINUE,
    VAL_ARENA,
    VAL_STRUCT,
    VAL_STRUCT_DEF
} ValueType;

typedef struct ArrayBuffer {
    struct Value *items;
    int count;
    int cap;
    int refcount;
} ArrayBuffer;

struct Env;

typedef struct Function {
    char *name;
    char **params;
    char **param_types;
    int param_count;
    const ASTNode *body;
    struct Env *closure_env;
    int refcount;
} Function;

typedef struct StructFieldDef {
    char *type_name;
    char *name;
    int   offset;
} StructFieldDef;

typedef struct StructDef {
    char           *name;
    StructFieldDef *fields;
    int             field_count;
    Function      **methods;
    int             method_count;
} StructDef;

typedef struct StructInstance {
    const StructDef *def;
    struct Value    *fields;
    int              refcount;
    int              arena_owned;
} StructInstance;

struct Value {
    ValueType type;
    int line;

    union {
        long                    num;
        double                  dec;
        char                   *str;
        char                   *err;
        Arena                  *arena;
        ArrayBuffer            *arr;
        Function               *func;
        StructInstance         *sinst;
        const StructDef        *sdef;
        struct Value           *ret;
    };
};

// Global Struct Definition Registry
void             struct_def_register(StructDef *def);
const StructDef *struct_def_lookup(const char *name);
int              struct_def_get_field_index(const StructDef *def, const char *fname);
void             struct_def_cleanup_all(void);

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
void   env_retain(Env *e);
void   env_release(Env *e);
void   env_reset(Env *e);  // Clear contents but don't free struct (for pooling)
Value  env_get(Env *e, const char *name);
const Value *env_peek(Env *e, const char *name);  // Read-only, no copy
void   env_put(Env *e, const char *name, Value v);
void   env_def(Env *e, const char *name, Value v);

// --- Evaluation ---
Value  eval(Env *e, const ASTNode *node);
Value  builtin_load(Env *e, const char *filename);

#endif // VCLANG_INTERPRETER_H
