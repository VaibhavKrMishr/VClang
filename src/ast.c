#include "../include/ast.h"

// --- Constructors ---

val *val_num(long x) {
  val *v = malloc(sizeof(val));
  v->type = VAL_INT;
  v->line = -1;
  v->num = x;
  return v;
}

val *val_float(double x) {
  val *v = malloc(sizeof(val));
  v->type = VAL_FLOAT;
  v->line = -1;
  v->dec = x;
  return v;
}

val *val_str(char *s) {
  val *v = malloc(sizeof(val));
  v->type = VAL_STR;
  v->line = -1;
  v->str = malloc(strlen(s) + 1);
  strcpy(v->str, s);
  return v;
}

val *val_break(void) {
  val *v = malloc(sizeof(val));
  v->type = VAL_BREAK;
  v->line = -1;
  return v;
}

val *val_continue(void) {
  val *v = malloc(sizeof(val));
  v->type = VAL_CONTINUE;
  v->line = -1;
  return v;
}

val *val_err(char *m) {
  val *v = malloc(sizeof(val));
  v->type = VAL_ERR;
  v->line = -1;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

val *val_sym(char *s) {
  val *v = malloc(sizeof(val));
  v->type = VAL_SYM;
  v->line = -1;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

val *val_sexpr(void) {
  val *v = malloc(sizeof(val));
  v->type = VAL_SEXPR;
  v->line = -1;
  v->count = 0;
  v->cell = NULL;
  return v;
}

val *val_fun(lbuiltin func) {
  val *v = malloc(sizeof(val));
  v->type = VAL_FUN;
  v->line = -1;
  v->fun = func;
  return v;
}

val *val_qexpr(void) {
  val *v = malloc(sizeof(val));
  v->type = VAL_QEXPR;
  v->line = -1;
  v->count = 0;
  v->cell = NULL;
  return v;
}

// --- Destructor ---

void val_del(val *v) {
  switch (v->type) {
  case VAL_INT:
    break;
  case VAL_FLOAT:
    break;
  case VAL_BREAK:
    break;
  case VAL_CONTINUE:
    break;
  case VAL_FUN:
    break;
  case VAL_ERR:
    free(v->err);
    break;
  case VAL_SYM:
    free(v->sym);
    break;
  case VAL_STR:
    free(v->str);
    break;
  case VAL_QEXPR:
  case VAL_SEXPR:
    for (int i = 0; i < v->count; i++) {
      val_del(v->cell[i]);
    }
    free(v->cell);
    break;
  }
  free(v);
}

// --- AST Manipulation ---

val *val_add(val *v, val *x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(val *) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

val *val_pop(val *v, int i) {
  val *x = v->cell[i];
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(val *) * (v->count - i - 1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(val *) * v->count);
  return x;
}

val *val_take(val *v, int i) {
  val *x = val_pop(v, i);
  val_del(v);
  return x;
}

val *val_copy(val *v) {
  val *x = malloc(sizeof(val));
  x->type = v->type;
  x->line = v->line;

  switch (v->type) {
  case VAL_INT:
    x->num = v->num;
    break;
  case VAL_FLOAT:
    x->dec = v->dec;
    break;
  case VAL_BREAK:
    break;
  case VAL_CONTINUE:
    break;
  case VAL_FUN:
    x->fun = v->fun;
    break;
  case VAL_ERR:
    x->err = malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err);
    break;
  case VAL_STR:
    x->str = malloc(strlen(v->str) + 1);
    strcpy(x->str, v->str);
    break;
  case VAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym);
    break;
  case VAL_SEXPR:
  case VAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(val *) * x->count);
    for (int i = 0; i < x->count; i++) {
      x->cell[i] = val_copy(v->cell[i]);
    }
    break;
  }
  return x;
}

// --- Printing ---

void val_expr_print(val *v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    val_print(v->cell[i]);
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void val_print(val *v) {
  switch (v->type) {
  case VAL_INT:
    printf("%li", v->num);
    break;
  case VAL_FLOAT:
    printf("%lf", v->dec);
    break;
  case VAL_BREAK:
    printf("break");
    break;
  case VAL_CONTINUE:
    printf("continue");
    break;
  case VAL_STR:
    printf("%s", v->str);
    break;
  case VAL_ERR:
    if (v->line > 0)
      printf("Error (Line %d): %s", v->line, v->err);
    else
      printf("Error: %s", v->err);
    break;
  case VAL_SYM:
    printf("%s", v->sym);
    break;
  case VAL_FUN:
    printf("<function>");
    break;
  case VAL_SEXPR:
    val_expr_print(v, '(', ')');
    break;
  case VAL_QEXPR:
    val_expr_print(v, '{', '}');
    break;
  }
}

void val_println(val *v) {
  val_print(v);
  putchar('\n');
  fflush(stdout);
}

val *val_node(char *op, val *x, val *y) {
  val *v = val_sexpr();
  val_add(v, val_sym(op));
  val_add(v, x);
  val_add(v, y);
  return v;
}
