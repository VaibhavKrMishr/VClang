#include "../include/interpreter.h"
#include "../include/parser.h"

// ============================================================
// Environment — Freelist pool to avoid malloc/free per loop iteration
// ============================================================

// Freelist of recycled Env structs (I-3 fix)
static Env *env_freelist = NULL;

Env *env_new(Env *parent) {
  Env *e;
  if (env_freelist) {
    e = env_freelist;
    env_freelist = e->parent;  // parent used as freelist next pointer
  } else {
    e = (Env *)malloc(sizeof(Env));
  }
  e->parent = parent;
  e->count  = 0;
  e->cap    = 0;
  e->syms   = NULL;
  e->vals   = NULL;
  return e;
}

// Clear contents but keep the struct for reuse
void env_reset(Env *e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    val_clear(&e->vals[i]);
  }
  e->count = 0;
  // Keep syms/vals arrays allocated (cap stays) for reuse
}

void env_del(Env *e) {
  env_reset(e);
  free(e->syms);
  free(e->vals);
  // Push onto freelist instead of freeing the struct (I-3 fix)
  e->syms   = NULL;
  e->vals   = NULL;
  e->cap    = 0;
  e->parent = env_freelist;
  env_freelist = e;
}

// Read-only peek — returns pointer into env storage, no copy (I-4 fix)
const Value *env_peek(Env *e, const char *name) {
  char first = name[0];
  while (e) {
    for (int i = 0; i < e->count; i++) {
      if (e->syms[i][0] == first && strcmp(e->syms[i], name) == 0) {
        return &e->vals[i];
      }
    }
    e = e->parent;
  }
  return NULL;
}

Value env_get(Env *e, const char *name) {
  const Value *v = env_peek(e, name);
  if (v) return val_copy(*v);
  return val_new_err("Unbound Symbol!", -1);
}

void env_put(Env *e, const char *name, Value v) {
  char first = name[0];
  Env *curr = e;
  while (curr) {
    for (int i = 0; i < curr->count; i++) {
      if (curr->syms[i][0] == first && strcmp(curr->syms[i], name) == 0) {
        val_clear(&curr->vals[i]);
        curr->vals[i] = val_copy(v);
        return;
      }
    }
    curr = curr->parent;
  }
  env_def(e, name, v);
}

void env_def(Env *e, const char *name, Value v) {
  char first = name[0];
  for (int i = 0; i < e->count; i++) {
    if (e->syms[i][0] == first && strcmp(e->syms[i], name) == 0) {
      val_clear(&e->vals[i]);
      e->vals[i] = val_copy(v);
      return;
    }
  }
  // Geometric growth (I-10 fix) — double capacity instead of realloc per var
  if (e->count >= e->cap) {
    e->cap = (e->cap == 0) ? 4 : e->cap * 2;
    e->vals = (Value *)realloc(e->vals, sizeof(Value) * e->cap);
    e->syms = (char **)realloc(e->syms, sizeof(char *) * e->cap);
  }
  e->vals[e->count] = val_copy(v);
  e->syms[e->count] = strdup(name);
  e->count++;
}

// ============================================================
// Truthiness helper
// ============================================================

static int is_truthy(Value v) {
  if (v.type == VAL_INT   && v.num != 0)   return 1;
  if (v.type == VAL_FLOAT && v.dec != 0.0)  return 1;
  return 0;
}

// ============================================================
// Built-in function dispatch
// ============================================================

