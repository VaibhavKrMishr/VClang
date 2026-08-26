#include "../include/ast.h"

// All constructors allocate from the arena. No individual free needed.

ASTNode *ast_int_lit(Arena *a, long val, int line) {
  ASTNode *n = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type    = AST_INT_LIT;
  n->line    = line;
  n->int_val = val;
  return n;
}

ASTNode *ast_float_lit(Arena *a, double val, int line) {
  ASTNode *n = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type      = AST_FLOAT_LIT;
  n->line      = line;
  n->float_val = val;
  return n;
}

ASTNode *ast_string_lit(Arena *a, const char *s, int line) {
  ASTNode *n = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type    = AST_STRING_LIT;
  n->line    = line;
  n->str_val = arena_strdup(a, s);
  return n;
}

ASTNode *ast_ident(Arena *a, const char *name, int line) {
  ASTNode *n = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type    = AST_IDENT;
  n->line    = line;
  n->str_val = arena_strdup(a, name);
  return n;
}

ASTNode *ast_binary_op(Arena *a, OpType op,
                       const ASTNode *lhs, const ASTNode *rhs, int line) {
  ASTNode *n     = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type        = AST_BINARY_OP;
  n->line        = line;
  n->binary.op   = op;
  n->binary.lhs  = lhs;
  n->binary.rhs  = rhs;
  return n;
}

ASTNode *ast_unary_op(Arena *a, OpType op,
                      const ASTNode *operand, int line) {
  ASTNode *n       = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type          = AST_UNARY_OP;
  n->line          = line;
  n->unary.op      = op;
  n->unary.operand = operand;
  return n;
}

ASTNode *ast_call(Arena *a, const char *func_name,
                  const ASTNode **args, int arg_count, int line) {
  ASTNode *n          = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type             = AST_CALL;
  n->line             = line;
  n->call.func_name   = arena_strdup(a, func_name);
  n->call.arg_count   = arg_count;
  // Copy the pointer array into the arena
  if (arg_count > 0) {
    const ASTNode **arr = (const ASTNode **)arena_alloc(
        a, sizeof(ASTNode *) * arg_count);
    memcpy(arr, args, sizeof(ASTNode *) * arg_count);
    n->call.args = arr;
  } else {
    n->call.args = NULL;
  }
  return n;
}

ASTNode *ast_decl(Arena *a, const char *name,
                  const ASTNode *value, int line) {
  ASTNode *n    = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type       = AST_DECL;
  n->line       = line;
  n->decl.name  = arena_strdup(a, name);
  n->decl.value = value;
  return n;
}

ASTNode *ast_assign(Arena *a, const char *name,
                    const ASTNode *value, int line) {
  ASTNode *n    = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type       = AST_ASSIGN;
  n->line       = line;
  n->decl.name  = arena_strdup(a, name);
  n->decl.value = value;
  return n;
}

ASTNode *ast_if(Arena *a, const ASTNode *cond,
                const ASTNode *then_body, const ASTNode *else_body,
                int line) {
  ASTNode *n            = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type               = AST_IF;
  n->line               = line;
  n->if_stmt.cond       = cond;
  n->if_stmt.then_body  = then_body;
  n->if_stmt.else_body  = else_body;
  return n;
}

ASTNode *ast_while(Arena *a, const ASTNode *cond,
                   const ASTNode *body, int line) {
  ASTNode *n           = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type              = AST_WHILE;
  n->line              = line;
  n->while_stmt.cond   = cond;
  n->while_stmt.body   = body;
  return n;
}

ASTNode *ast_for_body(Arena *a, const ASTNode *body,
                      const ASTNode *step, int line) {
  ASTNode *n        = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type           = AST_FOR_BODY;
  n->line           = line;
  n->for_body.body  = body;
  n->for_body.step  = step;
  return n;
}

