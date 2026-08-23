#include "../include/parser.h"
#include "../include/opcodes.h"
#include <string.h>

// ============================================================
// Parser state
// ============================================================

typedef struct {
  Arena          *arena;
  const TokenList *tl;
  int              pos;
} Parser;

// ============================================================
// Helpers
// ============================================================

static const Token *peek(Parser *p) {
  if (p->pos >= p->tl->count) return &p->tl->tokens[p->tl->count]; // EOF
  return &p->tl->tokens[p->pos];
}

static const Token *advance(Parser *p) {
  const Token *t = peek(p);
  if (t->type != TOK_EOF) p->pos++;
  return t;
}

static int check_op(Parser *p, const char *op) {
  const Token *t = peek(p);
  return (t->type == TOK_OP && strcmp(t->sval, op) == 0);
}

static int check_ident(Parser *p, const char *kw) {
  const Token *t = peek(p);
  return (t->type == TOK_IDENT && strcmp(t->sval, kw) == 0);
}

static int match_op(Parser *p, const char *op) {
  if (check_op(p, op)) { advance(p); return 1; }
  return 0;
}

static int match_ident(Parser *p, const char *kw) {
  if (check_ident(p, kw)) { advance(p); return 1; }
  return 0;
}

static int is_type_token(Parser *p) {
  const Token *t = peek(p);
  if (t->type != TOK_IDENT) return 0;
  return (strcmp(t->sval, "int") == 0 || strcmp(t->sval, "float") == 0 ||
          strcmp(t->sval, "string") == 0 || strcmp(t->sval, "array") == 0 ||
          strcmp(t->sval, "var") == 0 || strcmp(t->sval, "let") == 0 ||
          strcmp(t->sval, "byte") == 0 || strcmp(t->sval, "bool") == 0 ||
          strcmp(t->sval, "Arena") == 0 || strcmp(t->sval, "arena") == 0);
}

// Forward declarations
static ASTNode *parse_expr(Parser *p);
static ASTNode *parse_statement(Parser *p);
static ASTNode *parse_block(Parser *p);

// P-2 fix: error reporting instead of silent dummy nodes
static void parse_error(Parser *p, const char *msg) {
  const Token *t = peek(p);
  fprintf(stderr, "VClang: syntax error at line %d: %s", t->line, msg);
  if (t->type == TOK_OP || t->type == TOK_IDENT)
    fprintf(stderr, " (got '%s')", t->sval);
  else if (t->type == TOK_EOF)
    fprintf(stderr, " (at end of input)");
  fprintf(stderr, "\n");
}

// P-3 fix: expect a specific operator, report error if missing
static int expect_op(Parser *p, const char *op) {
  if (check_op(p, op)) { advance(p); return 1; }
  fprintf(stderr, "VClang: syntax error at line %d: expected '%s'\n",
          peek(p)->line, op);
  return 0;
}

// ============================================================
// Expression parsing (recursive descent, precedence climbing)
// ============================================================