static Value eval_builtin_call(Env *e, const char *fname,
                                const ASTNode **args, int argc, int line) {
  // print / println
  if (strcmp(fname, "print") == 0 || strcmp(fname, "println") == 0) {
    for (int i = 0; i < argc; i++) {
      Value v = eval(e, args[i]);
      if (v.type == VAL_STR)
        printf("%s", v.str);
      else
        val_print(v);
      val_clear(&v);
      if (i != argc - 1) putchar(' ');
    }
    if (strcmp(fname, "println") == 0) putchar('\n');
    return val_new_int(0);
  }

  // int()
  if (strcmp(fname, "int") == 0) {
    if (argc != 1) return val_new_err("int() takes 1 argument", line);
    Value v = eval(e, args[0]);
    Value res;
    if (v.type == VAL_INT)
      res = v;
    else if (v.type == VAL_FLOAT)
      res = val_new_int((long)v.dec);
    else if (v.type == VAL_STR) {
      if (strlen(v.str) == 1 && !isdigit(v.str[0]))
        res = val_new_int((long)v.str[0]);
      else
        res = val_new_int(strtol(v.str, NULL, 10));
      val_clear(&v);
    } else {
      val_clear(&v);
      res = val_new_err("Cannot convert to int", line);
    }
    return res;
  }

  // float()
  if (strcmp(fname, "float") == 0) {
    if (argc != 1) return val_new_err("float() takes 1 argument", line);
    Value v = eval(e, args[0]);
    Value res;
    if (v.type == VAL_FLOAT)
      res = v;
    else if (v.type == VAL_INT)
      res = val_new_float((double)v.num);
    else if (v.type == VAL_STR) {
      res = val_new_float(strtod(v.str, NULL));
      val_clear(&v);
    } else {
      val_clear(&v);
      res = val_new_err("Cannot convert to float", line);
    }
    return res;
  }

  // string()
  if (strcmp(fname, "string") == 0) {
    if (argc != 1) return val_new_err("string() takes 1 argument", line);
    Value v = eval(e, args[0]);
    char buf[128];
    if (v.type == VAL_INT)
      sprintf(buf, "%li", v.num);
    else if (v.type == VAL_FLOAT)
      sprintf(buf, "%g", v.dec);
    else if (v.type == VAL_STR) {
      // Already a string — return as-is
      return v;
    } else {
      val_clear(&v);
      return val_new_err("Cannot convert to string", line);
    }
    val_clear(&v);
    return val_new_str(buf);
  }

  // char()
  if (strcmp(fname, "char") == 0) {
    if (argc != 1) return val_new_err("char() takes 1 argument", line);
    Value v = eval(e, args[0]);
    if (v.type != VAL_INT) {
      val_clear(&v);
      return val_new_err("char() takes an int ASCII value", line);
    }
    char buf[2] = {(char)v.num, '\0'};
    return val_new_str(buf);
  }

  // input()
  if (strcmp(fname, "input") == 0) {
    if (argc > 1) return val_new_err("input() takes 0 or 1 argument", line);
    if (argc == 1) {
      Value prompt = eval(e, args[0]);
      if (prompt.type == VAL_STR)
        printf("%s", prompt.str);
      else
        val_print(prompt);
      val_clear(&prompt);
    }
    char buf[2048];
    if (!fgets(buf, 2048, stdin))
      return val_new_str("");
    buf[strcspn(buf, "\n")] = '\0';
    return val_new_str(buf);
  }

  // len()
  if (strcmp(fname, "len") == 0) {
    if (argc != 1) return val_new_err("len() takes 1 argument", line);
    Value v = eval(e, args[0]);
    if (v.type != VAL_STR) {
      val_clear(&v);
      return val_new_err("len() expects a string", line);
    }
    Value res = val_new_int((long)strlen(v.str));
    val_clear(&v);
    return res;
  }

  return val_new_err("Function not defined", line);
}

// ============================================================
// Arithmetic / comparison
// ============================================================

