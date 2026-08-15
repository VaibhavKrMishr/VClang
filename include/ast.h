#ifndef VCLANG_AST_H
#define VCLANG_AST_H

#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    int line; // Track source code line number

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

// Constructors
val* val_num(long x);
val* val_float(double x);
val* val_str(char* s);
val* val_break(void);
val* val_continue(void);
val* val_err(char* m);
val* val_sym(char* s);
val* val_sexpr(void);
val* val_fun(lbuiltin func);
val* val_qexpr(void);

// Destructor
void val_del(val* v);

// AST manipulation
val* val_add(val* v, val* x);
val* val_pop(val* v, int i);
val* val_take(val* v, int i);
val* val_copy(val* v);
val* val_node(char *op, val *x, val *y);

// Printing
void val_expr_print(val *v, char open, char close);
void val_print(val* v);
void val_println(val* v);

#endif // VCLANG_AST_H