static ASTNode *parse_factor(Parser *p) {
  const Token *t = peek(p);
  ASTNode *base = NULL;

  // Unary NOT
  if (t->type == TOK_OP && strcmp(t->sval, "!") == 0) {
    advance(p);
    ASTNode *operand = parse_factor(p);
    return ast_unary_op(p->arena, OP_NOT, operand, t->line);
  }

  // New expression: new [1, 2, 3] or new(arena) [1, 2, 3] or new int[10] or new Arena()
  if (match_ident(p, "new")) {
    int line = t->line;
    ASTNode *allocator = NULL;
    if (match_op(p, "(")) {
      allocator = parse_expr(p);
      expect_op(p, ")");
    }

    // Direct array literal: new [1, 2, 3] or new(arena) [1, 2, 3]
    if (check_op(p, "[")) {
      ASTNode *init = parse_factor(p);
      base = ast_new_expr(p->arena, allocator, NULL, NULL, init, line);
    }
    // Type name: int, byte, float, string, Arena, etc.
    else if (peek(p)->type == TOK_IDENT) {
      const Token *type_tok = advance(p);
      const char *tname = type_tok->sval;

      // Sized or typed array: new int[10] or new int[]([1, 2, 3])
      if (match_op(p, "[")) {
        if (!check_op(p, "]")) {
          ASTNode *size_expr = parse_expr(p);
          expect_op(p, "]");
          base = ast_new_expr(p->arena, allocator, tname, size_expr, NULL, line);
        } else {
          expect_op(p, "]");
          ASTNode *init = NULL;
          if (match_op(p, "(")) {
            init = parse_expr(p);
            expect_op(p, ")");
          }
          base = ast_new_expr(p->arena, allocator, tname, NULL, init, line);
        }
      }
      // Instantiation: new Arena() or new Arena(4096)
      else if (match_op(p, "(")) {
        ASTNode *init = NULL;
        if (!check_op(p, ")")) {
          init = parse_expr(p);
        }
        expect_op(p, ")");
        base = ast_new_expr(p->arena, allocator, tname, NULL, init, line);
      } else {
        base = ast_new_expr(p->arena, allocator, tname, NULL, NULL, line);
      }
    } else {
      parse_error(p, "expected type or array literal after 'new'");
      base = ast_int_lit(p->arena, 0, line);
    }
  }
  // Array literal: [e1, e2, ...]
  else if (match_op(p, "[")) {
    int line = t->line;
    int elem_cap = 8;
    int elem_count = 0;
    const ASTNode **elems = (const ASTNode **)malloc(
        sizeof(ASTNode *) * elem_cap);

    if (!check_op(p, "]")) {
      elems[elem_count++] = parse_expr(p);
      while (match_op(p, ",")) {
        if (check_op(p, "]")) break;
        if (elem_count >= elem_cap) {
          elem_cap *= 2;
          elems = (const ASTNode **)realloc(
              elems, sizeof(ASTNode *) * elem_cap);
        }
        elems[elem_count++] = parse_expr(p);
      }
    }
    expect_op(p, "]");
    base = ast_array_lit(p->arena, elems, elem_count, line);
    free(elems);
  }
  // Integer literal
  else if (t->type == TOK_INT) {
    advance(p);
    base = ast_int_lit(p->arena, t->ival, t->line);
  }
  // Float literal
  else if (t->type == TOK_FLOAT) {
    advance(p);
    base = ast_float_lit(p->arena, t->fval, t->line);
  }
  // String literal
  else if (t->type == TOK_STRING) {
    advance(p);
    base = ast_string_lit(p->arena, t->sval, t->line);
  }
  // Parenthesized expression
  else if (match_op(p, "(")) {
    base = parse_expr(p);
    expect_op(p, ")"); // P-3: report missing paren
  }
  // Identifier or function call
  else if (t->type == TOK_IDENT) {
    advance(p);
    // Function call?
    if (match_op(p, "(")) {
      // Collect arguments using a temporary stack buffer
      int arg_cap = 8;
      int arg_count = 0;
      const ASTNode **args = (const ASTNode **)malloc(
          sizeof(ASTNode *) * arg_cap);

      if (!check_op(p, ")")) {
        args[arg_count++] = parse_expr(p);
        while (match_op(p, ",")) {
          if (arg_count >= arg_cap) {
            arg_cap *= 2;
            args = (const ASTNode **)realloc(
                args, sizeof(ASTNode *) * arg_cap);
          }
          args[arg_count++] = parse_expr(p);
        }
      }
      expect_op(p, ")"); // P-3: report missing paren

      base = ast_call(p->arena, t->sval, args, arg_count, t->line);
      free(args); // Temporary — the arena copy is inside ast_call
    } else {
      // Plain identifier
      base = ast_ident(p->arena, t->sval, t->line);
    }
  } else {
    // P-2 fix: report the error instead of silently producing a dummy node
    parse_error(p, "unexpected token");
    advance(p);
    // Return a dummy int 0 to avoid NULL propagation
    return ast_int_lit(p->arena, 0, t->line);
  }

  // Suffix indexing: base[idx]
  while (match_op(p, "[")) {
    int idx_line = peek(p)->line;
    ASTNode *idx = parse_expr(p);
    expect_op(p, "]");
    base = ast_index(p->arena, base, idx, idx_line);
  }

  return base;
}