static Value eval_binary(Env *e, OpType op,
                          const ASTNode *lhs_node, const ASTNode *rhs_node,
                          int line) {
  // Short-circuit logical ops
  if (op == OP_AND) {
    Value lhs = eval(e, lhs_node);
    if (!is_truthy(lhs)) { val_clear(&lhs); return val_new_int(0); }
    val_clear(&lhs);
    Value rhs = eval(e, rhs_node);
    int r = is_truthy(rhs);
    val_clear(&rhs);
    return val_new_int(r);
  }
  if (op == OP_OR) {
    Value lhs = eval(e, lhs_node);
    if (is_truthy(lhs)) { val_clear(&lhs); return val_new_int(1); }
    val_clear(&lhs);
    Value rhs = eval(e, rhs_node);
    int r = is_truthy(rhs);
    val_clear(&rhs);
    return val_new_int(r);
  }

  // Evaluate both sides
  Value v1 = eval(e, lhs_node);
  Value v2 = eval(e, rhs_node);

  // String concatenation with +
  if (op == OP_ADD && (v1.type == VAL_STR || v2.type == VAL_STR)) {
    char buf1[128], buf2[128];
    const char *s1, *s2;
    if (v1.type == VAL_STR) s1 = v1.str;
    else {
      if (v1.type == VAL_INT) sprintf(buf1, "%li", v1.num);
      else if (v1.type == VAL_FLOAT) sprintf(buf1, "%g", v1.dec);
      else sprintf(buf1, "?");
      s1 = buf1;
    }
    if (v2.type == VAL_STR) s2 = v2.str;
    else {
      if (v2.type == VAL_INT) sprintf(buf2, "%li", v2.num);
      else if (v2.type == VAL_FLOAT) sprintf(buf2, "%g", v2.dec);
      else sprintf(buf2, "?");
      s2 = buf2;
    }
    char *combined = (char *)malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(combined, s1);
    strcat(combined, s2);
    // I-11 fix: build Value directly with owned string, avoiding
    // the double-alloc of malloc+strdup inside val_new_str.
    Value res;
    res.type = VAL_STR;
    res.line = -1;
    res.str  = combined;  // Transfer ownership — no free needed
    val_clear(&v1);
    val_clear(&v2);
    return res;
  }

  // Non-numeric operands after string concat check
  if (v1.type == VAL_STR || v2.type == VAL_STR) {
    val_clear(&v1);
    val_clear(&v2);
    return val_new_err("Invalid types for operator", line);
  }

  int is_float = (v1.type == VAL_FLOAT || v2.type == VAL_FLOAT);
  double x = (v1.type == VAL_FLOAT) ? v1.dec : (double)v1.num;
  double y = (v2.type == VAL_FLOAT) ? v2.dec : (double)v2.num;
  val_clear(&v1);
  val_clear(&v2);

  Value res;
  if (is_float) {
    switch (op) {
    case OP_ADD: res = val_new_float(x + y); break;
    case OP_SUB: res = val_new_float(x - y); break;
    case OP_MUL: res = val_new_float(x * y); break;
    case OP_DIV:
      if (y == 0.0) res = val_new_err("Div zero", line);
      else res = val_new_float(x / y);
      break;
    case OP_GT:  res = val_new_int(x > y);  break;
    case OP_LT:  res = val_new_int(x < y);  break;
    case OP_EQ:  res = val_new_int(x == y); break;
    case OP_NE:  res = val_new_int(x != y); break;
    case OP_GE:  res = val_new_int(x >= y); break;
    case OP_LE:  res = val_new_int(x <= y); break;
    default:     res = val_new_err("Bad float op", line); break;
    }
  } else {
    long ix = (long)x, iy = (long)y;
    switch (op) {
    case OP_ADD: res = val_new_int(ix + iy); break;
    case OP_SUB: res = val_new_int(ix - iy); break;
    case OP_MUL: res = val_new_int(ix * iy); break;
    case OP_DIV:
      if (iy == 0) res = val_new_err("Div zero", line);
      else res = val_new_int(ix / iy);
      break;
    case OP_MOD:
      if (iy == 0) res = val_new_err("Div zero", line);
      else res = val_new_int(ix % iy);
      break;
    case OP_GT:  res = val_new_int(ix > iy);  break;
    case OP_LT:  res = val_new_int(ix < iy);  break;
    case OP_EQ:  res = val_new_int(ix == iy); break;
    case OP_NE:  res = val_new_int(ix != iy); break;
    case OP_GE:  res = val_new_int(ix >= iy); break;
    case OP_LE:  res = val_new_int(ix <= iy); break;
    default:     res = val_new_err("Bad int op", line); break;
    }
  }
  return res;
}

// ============================================================
// Main eval — switch on ASTNodeType
// ============================================================

