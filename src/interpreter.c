#include "../include/interpreter.h"
#include "../include/parser.h"

// --- Environment ---

lenv *lenv_new(void) {
  lenv *e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv *e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    val_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

val *lenv_get(lenv *e, val *k) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      return val_copy(e->vals[i]);
    }
  }
  return val_err("Unbound Symbol!");
}

void lenv_put(lenv *e, val *k, val *v) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      val_del(e->vals[i]);
      e->vals[i] = val_copy(v);
      return;
    }
  }

  e->count++;
  e->vals = realloc(e->vals, sizeof(val *) * e->count);
  e->syms = realloc(e->syms, sizeof(char *) * e->count);

  e->vals[e->count - 1] = val_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
}

// --- Builtins Replaced by Direct Eval Logic ---

long val_eval_int(lenv *e, val *v) {
  val *r = val_eval(e, v);
  if (r->type != VAL_INT) {
    printf("Runtime Error: Expected integer, got %d\n", r->type);
    val_del(r);
    return 0; // Error handling usually returns val* but for simple extraction
              // we return 0
  }
  long n = r->num;
  val_del(r);
  return n;
}

val *val_eval(lenv *e, val *v) {
  int line = v->line;
  val *result = val_eval_inner(e, v);
  if (result->type == VAL_ERR && result->line <= 0 && line > 0) {
    result->line = line;
  }
  return result;
}

