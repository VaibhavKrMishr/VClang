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

  // Unary NOT
  if (t->type == TOK_OP && strcmp(t->sval, "!") == 0) {
    advance(p);
    ASTNode *operand = parse_factor(p);
    return ast_unary_op(p->arena, OP_NOT, operand, t->line);
  }

  // Integer literal
  if (t->type == TOK_INT) {
    advance(p);
    return ast_int_lit(p->arena, t->ival, t->line);
  }

  // Float literal
  if (t->type == TOK_FLOAT) {
    advance(p);
    return ast_float_lit(p->arena, t->fval, t->line);
  }

  // String literal
  if (t->type == TOK_STRING) {
    advance(p);
    return ast_string_lit(p->arena, t->sval, t->line);
  }

  // Parenthesized expression
  if (match_op(p, "(")) {
    ASTNode *e = parse_expr(p);
    expect_op(p, ")"); // P-3: report missing paren
    return e;
  }

  // Identifier or function call
  if (t->type == TOK_IDENT) {
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

      ASTNode *node = ast_call(p->arena, t->sval, args, arg_count, t->line);
      free(args); // Temporary — the arena copy is inside ast_call
      return node;
    }
    // Plain identifier
    return ast_ident(p->arena, t->sval, t->line);
  }

  // P-2 fix: report the error instead of silently producing a dummy node
  parse_error(p, "unexpected token");
  advance(p);
  // Return a dummy int 0 to avoid NULL propagation
  return ast_int_lit(p->arena, 0, t->line);
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

  // Variable declaration: int/float/string name = expr;
  if (check_ident(p, "int") || check_ident(p, "float") ||
      check_ident(p, "string")) {
    advance(p); // skip type keyword
    const Token *name_tok = advance(p); // identifier name
    match_op(p, "=");
    ASTNode *val = parse_expr(p);
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

  // Return
  if (match_ident(p, "return")) {
    ASTNode *expr = parse_expr(p);
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

  // Assignment: ident = expr;
  if (t->type == TOK_IDENT && p->pos + 1 < p->tl->count) {
    const Token *next = &p->tl->tokens[p->pos + 1];
    if (next->type == TOK_OP && strcmp(next->sval, "=") == 0) {
      const Token *name_tok = advance(p);
      advance(p); // skip '='
      ASTNode *val = parse_expr(p);
      match_op(p, ";");
      return ast_assign(p->arena, name_tok->sval, val, t->line);
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