Value eval(Env *e, const ASTNode *node) {
  if (!node) return val_new_int(0);

  switch (node->type) {

  case AST_INT_LIT:
    return val_new_int(node->int_val);

  case AST_FLOAT_LIT:
    return val_new_float(node->float_val);

  case AST_STRING_LIT:
    return val_new_str(node->str_val);

  case AST_IDENT: {
    Value v = env_get(e, node->str_val);
    if (v.type == VAL_ERR && v.line <= 0)
      v.line = node->line;
    return v;
  }

  case AST_BINARY_OP:
    return eval_binary(e, node->binary.op,
                       node->binary.lhs, node->binary.rhs, node->line);

  case AST_UNARY_OP: {
    // Currently only !
    Value arg = eval(e, node->unary.operand);
    int truthy = is_truthy(arg);
    val_clear(&arg);
    return val_new_int(!truthy);
  }

  case AST_CALL:
    return eval_builtin_call(e, node->call.func_name,
                             node->call.args, node->call.arg_count,
                             node->line);

  case AST_DECL: {
    Value v = eval(e, node->decl.value);
    env_def(e, node->decl.name, v);
    return v;
  }

  case AST_ASSIGN: {
    Value v = eval(e, node->decl.value);
    env_put(e, node->decl.name, v);
    return v;
  }

  case AST_IF: {
    Value cond = eval(e, node->if_stmt.cond);
    int c = is_truthy(cond);
    val_clear(&cond);
    if (c)
      return eval(e, node->if_stmt.then_body);
    else if (node->if_stmt.else_body)
      return eval(e, node->if_stmt.else_body);
    else
      return val_new_int(0);
  }

  case AST_WHILE: {
    Value res = val_new_int(0);
    while (1) {
      Value cond = eval(e, node->while_stmt.cond);
      int c = is_truthy(cond);
      val_clear(&cond);
      if (!c) break;

      val_clear(&res);
      res = eval(e, node->while_stmt.body);
      if (res.type == VAL_BREAK) {
        val_clear(&res);
        res = val_new_int(0);
        break;
      }
      if (res.type == VAL_ERR) break;
      // VAL_CONTINUE: just loop again
    }
    return res;
  }

  case AST_FOR_BODY: {
    Value res = eval(e, node->for_body.body);
    if (res.type == VAL_BREAK) {
      // Propagate break up so the while loop sees it
      return res;
    }
    // Even if continue, we run the step
    val_clear(&res);
    Value sres = eval(e, node->for_body.step);
    val_clear(&sres);
    return val_new_int(0);
  }

  case AST_BLOCK: {
    Env *local = env_new(e);
    Value res = val_new_int(0);
    for (int i = 0; i < node->block.stmt_count; i++) {
      val_clear(&res);
      res = eval(local, node->block.stmts[i]);
      if (res.type == VAL_BREAK || res.type == VAL_CONTINUE ||
          res.type == VAL_ERR) {
        env_del(local);
        return res;
      }
    }
    env_del(local);
    return res;
  }

  case AST_PROGRAM: {
    Value res = val_new_int(0);
    for (int i = 0; i < node->block.stmt_count; i++) {
      val_clear(&res);
      res = eval(e, node->block.stmts[i]);
      if (res.type == VAL_BREAK || res.type == VAL_CONTINUE ||
          res.type == VAL_ERR) {
        return res;
      }
    }
    return res;
  }

  case AST_RETURN: {
    // For now, just evaluate and return
    return eval(e, node->ret.expr);
  }

  case AST_BREAK:
    return val_new_break();

  case AST_CONTINUE:
    return val_new_continue();
  }

  return val_new_int(0);
}

// ============================================================
// File loader
// ============================================================

Value builtin_load(Env *e, const char *filename) {
  FILE *f = fopen(filename, "rb");
  if (!f) return val_new_err("Could not open file", -1);

  fseek(f, 0, SEEK_END);
  long length = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *input = (char *)malloc(length + 1);
  fread(input, 1, length, f);
  input[length] = '\0';
  fclose(f);

  Arena *a = arena_new(4096);
  ASTNode *ast = parse_source(a, input);
  free(input);

  Value result = eval(e, ast);

  arena_free(a);
  return result;
}
