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
    if (v.type == VAL_STR) {
      Value res = val_new_int((long)strlen(v.str));
      val_clear(&v);
      return res;
    } else if (v.type == VAL_ARRAY) {
      Value res = val_new_int((long)(v.arr ? v.arr->count : 0));
      val_clear(&v);
      return res;
    } else {
      val_clear(&v);
      return val_new_err("len() expects a string or array", line);
    }
  }

  // push(arr, val)
  if (strcmp(fname, "push") == 0) {
    if (argc != 2) return val_new_err("push() takes 2 arguments (array, value)", line);
    if (args[0]->type != AST_IDENT) {
      return val_new_err("push() first argument must be an array variable name", line);
    }
    const Value *target = env_peek(e, args[0]->str_val);
    if (!target || target->type != VAL_ARRAY) {
      return val_new_err("push() target must be an array", line);
    }
    Value elem = eval(e, args[1]);
    val_array_push((Value *)target, elem);
    val_clear(&elem);
    return val_new_int(0);
  }

  // new_arena([cap]) / Arena([cap])
  if (strcmp(fname, "new_arena") == 0 || strcmp(fname, "Arena") == 0) {
    size_t cap = 4096;
    if (argc == 1) {
      Value v = eval(e, args[0]);
      if (v.type == VAL_INT && v.num > 0) cap = (size_t)v.num;
      val_clear(&v);
    }
    return val_new_arena(cap);
  }

  // reset_arena(arena) / arena_reset(arena)
  if (strcmp(fname, "reset_arena") == 0 || strcmp(fname, "arena_reset") == 0) {
    if (argc != 1) return val_new_err("reset_arena() takes 1 argument", line);
    Value v = eval(e, args[0]);
    if (v.type != VAL_ARENA || !v.arena) {
      val_clear(&v);
      return val_new_err("reset_arena() expects an Arena", line);
    }
    arena_reset(v.arena);
    val_clear(&v);
    return val_new_int(0);
  }

  // arena_used(arena)
  if (strcmp(fname, "arena_used") == 0) {
    if (argc != 1) return val_new_err("arena_used() takes 1 argument", line);
    Value v = eval(e, args[0]);
    if (v.type != VAL_ARENA || !v.arena) {
      val_clear(&v);
      return val_new_err("arena_used() expects an Arena", line);
    }
    size_t used = arena_used(v.arena);
    val_clear(&v);
    return val_new_int((long)used);
  }

  return val_new_err("Function not defined", line);
}