ASTNode *ast_block(Arena *a, const ASTNode **stmts, int count, int line) {
  ASTNode *n          = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type             = AST_BLOCK;
  n->line             = line;
  n->block.stmt_count = count;
  if (count > 0) {
    const ASTNode **arr = (const ASTNode **)arena_alloc(
        a, sizeof(ASTNode *) * count);
    memcpy(arr, stmts, sizeof(ASTNode *) * count);
    n->block.stmts = arr;
  } else {
    n->block.stmts = NULL;
  }
  return n;
}

ASTNode *ast_program(Arena *a, const ASTNode **stmts, int count) {
  ASTNode *n          = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type             = AST_PROGRAM;
  n->line             = 0;
  n->block.stmt_count = count;
  if (count > 0) {
    const ASTNode **arr = (const ASTNode **)arena_alloc(
        a, sizeof(ASTNode *) * count);
    memcpy(arr, stmts, sizeof(ASTNode *) * count);
    n->block.stmts = arr;
  } else {
    n->block.stmts = NULL;
  }
  return n;
}

ASTNode *ast_return(Arena *a, const ASTNode *expr, int line) {
  ASTNode *n   = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type      = AST_RETURN;
  n->line      = line;
  n->ret.expr  = expr;
  return n;
}

ASTNode *ast_break(Arena *a, int line) {
  ASTNode *n = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type    = AST_BREAK;
  n->line    = line;
  return n;
}

ASTNode *ast_continue(Arena *a, int line) {
  ASTNode *n = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type    = AST_CONTINUE;
  n->line    = line;
  return n;
}

ASTNode *ast_func_def(Arena *a, const char *name,
                      const char **param_types, const char **params, int param_count,
                      const ASTNode *body, int line) {
  ASTNode *n              = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type                 = AST_FUNC_DEF;
  n->line                 = line;
  n->func_def.name        = arena_strdup(a, name);
  n->func_def.param_count = param_count;
  if (param_count > 0) {
    const char **p = (const char **)arena_alloc(a, sizeof(char *) * param_count);
    const char **pt = (const char **)arena_alloc(a, sizeof(char *) * param_count);
    for (int i = 0; i < param_count; i++) {
      p[i] = arena_strdup(a, params[i]);
      pt[i] = (param_types && param_types[i]) ? arena_strdup(a, param_types[i]) : NULL;
    }
    n->func_def.params = p;
    n->func_def.param_types = pt;
  } else {
    n->func_def.params = NULL;
    n->func_def.param_types = NULL;
  }
  n->func_def.body = body;
  return n;
}

ASTNode *ast_array_lit(Arena *a, const ASTNode **elements, int count, int line) {
  ASTNode *n            = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type               = AST_ARRAY_LIT;
  n->line               = line;
  n->array_lit.count    = count;
  if (count > 0) {
    const ASTNode **elems = (const ASTNode **)arena_alloc(
        a, sizeof(ASTNode *) * count);
    memcpy(elems, elements, sizeof(ASTNode *) * count);
    n->array_lit.elements = elems;
  } else {
    n->array_lit.elements = NULL;
  }
  return n;
}

ASTNode *ast_index(Arena *a, const ASTNode *target, const ASTNode *index, int line) {
  ASTNode *n           = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type              = AST_INDEX;
  n->line              = line;
  n->index_expr.target = target;
  n->index_expr.index  = index;
  return n;
}

ASTNode *ast_index_assign(Arena *a, const ASTNode *target, const ASTNode *index,
                          const ASTNode *value, int line) {
  ASTNode *n             = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type                = AST_INDEX_ASSIGN;
  n->line                = line;
  n->index_assign.target = target;
  n->index_assign.index  = index;
  n->index_assign.value  = value;
  return n;
}

ASTNode *ast_new_expr(Arena *a, const ASTNode *allocator, const char *type_name,
                      const ASTNode *size_expr, const ASTNode *init_expr, int line) {
  ASTNode *n            = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type               = AST_NEW;
  n->line               = line;
  n->new_expr.allocator = allocator;
  n->new_expr.type_name = type_name ? arena_strdup(a, type_name) : NULL;
  n->new_expr.size_expr = size_expr;
  n->new_expr.init_expr = init_expr;
  return n;
}

