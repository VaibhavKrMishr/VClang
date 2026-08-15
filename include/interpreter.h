#ifndef VCLANG_INTERPRETER_H
#define VCLANG_INTERPRETER_H

#include "ast.h"
#include <ctype.h>

// Environment
struct lenv {
  int count;
  char **syms;
  val **vals;
};

lenv* lenv_new(void);
void lenv_del(lenv* e);
val* lenv_get(lenv* e, val* k);
void lenv_put(lenv* e, val* k, val* v);

long val_eval_int(lenv* e, val* v);
val* val_eval(lenv* e, val* v);
val* val_eval_inner(lenv* e, val* v);
val* builtin_load(lenv* e, val* a);

#endif // VCLANG_INTERPRETER_H
