#ifndef VCLANG_H
#define VCLANG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- Data Types ---

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STR,
    VAL_SYM,
    VAL_SEXPR,
    VAL_QEXPR,
    VAL_FUN,
    VAL_ERR,
    VAL_BREAK,
    VAL_CONTINUE
} val_type;

struct val;
struct lenv;
typedef struct lenv lenv;
typedef struct val* (*lbuiltin)(struct lenv*, struct val*);

typedef struct val {
    val_type type;

    // Basic data
    long num;
    double dec;
    char* str;
    char* err;
    char* sym;
    lbuiltin fun;

    // List of sub-expressions
    int count;
    struct val** cell;
} val;

// --- Function Prototypes ---

// Constructors
val* val_num(long x);
val* val_float(double x);
val* val_str(char* s);
val* val_err(char* m);
val* val_sym(char* s);
val* val_fun(lbuiltin func);
val* val_sexpr(void);
val* val_qexpr(void);
val* val_break(void);
val* val_continue(void);

// Destructor
void val_del(val* v);

// AST manipulation
val* val_add(val* v, val* x);
val* val_pop(val* v, int i);
val* val_take(val* v, int i);
val* val_copy(val* v);

// Printing
void val_print(val* v);
void val_println(val* v);

// Evaluation
val* val_eval(lenv* e, val* v);

// Environment
struct lenv;
lenv* lenv_new(void);
void lenv_del(lenv* e);
val* lenv_get(lenv* e, val* k);
void lenv_put(lenv* e, val* k, val* v);
void lenv_add_builtin(lenv* e, char* name, lbuiltin func);

#endif