ASTNode *ast_struct_def(Arena *a, const char *name,
                        const char **field_types, const char **field_names, int field_count,
                        const ASTNode **methods, int method_count, int line) {
  ASTNode *n                 = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type                    = AST_STRUCT_DEF;
  n->line                    = line;
  n->struct_def.name         = arena_strdup(a, name);
  n->struct_def.field_count  = field_count;
  if (field_count > 0) {
    const char **ftypes = (const char **)arena_alloc(a, sizeof(char *) * field_count);
    const char **fnames = (const char **)arena_alloc(a, sizeof(char *) * field_count);
    for (int i = 0; i < field_count; i++) {
      ftypes[i] = arena_strdup(a, field_types[i]);
      fnames[i] = arena_strdup(a, field_names[i]);
    }
    n->struct_def.field_types = ftypes;
    n->struct_def.field_names = fnames;
  } else {
    n->struct_def.field_types = NULL;
    n->struct_def.field_names = NULL;
  }
  n->struct_def.method_count = method_count;
  if (method_count > 0) {
    const ASTNode **m = (const ASTNode **)arena_alloc(a, sizeof(ASTNode *) * method_count);
    memcpy(m, methods, sizeof(ASTNode *) * method_count);
    n->struct_def.methods = m;
  } else {
    n->struct_def.methods = NULL;
  }
  return n;
}

ASTNode *ast_struct_lit(Arena *a, const char *type_name,
                        const char **field_names, const ASTNode **field_values, int field_count, int line) {
  ASTNode *n                 = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type                    = AST_STRUCT_LIT;
  n->line                    = line;
  n->struct_lit.type_name    = arena_strdup(a, type_name);
  n->struct_lit.field_count  = field_count;
  if (field_count > 0) {
    const char **fnames = (const char **)arena_alloc(a, sizeof(char *) * field_count);
    const ASTNode **fvals = (const ASTNode **)arena_alloc(a, sizeof(ASTNode *) * field_count);
    for (int i = 0; i < field_count; i++) {
      fnames[i] = arena_strdup(a, field_names[i]);
    }
    memcpy(fvals, field_values, sizeof(ASTNode *) * field_count);
    n->struct_lit.field_names = fnames;
    n->struct_lit.field_values = fvals;
  } else {
    n->struct_lit.field_names = NULL;
    n->struct_lit.field_values = NULL;
  }
  return n;
}

ASTNode *ast_member_access(Arena *a, const ASTNode *target, const char *member, int line) {
  ASTNode *n                 = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type                    = AST_MEMBER_ACCESS;
  n->line                    = line;
  n->member_access.target    = target;
  n->member_access.member    = arena_strdup(a, member);
  return n;
}

ASTNode *ast_member_assign(Arena *a, const ASTNode *target, const char *member,
                           OpType op, const ASTNode *value, int line) {
  ASTNode *n                 = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type                    = AST_MEMBER_ASSIGN;
  n->line                    = line;
  n->member_assign.target    = target;
  n->member_assign.member    = arena_strdup(a, member);
  n->member_assign.op        = op;
  n->member_assign.value     = value;
  return n;
}

ASTNode *ast_method_call(Arena *a, const ASTNode *target, const char *method_name,
                         const ASTNode **args, int arg_count, int line) {
  ASTNode *n                 = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type                    = AST_METHOD_CALL;
  n->line                    = line;
  n->method_call.target      = target;
  n->method_call.method_name = arena_strdup(a, method_name);
  n->method_call.arg_count   = arg_count;
  if (arg_count > 0) {
    const ASTNode **a_args = (const ASTNode **)arena_alloc(a, sizeof(ASTNode *) * arg_count);
    memcpy(a_args, args, sizeof(ASTNode *) * arg_count);
    n->method_call.args = a_args;
  } else {
    n->method_call.args = NULL;
  }
  return n;
}