static ASTNode *parse_term(Parser *p) {
  ASTNode *lhs = parse_factor(p);
  while (1) {
    const Token *t = peek(p);
    if (t->type == TOK_OP &&
        (strcmp(t->sval, "*") == 0 || strcmp(t->sval, "/") == 0 ||
         strcmp(t->sval, "%") == 0)) {
      OpType op = op_from_string(t->sval);
      int line = t->line;
      advance(p);
      ASTNode *rhs = parse_factor(p);
      lhs = ast_binary_op(p->arena, op, lhs, rhs, line);
    } else {
      break;
    }
  }
  return lhs;
}

static ASTNode *parse_add(Parser *p) {
  ASTNode *lhs = parse_term(p);
  while (1) {
    const Token *t = peek(p);
    if (t->type == TOK_OP &&
        (strcmp(t->sval, "+") == 0 || strcmp(t->sval, "-") == 0)) {
      OpType op = op_from_string(t->sval);
      int line = t->line;
      advance(p);
      ASTNode *rhs = parse_term(p);
      lhs = ast_binary_op(p->arena, op, lhs, rhs, line);
    } else {
      break;
    }
  }
  return lhs;
}

static ASTNode *parse_cmp(Parser *p) {
  ASTNode *lhs = parse_add(p);
  while (1) {
    const Token *t = peek(p);
    if (t->type != TOK_OP) break;
    const char *ops = t->sval;
    // Don't match bare '=' (that's assignment)
    if (strcmp(ops, "=") == 0) break;
    if (strcmp(ops, "==") == 0 || strcmp(ops, "!=") == 0 ||
        strcmp(ops, "<") == 0  || strcmp(ops, ">") == 0  ||
        strcmp(ops, "<=") == 0 || strcmp(ops, ">=") == 0) {
      OpType op = op_from_string(ops);
      int line = t->line;
      advance(p);
      ASTNode *rhs = parse_add(p);
      lhs = ast_binary_op(p->arena, op, lhs, rhs, line);
    } else {
      break;
    }
  }
  return lhs;
}

static ASTNode *parse_logical(Parser *p) {
  ASTNode *lhs = parse_cmp(p);
  while (1) {
    const Token *t = peek(p);
    if (t->type == TOK_OP &&
        (strcmp(t->sval, "&&") == 0 || strcmp(t->sval, "||") == 0)) {
      OpType op = op_from_string(t->sval);
      int line = t->line;
      advance(p);
      ASTNode *rhs = parse_cmp(p);
      lhs = ast_binary_op(p->arena, op, lhs, rhs, line);
    } else {
      break;
    }
  }
  return lhs;
}

static ASTNode *parse_expr(Parser *p) {
  return parse_logical(p);
}

// ============================================================
// Block: { stmts... }
// ============================================================

static ASTNode *parse_block(Parser *p) {
  int line = peek(p)->line;
  if (!match_op(p, "{")) {
    // P-3 fix: report missing opening brace
    fprintf(stderr, "VClang: syntax error at line %d: expected '{'\n", peek(p)->line);
  }

  int stmt_cap = 8;
  int stmt_count = 0;
  const ASTNode **stmts = (const ASTNode **)malloc(
      sizeof(ASTNode *) * stmt_cap);

  while (!check_op(p, "}") && peek(p)->type != TOK_EOF) {
    if (stmt_count >= stmt_cap) {
      stmt_cap *= 2;
      stmts = (const ASTNode **)realloc(stmts, sizeof(ASTNode *) * stmt_cap);
    }
    stmts[stmt_count++] = parse_statement(p);
  }
  expect_op(p, "}"); // P-3: report missing closing brace

  ASTNode *node = ast_block(p->arena, stmts, stmt_count, line);
  free(stmts);
  return node;
}

// ============================================================
// Statements
// ============================================================