// User-defined and built-in function dispatch
static Value eval_call(Env *e, const char *fname,
                       const ASTNode **args, int argc, int line) {
  const Value *fn_val = env_peek(e, fname);
  if (fn_val && fn_val->type == VAL_FUNC) {
    if (argc != fn_val->func.param_count) {
      char err_msg[128];
      snprintf(err_msg, sizeof(err_msg),
               "Function '%s' expects %d arguments, got %d",
               fname, fn_val->func.param_count, argc);
      return val_new_err(err_msg, line);
    }

    // Evaluate arguments in caller's environment
    Value *evaled_args = (Value *)malloc(sizeof(Value) * (argc > 0 ? argc : 1));
    for (int i = 0; i < argc; i++) {
      evaled_args[i] = eval(e, args[i]);
      if (evaled_args[i].type == VAL_ERR) {
        Value err = val_copy(evaled_args[i]);
        for (int j = 0; j <= i; j++) val_clear(&evaled_args[j]);
        free(evaled_args);
        return err;
      }
    }

    // Create function environment linked to caller/global env
    Env *fn_env = env_new(e);
    for (int i = 0; i < argc; i++) {
      env_def(fn_env, fn_val->func.params[i], evaled_args[i]);
      val_clear(&evaled_args[i]);
    }
    free(evaled_args);

    const ASTNode *body = fn_val->func.body;
    Value res = eval(fn_env, body);
    env_del(fn_env);

    if (res.type == VAL_RETURN) {
      Value unwrapped = val_copy(*res.ret.val);
      val_clear(&res);
      return unwrapped;
    }
    if (res.type == VAL_BREAK || res.type == VAL_CONTINUE) {
      val_clear(&res);
      return val_new_int(0);
    }
    return res;
  }

  return eval_builtin_call(e, fname, args, argc, line);
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
    return eval_call(e, node->call.func_name,
                     node->call.args, node->call.arg_count,
                     node->line);

  case AST_FUNC_DEF: {
    Value f = val_new_func(node->func_def.name,
                           node->func_def.params,
                           node->func_def.param_count,
                           node->func_def.body);
    env_def(e, node->func_def.name, f);
    return f;
  }

  case AST_ARRAY_LIT: {
    Value arr = val_new_array(node->array_lit.count);
    for (int i = 0; i < node->array_lit.count; i++) {
      Value elem = eval(e, node->array_lit.elements[i]);
      val_array_push(&arr, elem);
      val_clear(&elem);
    }
    return arr;
  }

  case AST_INDEX: {
    Value target = eval(e, node->index_expr.target);
    Value index  = eval(e, node->index_expr.index);
    if (index.type != VAL_INT) {
      val_clear(&target);
      val_clear(&index);
      return val_new_err("Array index must be an integer", node->line);
    }
    long idx = index.num;
    val_clear(&index);

    if (target.type == VAL_ARRAY) {
      if (!target.arr || idx < 0 || idx >= target.arr->count) {
        val_clear(&target);
        return val_new_err("Array index out of bounds", node->line);
      }
      Value result = val_copy(target.arr->items[idx]);
      val_clear(&target);
      return result;
    } else if (target.type == VAL_STR) {
      long slen = (long)strlen(target.str);
      if (idx < 0 || idx >= slen) {
        val_clear(&target);
        return val_new_err("String index out of bounds", node->line);
      }
      char buf[2] = { target.str[idx], '\0' };
      val_clear(&target);
      return val_new_str(buf);
    } else {
      val_clear(&target);
      return val_new_err("Cannot index non-array/string value", node->line);
    }
  }

  case AST_INDEX_ASSIGN: {
    Value target;
    if (node->index_assign.target->type == AST_IDENT) {
      const char *name = node->index_assign.target->str_val;
      const Value *peeked = env_peek(e, name);
      if (!peeked || peeked->type != VAL_ARRAY || !peeked->arr) {
        return val_new_err("Target for index assignment must be an array variable", node->line);
      }
      target = val_copy(*peeked);
    } else {
      target = eval(e, node->index_assign.target);
      if (target.type != VAL_ARRAY || !target.arr) {
        val_clear(&target);
        return val_new_err("Target for index assignment must evaluate to an array", node->line);
      }
    }

    Value index = eval(e, node->index_assign.index);
    if (index.type != VAL_INT) {
      val_clear(&target);
      val_clear(&index);
      return val_new_err("Array index must be an integer", node->line);
    }
    long idx = index.num;
    val_clear(&index);

    if (idx < 0 || idx >= target.arr->count) {
      val_clear(&target);
      return val_new_err("Array index out of bounds", node->line);
    }
    Value new_val = eval(e, node->index_assign.value);
    val_clear(&target.arr->items[idx]);
    target.arr->items[idx] = val_copy(new_val);
    val_clear(&target);
    return new_val;
  }

  case AST_NEW: {
    // Optional allocator evaluation
    Value alloc_val = val_new_int(0);
    if (node->new_expr.allocator) {
      alloc_val = eval(e, node->new_expr.allocator);
    }

    // Instantiating Arena: new Arena() or new Arena(cap)
    if (node->new_expr.type_name &&
        (strcmp(node->new_expr.type_name, "Arena") == 0 || strcmp(node->new_expr.type_name, "arena") == 0)) {
      size_t cap = 4096;
      if (node->new_expr.init_expr) {
        Value iv = eval(e, node->new_expr.init_expr);
        if (iv.type == VAL_INT && iv.num > 0) cap = (size_t)iv.num;
        val_clear(&iv);
      }
      val_clear(&alloc_val);
      return val_new_arena(cap);
    }

    // Sized array allocation: new int[10] or new byte[1024]
    if (node->new_expr.size_expr) {
      Value sz = eval(e, node->new_expr.size_expr);
      if (sz.type != VAL_INT || sz.num < 0) {
        val_clear(&sz);
        val_clear(&alloc_val);
        return val_new_err("Array size must be a non-negative integer", node->line);
      }
      int count = (int)sz.num;
      val_clear(&sz);

      Value arr = val_new_array(count > 0 ? count : 4);
      if (node->new_expr.init_expr) {
        Value init = eval(e, node->new_expr.init_expr);
        if (init.type == VAL_ARRAY && init.arr) {
          for (int i = 0; i < count; i++) {
            if (i < init.arr->count) {
              val_array_push(&arr, init.arr->items[i]);
            } else {
              Value zero = val_new_int(0);
              val_array_push(&arr, zero);
              val_clear(&zero);
            }
          }
        }
        val_clear(&init);
      } else {
        for (int i = 0; i < count; i++) {
          Value zero = val_new_int(0);
          val_array_push(&arr, zero);
          val_clear(&zero);
        }
      }
      val_clear(&alloc_val);
      return arr;
    }

    // Immediate array initializer: new [1, 2, 3] or new int[]([1, 2, 3])
    if (node->new_expr.init_expr) {
      Value init = eval(e, node->new_expr.init_expr);
      val_clear(&alloc_val);
      return init;
    }

    // Default fallback
    val_clear(&alloc_val);
    return val_new_array(4);
  }

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
      if (res.type == VAL_RETURN || res.type == VAL_ERR) {
        return res;
      }
      // VAL_CONTINUE: just loop again
    }
    return res;
  }

  case AST_FOR_BODY: {
    Value res = eval(e, node->for_body.body);
    if (res.type == VAL_BREAK || res.type == VAL_RETURN) {
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
          res.type == VAL_RETURN || res.type == VAL_ERR) {
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
          res.type == VAL_RETURN || res.type == VAL_ERR) {
        if (res.type == VAL_RETURN) {
          Value unwrapped = val_copy(*res.ret.val);
          val_clear(&res);
          return unwrapped;
        }
        return res;
      }
    }
    return res;
  }

  case AST_RETURN: {
    Value ret_val = node->ret.expr ? eval(e, node->ret.expr) : val_new_int(0);
    Value res = val_new_return(ret_val);
    val_clear(&ret_val);
    return res;
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