val *val_eval_inner(lenv *e, val *v) {
  // printf("Eval Type: %d ", v->type); if(v->type==VAL_SYM) printf("Sym: %s",
  // v->sym); printf("\n");
  if (v->type == VAL_SYM) {
    val *x = lenv_get(e, v);
    val_del(v);
    return x;
  }
  if (v->type == VAL_BREAK || v->type == VAL_CONTINUE) {
    return v;
  }

  if (v->type == VAL_SEXPR) {
    if (v->count == 0)
      return v;

    // Check for specific forms based on first symbol
    val *f = v->cell[0];
    if (f->type == VAL_SYM) {
      char *op = f->sym;

      // Unary NOT
      if (strcmp(op, "!") == 0) {
        val *f = val_pop(v, 0);
        if (v->count != 1) {
          val_del(v);
          val_del(f);
          return val_err("! expects 1 argument");
        }
        val *arg = val_eval(e, val_copy(v->cell[0]));
        long truthy = 0;
        if (arg->type == VAL_INT && arg->num != 0)
          truthy = 1;
        else if (arg->type == VAL_FLOAT && arg->dec != 0.0)
          truthy = 1;
        val_del(arg);
        val_del(v);
        val_del(f);
        return val_num(!truthy);
      }

      // Logical AND/OR (Short-circuiting)
      if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        val *f = val_pop(v, 0);
        if (v->count != 2) {
          val_del(v);
          val_del(f);
          return val_err("Logical op expects 2 arguments");
        }

        val *lhs = val_eval(e, val_copy(v->cell[0]));
        long ltruth = 0;
        if (lhs->type == VAL_INT && lhs->num != 0)
          ltruth = 1;
        else if (lhs->type == VAL_FLOAT && lhs->dec != 0.0)
          ltruth = 1;
        val_del(lhs);

        if (strcmp(op, "&&") == 0) {
          if (!ltruth) {
            val_del(v);
            val_del(f);
            return val_num(0);
          }      // Short circuit
        } else { // ||
          if (ltruth) {
            val_del(v);
            val_del(f);
            return val_num(1);
          } // Short circuit
        }

        val *rhs = val_eval(e, val_copy(v->cell[1]));
        long rtruth = 0;
        if (rhs->type == VAL_INT && rhs->num != 0)
          rtruth = 1;
        else if (rhs->type == VAL_FLOAT && rhs->dec != 0.0)
          rtruth = 1;
        val_del(rhs);

        val_del(v);
        val_del(f);
        return val_num(rtruth);
      }

      // Arithmetic / Comparison
      if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
          strcmp(op, "*") == 0 || strcmp(op, "/") == 0 ||
          strcmp(op, "%") == 0 || strcmp(op, ">") == 0 ||
          strcmp(op, "<") == 0 || strcmp(op, "==") == 0 ||
          strcmp(op, "!=") == 0 || strcmp(op, ">=") == 0 ||
          strcmp(op, "<=") == 0) {

        val *f = val_pop(v, 0); // Pop operator to keep it alive

        if (v->count != 2) {
          val_del(v);
          val_del(f);
          return val_err("Operator expects 2 arguments");
        }

        val *arg1 = v->cell[0];
        val *arg2 = v->cell[1];

        // If either is float, result is float
        // Evaluate args
        val *val1 = val_eval(e, val_copy(arg1));
        val *val2 = val_eval(e, val_copy(arg2));

        val *result = NULL;
        char *s = f->sym;

        // String concatenation
        if (strcmp(s, "+") == 0 &&
            (val1->type == VAL_STR || val2->type == VAL_STR)) {
          char *s1 = (val1->type == VAL_STR) ? val1->str : NULL;
          char *s2 = (val2->type == VAL_STR) ? val2->str : NULL;

          char buf1[128], buf2[128];
          if (!s1) {
            if (val1->type == VAL_INT)
              sprintf(buf1, "%li", val1->num);
            else if (val1->type == VAL_FLOAT)
              sprintf(buf1, "%g", val1->dec);
            else
              sprintf(buf1, "?");
            s1 = buf1;
          }
          if (!s2) {
            if (val2->type == VAL_INT)
              sprintf(buf2, "%li", val2->num);
            else if (val2->type == VAL_FLOAT)
              sprintf(buf2, "%g", val2->dec);
            else
              sprintf(buf2, "?");
            s2 = buf2;
          }

          char *combined = malloc(strlen(s1) + strlen(s2) + 1);
          strcpy(combined, s1);
          strcat(combined, s2);
          result = val_str(combined);
          free(combined);

          val_del(val1);
          val_del(val2);
          val_del(v);
          val_del(f);
          return result;
        }

        // Numeric arithmetic
        if (val1->type == VAL_STR || val2->type == VAL_STR) {
          val_del(val1);
          val_del(val2);
          val_del(v);
          val_del(f);
          return val_err("Invalid types for operator");
        }

        int is_float = (val1->type == VAL_FLOAT || val2->type == VAL_FLOAT);
        double x = (val1->type == VAL_FLOAT) ? val1->dec : (double)val1->num;
        double y = (val2->type == VAL_FLOAT) ? val2->dec : (double)val2->num;

        val_del(val1);
        val_del(val2);
        val_del(v);

        // val* result; // Already declared above
        // char* s = f->sym; // Already declared above

        if (is_float) {
          // Float arithmetic
          if (strcmp(s, "+") == 0)
            result = val_float(x + y);
          else if (strcmp(s, "-") == 0)
            result = val_float(x - y);
          else if (strcmp(s, "*") == 0)
            result = val_float(x * y);
          else if (strcmp(s, "/") == 0) {
            if (y == 0.0)
              result = val_err("Div zero");
            else
              result = val_float(x / y);
          } else if (strcmp(s, ">") == 0)
            result = val_num(x > y);
          else if (strcmp(s, "<") == 0)
            result = val_num(x < y);
          else if (strcmp(s, "==") == 0)
            result = val_num(x == y);
          else if (strcmp(s, "!=") == 0)
            result = val_num(x != y);
          else if (strcmp(s, ">=") == 0)
            result = val_num(x >= y);
          else if (strcmp(s, "<=") == 0)
            result = val_num(x <= y);
          else
            result = val_err("Bad float op");
        } else {
          // Integer arithmetic (preserves existing behavior)
          long ix = (long)x;
          long iy = (long)y;

          if (strcmp(s, "+") == 0)
            result = val_num(ix + iy);
          else if (strcmp(s, "-") == 0)
            result = val_num(ix - iy);
          else if (strcmp(s, "*") == 0)
            result = val_num(ix * iy);
          else if (strcmp(s, "/") == 0) {
            if (iy == 0)
              result = val_err("Div zero");
            else
              result = val_num(ix / iy);
          } else if (strcmp(s, "%") == 0) {
            if (iy == 0)
              result = val_err("Div zero");
            else
              result = val_num(ix % iy);
          } else if (strcmp(s, ">") == 0)
            result = val_num(ix > iy);
          else if (strcmp(s, "<") == 0)
            result = val_num(ix < iy);
          else if (strcmp(s, "==") == 0)
            result = val_num(ix == iy);
          else if (strcmp(s, "!=") == 0)
            result = val_num(ix != iy);
          else if (strcmp(s, ">=") == 0)
            result = val_num(ix >= iy);
          else if (strcmp(s, "<=") == 0)
            result = val_num(ix <= iy);
          else
            result = val_err("Bad int op");
        }

        val_del(f);
        return result;
      }

      // Call: (call fname args)
      if (strcmp(op, "call") == 0) {
        val *fname = v->cell[1];
        val *args = v->cell[2];

        // Builtin Print/Println
        if (strcmp(fname->sym, "print") == 0 ||
            strcmp(fname->sym, "println") == 0) {
          for (int i = 0; i < args->count; i++) {
            val *v_val = val_eval(e, val_copy(args->cell[i]));
            if (v_val->type == VAL_STR) {
              printf("%s", v_val->str);
            } else {
              val_print(v_val);
            }
            val_del(v_val);
            if (i != args->count - 1)
              putchar(' ');
          }
          if (strcmp(fname->sym, "println") == 0)
            putchar('\n');
          val_del(v);
          return val_num(0);
        }

        // Builtin Type Conversions
        if (strcmp(fname->sym, "int") == 0) {
          if (args->count != 1) {
            val_del(v);
            return val_err("int() takes 1 argument");
          }
          val *v_val = val_eval(e, val_copy(args->cell[0]));
          val *res;
          if (v_val->type == VAL_INT)
            res = val_copy(v_val);
          else if (v_val->type == VAL_FLOAT)
            res = val_num((long)v_val->dec);
          else if (v_val->type == VAL_STR) {
            if (strlen(v_val->str) == 1 && !isdigit(v_val->str[0])) {
              res = val_num((long)v_val->str[0]);
            } else {
              res = val_num(strtol(v_val->str, NULL, 10));
            }
          } else
            res = val_err("Cannot convert to int");
          val_del(v_val);
          val_del(v);
          return res;
        }

        if (strcmp(fname->sym, "float") == 0) {
          if (args->count != 1) {
            val_del(v);
            return val_err("float() takes 1 argument");
          }
          val *v_val = val_eval(e, val_copy(args->cell[0]));
          val *res;
          if (v_val->type == VAL_FLOAT)
            res = val_copy(v_val);
          else if (v_val->type == VAL_INT)
            res = val_float((double)v_val->num);
          else if (v_val->type == VAL_STR)
            res = val_float(strtod(v_val->str, NULL));
          else
            res = val_err("Cannot convert to float");
          val_del(v_val);
          val_del(v);
          return res;
        }

        if (strcmp(fname->sym, "string") == 0) {
          if (args->count != 1) {
            val_del(v);
            return val_err("string() takes 1 argument");
          }
          val *v_val = val_eval(e, val_copy(args->cell[0]));
          char buf[128];
          if (v_val->type == VAL_INT)
            sprintf(buf, "%li", v_val->num);
          else if (v_val->type == VAL_FLOAT)
            sprintf(buf, "%g", v_val->dec);
          else if (v_val->type == VAL_STR) {
            val_del(v);
            return v_val;
          } else {
            val_del(v_val);
            val_del(v);
            return val_err("Cannot convert to string");
          }
          val *res = val_str(buf);
          val_del(v_val);
          val_del(v);
          return res;
        }

        if (strcmp(fname->sym, "char") == 0) {
          if (args->count != 1) {
            val_del(v);
            return val_err("char() takes 1 argument");
          }
          val *v_val = val_eval(e, val_copy(args->cell[0]));
          if (v_val->type != VAL_INT) {
            val_del(v_val);
            val_del(v);
            return val_err("char() takes an int ASCII value");
          }
          char buf[2] = {(char)v_val->num, '\0'};
          val *res = val_str(buf);
          val_del(v_val);
          val_del(v);
          return res;
        }

        if (strcmp(fname->sym, "input") == 0) {
          if (args->count > 1) {
            val_del(v);
            return val_err("input() takes 0 or 1 argument");
          }
          if (args->count == 1) {
            val *prompt = val_eval(e, val_copy(args->cell[0]));
            if (prompt->type == VAL_STR)
              printf("%s", prompt->str);
            else
              val_print(prompt);
            val_del(prompt);
          }

          char buf[2048];
          if (!fgets(buf, 2048, stdin)) {
            val_del(v);
            return val_str("");
          }
          buf[strcspn(buf, "\n")] = '\0';
          val *res = val_str(buf);
          val_del(v);
          return res;
        }

        if (strcmp(fname->sym, "len") == 0) {
          if (args->count != 1) {
            val_del(v);
            return val_err("len() takes 1 argument");
          }
          val *v_val = val_eval(e, val_copy(args->cell[0]));
          if (v_val->type != VAL_STR) {
            val_del(v_val);
            val_del(v);
            return val_err("len() expects a string");
          }
          val *res = val_num((long)strlen(v_val->str));
          val_del(v_val);
          val_del(v);
          return res;
        }

        val_del(v);
        return val_err("Function not defined");
      }

      // Internal for_body: (for_body body step)
      if (strcmp(op, "for_body") == 0) {
        val *body = v->cell[1];
        val *step = v->cell[2];
        val *res = val_eval(e, val_copy(body));
        if (res->type == VAL_BREAK) {
          val_del(res);
          val_del(v);
          return val_break(); // Exit while loop
        }
        // Even if continue, we run the step
        val_del(res);
        val *sres = val_eval(e, val_copy(step));
        val_del(sres);
        val_del(v);
        return val_num(0);
      }

      // Block: (block st1 st2 ...)
      if (strcmp(op, "block") == 0) {
        val *res = val_num(0);
        for (int i = 1; i < v->count; i++) {
          val_del(res);
          res = val_eval(e, val_copy(v->cell[i]));
          if (res->type == VAL_BREAK || res->type == VAL_CONTINUE ||
              res->type == VAL_ERR) {
            val_del(v);
            return res;
          }
        }
        val_del(v);
        return res;
      }

      // Declaration: (decl name val)
      if (strcmp(op, "decl") == 0) {
        val *name = v->cell[1];
        val *v_val = val_eval(e, val_copy(v->cell[2]));
        lenv_put(e, name, v_val);
        val_del(v);
        return v_val;
      }

      // Assignment: (assign name val)
      if (strcmp(op, "assign") == 0) {
        val *name = v->cell[1];
        val *v_val = val_eval(e, val_copy(v->cell[2]));
        lenv_put(e, name, v_val);
        val_del(v);
        return v_val;
      }

      // While: (while cond body)
      if (strcmp(op, "while") == 0) {
        val *res = val_num(0);
        while (1) {
          val *cond = val_eval(e, val_copy(v->cell[1]));
          int c = (cond->type == VAL_INT && cond->num != 0) ||
                  (cond->type == VAL_FLOAT && cond->dec != 0.0);
          val_del(cond);
          if (!c)
            break;

          val_del(res);
          res = val_eval(e, val_copy(v->cell[2]));
          if (res->type == VAL_BREAK) {
            val_del(res);
            res = val_num(0);
            break;
          }
          if (res->type == VAL_ERR)
            break;
          // continue just skips to next iteration of while
        }
        val_del(v);
        return res;
      }

      // If: (if cond then [else])
      if (strcmp(op, "if") == 0) {
        val *cond = val_eval(e, val_copy(v->cell[1]));
        int c = (cond->type == VAL_INT && cond->num != 0) ||
                (cond->type == VAL_FLOAT && cond->dec != 0.0);
        val_del(cond);
        val *res;
        if (c) {
          res = val_eval(e, val_copy(v->cell[2]));
        } else if (v->count > 3) {
          res = val_eval(e, val_copy(v->cell[3]));
        } else {
          res = val_num(0);
        }
        val_del(v);
        return res;
      }

      // Program: (program stmts...)
      if (strcmp(op, "program") == 0) {
        val *res = val_num(0);
        for (int i = 1; i < v->count; i++) {
          val_del(res);
          res = val_eval(e, val_copy(v->cell[i]));
          if (res->type == VAL_ERR)
            break;
        }
        val_del(v);
        return res;
      }
    }
  }
  return v;
}

val *builtin_load(lenv *e, val *a) {
  val *filename = a->cell[0];
  FILE *f = fopen(filename->str, "rb");
  if (!f) {
    val_del(a);
    return val_err("Could not open file");
  }
  fseek(f, 0, SEEK_END);
  long length = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *input = malloc(length + 1);
  fread(input, 1, length, f);
  input[length] = '\0';
  fclose(f);

  val *expr = val_read(input);
  free(input);
  val *x = val_eval(e, expr);
  val_del(a);
  return x;
}