static ASTNode *parse_statement(Parser *p) {
  const Token *t = peek(p);

  // Block
  if (check_op(p, "{")) {
    return parse_block(p);
  }

  // Function definition: func name(param1, param2) { ... }
  if (match_ident(p, "func") || match_ident(p, "def") || match_ident(p, "fn")) {
    const Token *name_tok = peek(p);
    if (name_tok->type != TOK_IDENT) {
      parse_error(p, "expected function name");
    }
    advance(p);
    expect_op(p, "(");

    int pcap = 8;
    int pcount = 0;
    const char **params = (const char **)malloc(sizeof(char *) * pcap);

    if (!check_op(p, ")")) {
      // Optional type keyword before parameter name
      if (is_type_token(p)) {
        advance(p);
        while (check_op(p, "[")) {
          advance(p);
          expect_op(p, "]");
        }
      }
      const Token *ptok = peek(p);
      if (ptok->type != TOK_IDENT) {
        parse_error(p, "expected parameter name");
      }
      params[pcount++] = ptok->sval;
      advance(p);

      while (match_op(p, ",")) {
        if (is_type_token(p)) {
          advance(p);
          while (check_op(p, "[")) {
            advance(p);
            expect_op(p, "]");
          }
        }
        const Token *pt = peek(p);
        if (pt->type != TOK_IDENT) {
          parse_error(p, "expected parameter name");
        }
        if (pcount >= pcap) {
          pcap *= 2;
          params = (const char **)realloc(params, sizeof(char *) * pcap);
        }
        params[pcount++] = pt->sval;
        advance(p);
      }
    }
    expect_op(p, ")");
    ASTNode *body = parse_block(p);
    ASTNode *fdef = ast_func_def(p->arena, name_tok->sval, params, pcount, body, t->line);
    free(params);
    return fdef;
  }

  // Variable declaration: int x = 5; or int[][] arr = [[1], [2]]; or int arr[5]; or let x = 5;
  if (is_type_token(p)) {
    const Token *type_tok = advance(p); // skip type keyword
    while (check_op(p, "[")) {
      if (p->pos + 1 < p->tl->count && p->tl->tokens[p->pos + 1].type == TOK_OP &&
          strcmp(p->tl->tokens[p->pos + 1].sval, "]") == 0) {
        advance(p); // skip '['
        advance(p); // skip ']'
      } else {
        break;
      }
    }
    const Token *name_tok = peek(p); // identifier name
    if (name_tok->type != TOK_IDENT) {
      parse_error(p, "expected variable name");
    }
    advance(p);

    // Fixed-size array declaration: int name[5]; or int name[5] = [1, 2, 3];
    if (check_op(p, "[")) {
      advance(p);
      ASTNode *size_expr = parse_expr(p);
      expect_op(p, "]");
      ASTNode *init_val = NULL;
      if (match_op(p, "=")) {
        init_val = parse_expr(p);
      }
      match_op(p, ";");
      ASTNode *new_node = ast_new_expr(p->arena, NULL, type_tok->sval, size_expr, init_val, t->line);
      return ast_decl(p->arena, name_tok->sval, new_node, t->line);
    }

    ASTNode *val = NULL;
    if (match_op(p, "=")) {
      val = parse_expr(p);
    }
    match_op(p, ";");
    return ast_decl(p->arena, name_tok->sval, val, t->line);
  }

  // If statement
  if (match_ident(p, "if")) {
    match_op(p, "(");
    ASTNode *cond = parse_expr(p);
    match_op(p, ")");
    ASTNode *then_body = parse_statement(p);
    ASTNode *else_body = NULL;
    if (check_ident(p, "else")) {
      advance(p);
      else_body = parse_statement(p);
    }
    return ast_if(p->arena, cond, then_body, else_body, t->line);
  }

  // While loop
  if (match_ident(p, "while")) {
    match_op(p, "(");
    ASTNode *cond = parse_expr(p);
    match_op(p, ")");
    ASTNode *body = parse_statement(p);
    return ast_while(p->arena, cond, body, t->line);
  }

  // For loop: for (init; cond; step) body
  if (match_ident(p, "for")) {
    match_op(p, "(");

    // Init statement
    ASTNode *init = parse_statement(p);

    // Condition
    ASTNode *cond = parse_expr(p);
    match_op(p, ";");

    // Step — check for assignment: IDENT = ...
    ASTNode *step;
    const Token *t1 = peek(p);
    const Token *t2 = (p->pos + 1 < p->tl->count)
                          ? &p->tl->tokens[p->pos + 1]
                          : t1;
    if (t1->type == TOK_IDENT && t2->type == TOK_OP &&
        strcmp(t2->sval, "=") == 0) {
      const Token *name_tok = advance(p);
      advance(p); // skip '='
      ASTNode *val = parse_expr(p);
      step = ast_assign(p->arena, name_tok->sval, val, t1->line);
    } else {
      step = parse_expr(p);
    }
    match_op(p, ")");

    ASTNode *body = parse_statement(p); // P-6 fix: consistent with while/if

    // Construct: block(init, while(cond, for_body(body, step)))
    ASTNode *fb = ast_for_body(p->arena, body, step, t->line);
    ASTNode *wh = ast_while(p->arena, cond, fb, t->line);

    const ASTNode *outer_stmts[2] = { init, wh };
    return ast_block(p->arena, outer_stmts, 2, t->line);
  }

  // Return: return expr; or return;
  if (match_ident(p, "return")) {
    ASTNode *expr = NULL;
    if (!check_op(p, ";")) {
      expr = parse_expr(p);
    }
    match_op(p, ";");
    return ast_return(p->arena, expr, t->line);
  }

  // Break
  if (match_ident(p, "break")) {
    match_op(p, ";");
    return ast_break(p->arena, t->line);
  }

  // Continue
  if (match_ident(p, "continue")) {
    match_op(p, ";");
    return ast_continue(p->arena, t->line);
  }

  // Check for assignment: ident = expr; or ident[idx] = expr;
  if (t->type == TOK_IDENT && p->pos + 1 < p->tl->count) {
    const Token *next = &p->tl->tokens[p->pos + 1];
    if (next->type == TOK_OP && strcmp(next->sval, "=") == 0) {
      const Token *name_tok = advance(p);
      advance(p); // skip '='
      ASTNode *val = parse_expr(p);
      match_op(p, ";");
      return ast_assign(p->arena, name_tok->sval, val, t->line);
    }
    if (next->type == TOK_OP && strcmp(next->sval, "[") == 0) {
      int start_pos = p->pos;
      const Token *name_tok = advance(p);
      ASTNode *target = ast_ident(p->arena, name_tok->sval, name_tok->line);
      while (match_op(p, "[")) {
        ASTNode *idx = parse_expr(p);
        expect_op(p, "]");
        if (check_op(p, "[")) {
          target = ast_index(p->arena, target, idx, name_tok->line);
        } else if (match_op(p, "=")) {
          ASTNode *val = parse_expr(p);
          match_op(p, ";");
          return ast_index_assign(p->arena, target, idx, val, t->line);
        } else {
          break;
        }
      }
      // Backtrack and fall through to expression statement
      p->pos = start_pos;
    }
  }

  // Expression statement
  ASTNode *expr = parse_expr(p);
  match_op(p, ";");
  return expr;
}

// ============================================================
// Public API
// ============================================================

ASTNode *parse(Arena *a, const TokenList *tl) {
  Parser p = { .arena = a, .tl = tl, .pos = 0 };

  int stmt_cap = 16;
  int stmt_count = 0;
  const ASTNode **stmts = (const ASTNode **)malloc(
      sizeof(ASTNode *) * stmt_cap);

  while (peek(&p)->type != TOK_EOF) {
    if (stmt_count >= stmt_cap) {
      stmt_cap *= 2;
      stmts = (const ASTNode **)realloc(stmts, sizeof(ASTNode *) * stmt_cap);
    }
    stmts[stmt_count++] = parse_statement(&p);
  }

  ASTNode *prog = ast_program(a, stmts, stmt_count);
  free(stmts);
  return prog;
}

ASTNode *parse_source(Arena *a, const char *source) {
  TokenList tl = tokenize(source);
  ASTNode *ast = parse(a, &tl);
  tokenlist_free(&tl);
  return ast;
}
