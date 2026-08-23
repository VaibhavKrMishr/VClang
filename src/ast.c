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
                      const char **params, int param_count,
                      const ASTNode *body, int line) {
  ASTNode *n              = (ASTNode *)arena_alloc(a, sizeof(ASTNode));
  n->type                 = AST_FUNC_DEF;
  n->line                 = line;
  n->func_def.name        = arena_strdup(a, name);
  n->func_def.param_count = param_count;
  if (param_count > 0) {
    const char **p = (const char **)arena_alloc(a, sizeof(char *) * param_count);
    for (int i = 0; i < param_count; i++) {
      p[i] = arena_strdup(a, params[i]);
    }
    n->func_def.params = p;
  } else {
    n->func_def.params = NULL;
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


