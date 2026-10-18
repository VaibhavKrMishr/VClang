#include "../include/interpreter.h"
#include "../include/parser.h"
#include <sys/stat.h>

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

void env_freelist_cleanup(void) {
  while (env_freelist) {
    Env *next = env_freelist->parent;
    free(env_freelist);
    env_freelist = next;
  }
}

// Read-only peek — returns pointer into env storage, no copy (I-4 fix)
const Value *env_peek(Env *e, const char *name) {
  char first = name[0];
  while (e) {
    for (int i = 0; i < e->count; i++) {
      if (e->syms[i] == name || (e->syms[i][0] == first && strcmp(e->syms[i], name) == 0)) {
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
      if (curr->syms[i] == name || (curr->syms[i][0] == first && strcmp(curr->syms[i], name) == 0)) {
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
    if (e->syms[i] == name || (e->syms[i][0] == first && strcmp(e->syms[i], name) == 0)) {
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
    Value res = val_new_int(0);
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
    Value res = val_new_int(0);
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

  // typeof()
  if (strcmp(fname, "typeof") == 0) {
    if (argc != 1) return val_new_err("typeof() takes 1 argument", line);
    Value v = eval(e, args[0]);
    const char *tname = "unknown";
    if (v.type == VAL_INT) tname = "int";
    else if (v.type == VAL_FLOAT) tname = "float";
    else if (v.type == VAL_STR) tname = "string";
    else if (v.type == VAL_ARRAY) tname = "array";
    else if (v.type == VAL_FUNC) tname = "function";
    else if (v.type == VAL_ARENA) tname = "Arena";
    else if (v.type == VAL_STRUCT) {
      tname = (v.sinst && v.sinst->def && v.sinst->def->name) ? v.sinst->def->name : "struct";
    } else if (v.type == VAL_STRUCT_DEF) {
      tname = "type";
    }
    Value res = val_new_str(tname);
    val_clear(&v);
    return res;
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

  // ============================================================
  // File I/O Built-ins
  // ============================================================

  // file_exists(path)
  if (strcmp(fname, "file_exists") == 0) {
    if (argc != 1) return val_new_err("file_exists() takes 1 argument (path)", line);
    Value p = eval(e, args[0]);
    if (p.type == VAL_ERR) return p;
    if (p.type != VAL_STR) {
      val_clear(&p);
      return val_new_err("file_exists() expects a string path", line);
    }
    struct stat st;
    int exists = (stat(p.str, &st) == 0) ? 1 : 0;
    val_clear(&p);
    return val_new_int(exists);
  }

  // read_file(path)
  if (strcmp(fname, "read_file") == 0) {
    if (argc != 1) return val_new_err("read_file() takes 1 argument (path)", line);
    Value p = eval(e, args[0]);
    if (p.type == VAL_ERR) return p;
    if (p.type != VAL_STR) {
      val_clear(&p);
      return val_new_err("read_file() expects a string path", line);
    }
    FILE *f = fopen(p.str, "rb");
    if (!f) {
      char err_msg[256];
      snprintf(err_msg, sizeof(err_msg), "Could not open file '%s' for reading", p.str);
      val_clear(&p);
      return val_new_err(err_msg, line);
    }
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length < 0) {
      fclose(f);
      val_clear(&p);
      return val_new_err("Failed to determine file size", line);
    }
    if (length == 0) {
      fclose(f);
      val_clear(&p);
      return val_new_str("");
    }
    char *buf = (char *)malloc(length + 1);
    if (!buf) {
      fclose(f);
      val_clear(&p);
      return val_new_err("Out of memory reading file", line);
    }
    size_t bytes_read = fread(buf, 1, length, f);
    buf[bytes_read] = '\0';
    fclose(f);
    val_clear(&p);
    Value res = val_new_str(buf);
    free(buf);
    return res;
  }

  // write_file(path, content)
  if (strcmp(fname, "write_file") == 0) {
    if (argc != 2) return val_new_err("write_file() takes 2 arguments (path, content)", line);
    Value p = eval(e, args[0]);
    if (p.type == VAL_ERR) return p;
    if (p.type != VAL_STR) {
      val_clear(&p);
      return val_new_err("write_file() expects a string path", line);
    }
    Value c = eval(e, args[1]);
    if (c.type == VAL_ERR) {
      val_clear(&p);
      return c;
    }
    if (c.type != VAL_STR) {
      val_clear(&p);
      val_clear(&c);
      return val_new_err("write_file() expects string content", line);
    }
    FILE *f = fopen(p.str, "wb");
    if (!f) {
      char err_msg[256];
      snprintf(err_msg, sizeof(err_msg), "Could not open file '%s' for writing", p.str);
      val_clear(&p);
      val_clear(&c);
      return val_new_err(err_msg, line);
    }
    size_t len = strlen(c.str);
    if (len > 0) {
      size_t written = fwrite(c.str, 1, len, f);
      (void)written;
    }
    fclose(f);
    val_clear(&p);
    val_clear(&c);
    return val_new_int(1);
  }

  // append_file(path, content)
  if (strcmp(fname, "append_file") == 0) {
    if (argc != 2) return val_new_err("append_file() takes 2 arguments (path, content)", line);
    Value p = eval(e, args[0]);
    if (p.type == VAL_ERR) return p;
    if (p.type != VAL_STR) {
      val_clear(&p);
      return val_new_err("append_file() expects a string path", line);
    }
    Value c = eval(e, args[1]);
    if (c.type == VAL_ERR) {
      val_clear(&p);
      return c;
    }
    if (c.type != VAL_STR) {
      val_clear(&p);
      val_clear(&c);
      return val_new_err("append_file() expects string content", line);
    }
    FILE *f = fopen(p.str, "ab");
    if (!f) {
      char err_msg[256];
      snprintf(err_msg, sizeof(err_msg), "Could not open file '%s' for appending", p.str);
      val_clear(&p);
      val_clear(&c);
      return val_new_err(err_msg, line);
    }
    size_t len = strlen(c.str);
    if (len > 0) {
      size_t written = fwrite(c.str, 1, len, f);
      (void)written;
    }
    fclose(f);
    val_clear(&p);
    val_clear(&c);
    return val_new_int(1);
  }

  // remove_file(path)
  if (strcmp(fname, "remove_file") == 0) {
    if (argc != 1) return val_new_err("remove_file() takes 1 argument (path)", line);
    Value p = eval(e, args[0]);
    if (p.type == VAL_ERR) return p;
    if (p.type != VAL_STR) {
      val_clear(&p);
      return val_new_err("remove_file() expects a string path", line);
    }
    int res = (remove(p.str) == 0) ? 1 : 0;
    val_clear(&p);
    return val_new_int(res);
  }

  return val_new_err("Function not defined", line);
}

// User-defined and built-in function dispatch
static Value eval_call(Env *e, const char *fname,
                       const ASTNode **args, int argc, int line) {
  const Value *fn_val = env_peek(e, fname);
  if (fn_val && fn_val->type == VAL_FUNC) {
    if (argc != fn_val->func->param_count) {
      char err_msg[128];
      snprintf(err_msg, sizeof(err_msg),
               "Function '%s' expects %d arguments, got %d",
               fname, fn_val->func->param_count, argc);
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

      // Strict parameter type checking (Zero is-a substitutability)
      if (fn_val->func->param_types && fn_val->func->param_types[i]) {
        const char *expected_t = fn_val->func->param_types[i];
        int ok = 1;
        if (strcmp(expected_t, "int") == 0 || strcmp(expected_t, "byte") == 0 || strcmp(expected_t, "bool") == 0) {
          if (evaled_args[i].type != VAL_INT) ok = 0;
        } else if (strcmp(expected_t, "float") == 0) {
          if (evaled_args[i].type != VAL_FLOAT) ok = 0;
        } else if (strcmp(expected_t, "string") == 0) {
          if (evaled_args[i].type != VAL_STR) ok = 0;
        } else if (strcmp(expected_t, "array") == 0 || strstr(expected_t, "[]") != NULL) {
          if (evaled_args[i].type != VAL_ARRAY) ok = 0;
        } else if (strcmp(expected_t, "Arena") == 0 || strcmp(expected_t, "arena") == 0) {
          if (evaled_args[i].type != VAL_ARENA) ok = 0;
        } else if (strcmp(expected_t, "var") == 0 || strcmp(expected_t, "let") == 0) {
          ok = 1;
        } else {
          // Check struct type match
          const StructDef *exp_def = struct_def_lookup(expected_t);
          if (exp_def) {
            if (evaled_args[i].type != VAL_STRUCT || !evaled_args[i].sinst ||
                evaled_args[i].sinst->def != exp_def) {
              const char *got_t = "unknown";
              if (evaled_args[i].type == VAL_STRUCT && evaled_args[i].sinst && evaled_args[i].sinst->def) {
                got_t = evaled_args[i].sinst->def->name;
              } else if (evaled_args[i].type == VAL_INT) got_t = "int";
              else if (evaled_args[i].type == VAL_FLOAT) got_t = "float";
              else if (evaled_args[i].type == VAL_STR) got_t = "string";
              else if (evaled_args[i].type == VAL_ARRAY) got_t = "array";
              char err_msg[256];
              snprintf(err_msg, sizeof(err_msg),
                       "Type mismatch in function '%s': expected '%s' for parameter '%s', got '%s' (no is-a substitutability)",
                       fname, expected_t, fn_val->func->params[i], got_t);
              for (int j = 0; j <= i; j++) val_clear(&evaled_args[j]);
              free(evaled_args);
              return val_new_err(err_msg, line);
            }
          }
        }
        if (!ok) {
          char err_msg[256];
          snprintf(err_msg, sizeof(err_msg),
                   "Type mismatch in function '%s': expected '%s' for parameter '%s'",
                   fname, expected_t, fn_val->func->params[i]);
          for (int j = 0; j <= i; j++) val_clear(&evaled_args[j]);
          free(evaled_args);
          return val_new_err(err_msg, line);
        }
      }
    }

    // Create function environment linked to caller/global env
    Env *fn_env = env_new(e);
    for (int i = 0; i < argc; i++) {
      env_def(fn_env, fn_val->func->params[i], evaled_args[i]);
      val_clear(&evaled_args[i]);
    }
    free(evaled_args);

    const ASTNode *body = fn_val->func->body;
    Value res = eval(fn_env, body);
    env_del(fn_env);

    if (res.type == VAL_RETURN) {
      Value unwrapped = val_copy(*res.ret);
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

// In-place L-Value interior pointer resolver
static Value *eval_lvalue(Env *e, const ASTNode *node) {
  if (!node) return NULL;
  if (node->type == AST_IDENT) {
    char first = node->str_val[0];
    Env *curr = e;
    while (curr) {
      for (int i = 0; i < curr->count; i++) {
        if (curr->syms[i][0] == first && strcmp(curr->syms[i], node->str_val) == 0) {
          return &curr->vals[i];
        }
      }
      curr = curr->parent;
    }
    return NULL;
  }
  if (node->type == AST_INDEX) {
    Value *target = eval_lvalue(e, node->index_expr.target);
    if (!target || target->type != VAL_ARRAY || !target->arr) return NULL;
    Value idx_v = eval(e, node->index_expr.index);
    if (idx_v.type != VAL_INT) { val_clear(&idx_v); return NULL; }
    long idx = idx_v.num;
    val_clear(&idx_v);
    if (idx < 0) idx = target->arr->count + idx;
    if (idx < 0 || idx >= target->arr->count) return NULL;
    return &target->arr->items[idx];
  }
  if (node->type == AST_MEMBER_ACCESS) {
    Value *target = eval_lvalue(e, node->member_access.target);
    if (!target || target->type != VAL_STRUCT || !target->sinst || !target->sinst->def) return NULL;
    int fidx = struct_def_get_field_index(target->sinst->def, node->member_access.member);
    if (fidx < 0) return NULL;
    return &target->sinst->fields[fidx];
  }
  return NULL;
}

static Value eval_binary_values(OpType op, Value lhs, Value rhs, int line) {
  if (lhs.type == VAL_INT && rhs.type == VAL_INT) {
    switch (op) {
    case OP_ADD: case OP_ADD_ASSIGN: return val_new_int(lhs.num + rhs.num);
    case OP_SUB: case OP_SUB_ASSIGN: return val_new_int(lhs.num - rhs.num);
    case OP_MUL: return val_new_int(lhs.num * rhs.num);
    case OP_DIV:
      if (rhs.num == 0) return val_new_err("Division by zero", line);
      return val_new_int(lhs.num / rhs.num);
    case OP_MOD:
      if (rhs.num == 0) return val_new_err("Modulo by zero", line);
      return val_new_int(lhs.num % rhs.num);
    default: break;
    }
  }
  if ((lhs.type == VAL_FLOAT || lhs.type == VAL_INT) &&
      (rhs.type == VAL_FLOAT || rhs.type == VAL_INT)) {
    double l = (lhs.type == VAL_FLOAT) ? lhs.dec : (double)lhs.num;
    double r = (rhs.type == VAL_FLOAT) ? rhs.dec : (double)rhs.num;
    switch (op) {
    case OP_ADD: case OP_ADD_ASSIGN: return val_new_float(l + r);
    case OP_SUB: case OP_SUB_ASSIGN: return val_new_float(l - r);
    case OP_MUL: return val_new_float(l * r);
    case OP_DIV:
      if (r == 0.0) return val_new_err("Division by zero", line);
      return val_new_float(l / r);
    default: break;
    }
  }
  if (lhs.type == VAL_STR && rhs.type == VAL_STR && (op == OP_ADD || op == OP_ADD_ASSIGN)) {
    size_t len_a = strlen(lhs.str);
    size_t len_b = strlen(rhs.str);
    char *res = (char *)malloc(len_a + len_b + 1);
    strcpy(res, lhs.str);
    strcat(res, rhs.str);
    Value v;
    v.type = VAL_STR;
    v.line = -1;
    v.str = res;
    return v;
  }
  return val_new_err("Invalid types for binary operation", line);
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
    Value res = val_new_int(0);
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

  Value res = val_new_int(0);
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
                           node->func_def.param_types,
                           node->func_def.params,
                           node->func_def.param_count,
                           node->func_def.body,
                           NULL);
    env_def(e, node->func_def.name, f);
    return f;
  }

  case AST_STRUCT_DEF: {
    StructDef *sdef = (StructDef *)malloc(sizeof(StructDef));
    sdef->name = vclang_strdup(node->struct_def.name);
    sdef->field_count = node->struct_def.field_count;
    if (sdef->field_count > 0) {
      sdef->fields = (StructFieldDef *)malloc(sizeof(StructFieldDef) * sdef->field_count);
      for (int i = 0; i < sdef->field_count; i++) {
        sdef->fields[i].type_name = vclang_strdup(node->struct_def.field_types[i]);
        sdef->fields[i].name = vclang_strdup(node->struct_def.field_names[i]);
        sdef->fields[i].offset = i;
      }
    } else {
      sdef->fields = NULL;
    }

    sdef->method_count = node->struct_def.method_count;
    if (sdef->method_count > 0) {
      sdef->methods = (Function **)malloc(sizeof(Function *) * sdef->method_count);
      for (int i = 0; i < sdef->method_count; i++) {
        Value mval = eval(e, node->struct_def.methods[i]);
        if (mval.type == VAL_FUNC) {
          sdef->methods[i] = mval.func;
          mval.func->refcount++;
        } else {
          sdef->methods[i] = NULL;
        }
        val_clear(&mval);
      }
    } else {
      sdef->methods = NULL;
    }

    struct_def_register(sdef);
    Value type_val = val_new_struct_def(sdef);
    env_def(e, sdef->name, type_val);
    return type_val;
  }

  case AST_STRUCT_LIT: {
    const StructDef *def = struct_def_lookup(node->struct_lit.type_name);
    if (!def) {
      char err_msg[128];
      snprintf(err_msg, sizeof(err_msg), "Unknown struct type '%s'", node->struct_lit.type_name);
      return val_new_err(err_msg, node->line);
    }
    Value sval = val_new_struct(def, NULL);
    for (int i = 0; i < node->struct_lit.field_count; i++) {
      int fidx = struct_def_get_field_index(def, node->struct_lit.field_names[i]);
      if (fidx < 0) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Struct '%s' has no field '%s'", def->name, node->struct_lit.field_names[i]);
        val_clear(&sval);
        return val_new_err(err_msg, node->line);
      }
      Value fv = eval(e, node->struct_lit.field_values[i]);
      if (fv.type == VAL_ERR) {
        val_clear(&sval);
        return fv;
      }
      val_clear(&sval.sinst->fields[fidx]);
      sval.sinst->fields[fidx] = val_copy(fv);
      val_clear(&fv);
    }
    return sval;
  }

  case AST_MEMBER_ACCESS: {
    Value target = eval(e, node->member_access.target);
    if (target.type != VAL_STRUCT || !target.sinst || !target.sinst->def) {
      val_clear(&target);
      return val_new_err("Cannot access member on non-struct value", node->line);
    }
    const StructDef *def = target.sinst->def;
    int fidx = struct_def_get_field_index(def, node->member_access.member);
    if (fidx < 0) {
      char err_msg[128];
      snprintf(err_msg, sizeof(err_msg), "Struct '%s' has no field '%s'", def->name, node->member_access.member);
      val_clear(&target);
      return val_new_err(err_msg, node->line);
    }
    Value res = val_copy(target.sinst->fields[fidx]);
    val_clear(&target);
    return res;
  }

  case AST_MEMBER_ASSIGN: {
    Value *target_slot = eval_lvalue(e, node->member_assign.target);
    if (!target_slot || target_slot->type != VAL_STRUCT || !target_slot->sinst || !target_slot->sinst->def) {
      return val_new_err("Target for member assignment must evaluate to a struct", node->line);
    }
    const StructDef *def = target_slot->sinst->def;
    int fidx = struct_def_get_field_index(def, node->member_assign.member);
    if (fidx < 0) {
      char err_msg[128];
      snprintf(err_msg, sizeof(err_msg), "Struct '%s' has no field '%s'", def->name, node->member_assign.member);
      return val_new_err(err_msg, node->line);
    }

    Value rhs = eval(e, node->member_assign.value ? node->member_assign.value : NULL);
    if (node->member_assign.op == OP_ADD) { // Direct assignment '='
      val_clear(&target_slot->sinst->fields[fidx]);
      target_slot->sinst->fields[fidx] = val_copy(rhs);
      val_clear(&rhs);
      return val_copy(target_slot->sinst->fields[fidx]);
    } else if (node->member_assign.op == OP_INC) {
      Value one = val_new_int(1);
      Value new_val = eval_binary_values(OP_ADD, target_slot->sinst->fields[fidx], one, node->line);
      val_clear(&one);
      val_clear(&target_slot->sinst->fields[fidx]);
      target_slot->sinst->fields[fidx] = new_val;
      return val_copy(new_val);
    } else if (node->member_assign.op == OP_DEC) {
      Value one = val_new_int(1);
      Value new_val = eval_binary_values(OP_SUB, target_slot->sinst->fields[fidx], one, node->line);
      val_clear(&one);
      val_clear(&target_slot->sinst->fields[fidx]);
      target_slot->sinst->fields[fidx] = new_val;
      return val_copy(new_val);
    } else {
      Value new_val = eval_binary_values(node->member_assign.op, target_slot->sinst->fields[fidx], rhs, node->line);
      val_clear(&rhs);
      val_clear(&target_slot->sinst->fields[fidx]);
      target_slot->sinst->fields[fidx] = new_val;
      return val_copy(new_val);
    }
  }

  case AST_METHOD_CALL: {
    Value target = eval(e, node->method_call.target);
    if (target.type == VAL_STRUCT_DEF) {
      // Static / factory call: StructName.func(...)
      char desugared_name[256];
      snprintf(desugared_name, sizeof(desugared_name), "%s.%s", target.sdef->name, node->method_call.method_name);
      val_clear(&target);
      return eval_call(e, desugared_name, node->method_call.args, node->method_call.arg_count, node->line);
    }
    if (target.type == VAL_STRUCT) {
      // Instance method call: self is passed as first argument
      const StructDef *def = target.sinst->def;
      char desugared_name[256];
      snprintf(desugared_name, sizeof(desugared_name), "%s.%s", def->name, node->method_call.method_name);

      const Value *fn_val = env_peek(e, desugared_name);
      if (!fn_val || fn_val->type != VAL_FUNC) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Struct '%s' has no method '%s'", def->name, node->method_call.method_name);
        val_clear(&target);
        return val_new_err(err_msg, node->line);
      }

      int total_args = node->method_call.arg_count + 1;
      Value *call_args = (Value *)malloc(sizeof(Value) * total_args);
      call_args[0] = target; // passes receiver

      for (int i = 0; i < node->method_call.arg_count; i++) {
        call_args[i + 1] = eval(e, node->method_call.args[i]);
        if (call_args[i + 1].type == VAL_ERR) {
          Value err = val_copy(call_args[i + 1]);
          for (int j = 0; j <= i + 1; j++) val_clear(&call_args[j]);
          free(call_args);
          return err;
        }
      }

      // Strict argument check & execute function
      if (total_args != fn_val->func->param_count) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Method '%s' expects %d arguments, got %d",
                 node->method_call.method_name, fn_val->func->param_count - 1, node->method_call.arg_count);
        for (int j = 0; j < total_args; j++) val_clear(&call_args[j]);
        free(call_args);
        return val_new_err(err_msg, node->line);
      }

      Env *fn_env = env_new(e);
      for (int i = 0; i < total_args; i++) {
        env_def(fn_env, fn_val->func->params[i], call_args[i]);
        val_clear(&call_args[i]);
      }
      free(call_args);

      Value res = eval(fn_env, fn_val->func->body);
      env_del(fn_env);

      if (res.type == VAL_RETURN) {
        Value unwrapped = val_copy(*res.ret);
        val_clear(&res);
        return unwrapped;
      }
      if (res.type == VAL_BREAK || res.type == VAL_CONTINUE) {
        val_clear(&res);
        return val_new_int(0);
      }
      return res;
    }

    val_clear(&target);
    return val_new_err("Cannot call method on non-struct", node->line);
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
    Value *target_slot = eval_lvalue(e, node->index_assign.target);
    if (target_slot && target_slot->type == VAL_ARRAY && target_slot->arr) {
      Value index = eval(e, node->index_assign.index);
      if (index.type != VAL_INT) {
        val_clear(&index);
        return val_new_err("Array index must be an integer", node->line);
      }
      long idx = index.num;
      val_clear(&index);
      if (idx < 0) idx = target_slot->arr->count + idx;
      if (idx < 0 || idx >= target_slot->arr->count) {
        return val_new_err("Array index out of bounds", node->line);
      }
      Value new_val = eval(e, node->index_assign.value);
      val_clear(&target_slot->arr->items[idx]);
      target_slot->arr->items[idx] = val_copy(new_val);
      return new_val;
    }
    return val_new_err("Target for index assignment must evaluate to an array", node->line);
  }

  case AST_NEW: {
    // Optional allocator evaluation
    Value alloc_val = val_new_int(0);
    Arena *custom_arena = NULL;
    if (node->new_expr.allocator) {
      alloc_val = eval(e, node->new_expr.allocator);
      if (alloc_val.type == VAL_ARENA) {
        custom_arena = alloc_val.arena;
      }
    }

    // Instantiating struct type: new StructName { ... } or new(arena) StructName { ... }
    if (node->new_expr.type_name) {
      const StructDef *sdef = struct_def_lookup(node->new_expr.type_name);
      if (sdef) {
        Value sval = val_new_struct(sdef, custom_arena);
        if (node->new_expr.init_expr && node->new_expr.init_expr->type == AST_STRUCT_LIT) {
          const ASTNode *slit = node->new_expr.init_expr;
          for (int i = 0; i < slit->struct_lit.field_count; i++) {
            int fidx = struct_def_get_field_index(sdef, slit->struct_lit.field_names[i]);
            if (fidx >= 0) {
              Value fv = eval(e, slit->struct_lit.field_values[i]);
              val_clear(&sval.sinst->fields[fidx]);
              sval.sinst->fields[fidx] = val_copy(fv);
              val_clear(&fv);
            }
          }
        }
        val_clear(&alloc_val);
        return sval;
      }
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
      int c = (cond.type == VAL_INT) ? (cond.num != 0) : is_truthy(cond);
      if (cond.type != VAL_INT) val_clear(&cond);
      if (!c) break;

      if (res.type != VAL_INT) val_clear(&res);
      res = eval(e, node->while_stmt.body);
      if (res.type == VAL_BREAK) {
        if (res.type != VAL_INT) val_clear(&res);
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
    if (res.type == VAL_BREAK || res.type == VAL_RETURN || res.type == VAL_ERR) {
      return res;
    }
    // Even if continue, we run the step
    if (res.type != VAL_INT) val_clear(&res);
    Value sres = eval(e, node->for_body.step);
    if (sres.type != VAL_INT) val_clear(&sres);
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
          Value unwrapped = val_copy(*res.ret);
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
  size_t bytes_read = fread(input, 1, length, f);
  input[bytes_read] = '\0';
  fclose(f);

  Arena *a = arena_new(4096);
  ASTNode *ast = parse_source(a, input);
  free(input);

  Value result = eval(e, ast);

  arena_free(a);
  return result;
}
