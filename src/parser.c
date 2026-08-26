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

static int is_reserved_keyword(const char *s) {
  return (strcmp(s, "int") == 0 || strcmp(s, "float") == 0 ||
          strcmp(s, "string") == 0 || strcmp(s, "array") == 0 ||
          strcmp(s, "var") == 0 || strcmp(s, "let") == 0 ||
          strcmp(s, "byte") == 0 || strcmp(s, "bool") == 0 ||
          strcmp(s, "Arena") == 0 || strcmp(s, "arena") == 0 ||
          strcmp(s, "func") == 0 || strcmp(s, "def") == 0 ||
          strcmp(s, "fn") == 0 || strcmp(s, "if") == 0 ||
          strcmp(s, "else") == 0 || strcmp(s, "while") == 0 ||
          strcmp(s, "for") == 0 || strcmp(s, "return") == 0 ||
          strcmp(s, "break") == 0 || strcmp(s, "continue") == 0 ||
          strcmp(s, "new") == 0 || strcmp(s, "struct") == 0);
}

static int is_type_token(Parser *p) {
  const Token *t = peek(p);
  if (t->type != TOK_IDENT) return 0;
  if (strcmp(t->sval, "int") == 0 || strcmp(t->sval, "float") == 0 ||
      strcmp(t->sval, "string") == 0 || strcmp(t->sval, "array") == 0 ||
      strcmp(t->sval, "var") == 0 || strcmp(t->sval, "let") == 0 ||
      strcmp(t->sval, "byte") == 0 || strcmp(t->sval, "bool") == 0 ||
      strcmp(t->sval, "Arena") == 0 || strcmp(t->sval, "arena") == 0) {
    return 1;
  }
  if (is_reserved_keyword(t->sval)) return 0;

  // Lookahead: If followed by an identifier or '[' ']' ... identifier:
  if (p->pos + 1 < p->tl->count) {
    const Token *next = &p->tl->tokens[p->pos + 1];
    if (next->type == TOK_IDENT && !is_reserved_keyword(next->sval)) {
      return 1;
    }
    if (next->type == TOK_OP && strcmp(next->sval, "[") == 0) {
      int k = p->pos + 1;
      while (k < p->tl->count && p->tl->tokens[k].type == TOK_OP && strcmp(p->tl->tokens[k].sval, "[") == 0) {
        k++;
        while (k < p->tl->count && !(p->tl->tokens[k].type == TOK_OP && strcmp(p->tl->tokens[k].sval, "]") == 0)) {
          k++;
        }
        if (k < p->tl->count) k++;
      }
      if (k < p->tl->count && p->tl->tokens[k].type == TOK_IDENT && !is_reserved_keyword(p->tl->tokens[k].sval)) {
        return 1;
      }
    }
  }
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
  ASTNode *base = NULL;

  // Unary NOT, MINUS, PLUS
  if (t->type == TOK_OP && strcmp(t->sval, "!") == 0) {
    advance(p);
    ASTNode *operand = parse_factor(p);
    return ast_unary_op(p->arena, OP_NOT, operand, t->line);
  }
  if (t->type == TOK_OP && strcmp(t->sval, "-") == 0) {
    advance(p);
    ASTNode *operand = parse_factor(p);
    ASTNode *zero = ast_int_lit(p->arena, 0, t->line);
    return ast_binary_op(p->arena, OP_SUB, zero, operand, t->line);
  }
  if (t->type == TOK_OP && strcmp(t->sval, "+") == 0) {
    advance(p);
    return parse_factor(p);
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
    // Type name: int, byte, float, string, Arena, or custom Struct
    else if (peek(p)->type == TOK_IDENT) {
      const Token *type_tok = advance(p);
      const char *tname = type_tok->sval;

      // Struct literal: new StructName { ... } or new(arena) StructName { ... }
      if (check_op(p, "{")) {
        advance(p); // skip '{'
        int fcap = 8, fcount = 0;
        const char **fnames = (const char **)malloc(sizeof(char *) * fcap);
        const ASTNode **fvals = (const ASTNode **)malloc(sizeof(ASTNode *) * fcap);

        if (!check_op(p, "}")) {
          const Token *fn = peek(p);
          if (fn->type != TOK_IDENT) parse_error(p, "expected field name in struct literal");
          advance(p);
          expect_op(p, ":");
          ASTNode *fval = parse_expr(p);
          fnames[fcount] = fn->sval;
          fvals[fcount] = fval;
          fcount++;

          while (match_op(p, ",")) {
            if (check_op(p, "}")) break;
            const Token *fn_next = peek(p);
            if (fn_next->type != TOK_IDENT) parse_error(p, "expected field name in struct literal");
            advance(p);
            expect_op(p, ":");
            ASTNode *fval_next = parse_expr(p);
            if (fcount >= fcap) {
              fcap *= 2;
              fnames = (const char **)realloc(fnames, sizeof(char *) * fcap);
              fvals = (const ASTNode **)realloc(fvals, sizeof(ASTNode *) * fcap);
            }
            fnames[fcount] = fn_next->sval;
            fvals[fcount] = fval_next;
            fcount++;
          }
        }
        expect_op(p, "}");
        ASTNode *slit = ast_struct_lit(p->arena, tname, fnames, fvals, fcount, line);
        free(fnames);
        free(fvals);
        base = ast_new_expr(p->arena, allocator, tname, NULL, slit, line);
      }
      // Sized or typed array: new int[10] or new int[]([1, 2, 3])
      else if (match_op(p, "[")) {
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
  // Struct literal: StructName { field1: expr, field2: expr, ... }
  else if (t->type == TOK_IDENT && p->pos + 1 < p->tl->count &&
           p->tl->tokens[p->pos + 1].type == TOK_OP &&
           strcmp(p->tl->tokens[p->pos + 1].sval, "{") == 0 &&
           !is_reserved_keyword(t->sval)) {
    const Token *type_tok = advance(p);
    advance(p); // skip '{'
    int line = type_tok->line;
    int fcap = 8, fcount = 0;
    const char **fnames = (const char **)malloc(sizeof(char *) * fcap);
    const ASTNode **fvals = (const ASTNode **)malloc(sizeof(ASTNode *) * fcap);

    if (!check_op(p, "}")) {
      const Token *fn = peek(p);
      if (fn->type != TOK_IDENT) parse_error(p, "expected field name in struct literal");
      advance(p);
      expect_op(p, ":");
      ASTNode *fval = parse_expr(p);
      fnames[fcount] = fn->sval;
      fvals[fcount] = fval;
      fcount++;

      while (match_op(p, ",")) {
        if (check_op(p, "}")) break;
        const Token *fn_next = peek(p);
        if (fn_next->type != TOK_IDENT) parse_error(p, "expected field name in struct literal");
        advance(p);
        expect_op(p, ":");
        ASTNode *fval_next = parse_expr(p);
        if (fcount >= fcap) {
          fcap *= 2;
          fnames = (const char **)realloc(fnames, sizeof(char *) * fcap);
          fvals = (const ASTNode **)realloc(fvals, sizeof(ASTNode *) * fcap);
        }
        fnames[fcount] = fn_next->sval;
        fvals[fcount] = fval_next;
        fcount++;
      }
    }
    expect_op(p, "}");
    base = ast_struct_lit(p->arena, type_tok->sval, fnames, fvals, fcount, line);
    free(fnames);
    free(fvals);
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

  // Suffix indexing, member access, and method calls
  while (1) {
    if (match_op(p, "[")) {
      int idx_line = peek(p)->line;
      ASTNode *idx = parse_expr(p);
      expect_op(p, "]");
      base = ast_index(p->arena, base, idx, idx_line);
    } else if (match_op(p, ".")) {
      int dot_line = peek(p)->line;
      const Token *member_tok = peek(p);
      if (member_tok->type != TOK_IDENT) {
        parse_error(p, "expected field or method name after '.'");
        break;
      }
      advance(p);
      const char *member_name = member_tok->sval;

      // Method call: base.method(args...)
      if (match_op(p, "(")) {
        int arg_cap = 8, arg_count = 0;
        const ASTNode **args = (const ASTNode **)malloc(sizeof(ASTNode *) * arg_cap);
        if (!check_op(p, ")")) {
          args[arg_count++] = parse_expr(p);
          while (match_op(p, ",")) {
            if (arg_count >= arg_cap) {
              arg_cap *= 2;
              args = (const ASTNode **)realloc(args, sizeof(ASTNode *) * arg_cap);
            }
            args[arg_count++] = parse_expr(p);
          }
        }
        expect_op(p, ")");
        base = ast_method_call(p->arena, base, member_name, args, arg_count, dot_line);
        free(args);
      } else {
        base = ast_member_access(p->arena, base, member_name, dot_line);
      }
    } else {
      break;
    }
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

  // Struct definition: struct Name { type field; func method(...) { ... } }
  if (match_ident(p, "struct")) {
    const Token *name_tok = peek(p);
    if (name_tok->type != TOK_IDENT) {
      parse_error(p, "expected struct name");
    }
    advance(p);
    expect_op(p, "{");

    int fcap = 8, fcount = 0;
    const char **ftypes = (const char **)malloc(sizeof(char *) * fcap);
    const char **fnames = (const char **)malloc(sizeof(char *) * fcap);

    int mcap = 4, mcount = 0;
    const ASTNode **methods = (const ASTNode **)malloc(sizeof(ASTNode *) * mcap);

    while (!check_op(p, "}") && peek(p)->type != TOK_EOF) {
      // Inline method definition: func method_name(...) { ... }
      if (match_ident(p, "func") || match_ident(p, "def") || match_ident(p, "fn")) {
        const Token *mname_tok = peek(p);
        if (mname_tok->type != TOK_IDENT) {
          parse_error(p, "expected method name");
        }
        advance(p);
        expect_op(p, "(");

        int pcap = 8, pcount = 0;
        const char **params = (const char **)malloc(sizeof(char *) * pcap);
        const char **param_types = (const char **)malloc(sizeof(char *) * pcap);

        if (!check_op(p, ")")) {
          const char *ptype = NULL;
          if (is_type_token(p)) {
            ptype = advance(p)->sval;
            int is_arr = 0;
            while (check_op(p, "[")) {
              advance(p);
              expect_op(p, "]");
              is_arr = 1;
            }
            if (is_arr) ptype = "array";
          }
          const Token *ptok = peek(p);
          if (ptok->type != TOK_IDENT) parse_error(p, "expected parameter name");
          param_types[pcount] = ptype;
          params[pcount++] = ptok->sval;
          advance(p);

          while (match_op(p, ",")) {
            ptype = NULL;
            if (is_type_token(p)) {
              ptype = advance(p)->sval;
              int is_arr = 0;
              while (check_op(p, "[")) {
                advance(p);
                expect_op(p, "]");
                is_arr = 1;
              }
              if (is_arr) ptype = "array";
            }
            const Token *pt = peek(p);
            if (pt->type != TOK_IDENT) parse_error(p, "expected parameter name");
            if (pcount >= pcap) {
              pcap *= 2;
              params = (const char **)realloc(params, sizeof(char *) * pcap);
              param_types = (const char **)realloc(param_types, sizeof(char *) * pcap);
            }
            param_types[pcount] = ptype;
            params[pcount++] = pt->sval;
            advance(p);
          }
        }
        expect_op(p, ")");
        ASTNode *mbody = parse_block(p);
        char desugared_name[256];
        snprintf(desugared_name, sizeof(desugared_name), "%s.%s", name_tok->sval, mname_tok->sval);
        ASTNode *mdef = ast_func_def(p->arena, desugared_name, param_types, params, pcount, mbody, mname_tok->line);
        free(params);
        free(param_types);

        if (mcount >= mcap) {
          mcap *= 2;
          methods = (const ASTNode **)realloc(methods, sizeof(ASTNode *) * mcap);
        }
        methods[mcount++] = mdef;
      }
      // Field definition: type field_name;
      else if (is_type_token(p)) {
        const Token *ftype_tok = advance(p);
        while (check_op(p, "[")) {
          advance(p);
          expect_op(p, "]");
        }
        const Token *fname_tok = peek(p);
        if (fname_tok->type != TOK_IDENT) parse_error(p, "expected field name");
        advance(p);
        match_op(p, ";");

        if (fcount >= fcap) {
          fcap *= 2;
          ftypes = (const char **)realloc(ftypes, sizeof(char *) * fcap);
          fnames = (const char **)realloc(fnames, sizeof(char *) * fcap);
        }
        ftypes[fcount] = ftype_tok->sval;
        fnames[fcount++] = fname_tok->sval;
      } else {
        parse_error(p, "expected field or method declaration in struct");
        advance(p);
      }
    }
    expect_op(p, "}");
    ASTNode *sdef = ast_struct_def(p->arena, name_tok->sval, ftypes, fnames, fcount, methods, mcount, t->line);
    free(ftypes);
    free(fnames);
    free(methods);
    return sdef;
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
    const char **param_types = (const char **)malloc(sizeof(char *) * pcap);

    if (!check_op(p, ")")) {
      const char *ptype = NULL;
      // Optional type keyword before parameter name
      if (is_type_token(p)) {
        ptype = advance(p)->sval;
        int is_arr = 0;
        while (check_op(p, "[")) {
          advance(p);
          expect_op(p, "]");
          is_arr = 1;
        }
        if (is_arr) ptype = "array";
      }
      const Token *ptok = peek(p);
      if (ptok->type != TOK_IDENT) {
        parse_error(p, "expected parameter name");
      }
      param_types[pcount] = ptype;
      params[pcount++] = ptok->sval;
      advance(p);

      while (match_op(p, ",")) {
        ptype = NULL;
        if (is_type_token(p)) {
          ptype = advance(p)->sval;
          int is_arr = 0;
          while (check_op(p, "[")) {
            advance(p);
            expect_op(p, "]");
            is_arr = 1;
          }
          if (is_arr) ptype = "array";
        }
        const Token *pt = peek(p);
        if (pt->type != TOK_IDENT) {
          parse_error(p, "expected parameter name");
        }
        if (pcount >= pcap) {
          pcap *= 2;
          params = (const char **)realloc(params, sizeof(char *) * pcap);
          param_types = (const char **)realloc(param_types, sizeof(char *) * pcap);
        }
        param_types[pcount] = ptype;
        params[pcount++] = pt->sval;
        advance(p);
      }
    }
    expect_op(p, ")");
    ASTNode *body = parse_block(p);
    ASTNode *fdef = ast_func_def(p->arena, name_tok->sval, param_types, params, pcount, body, t->line);
    free(params);
    free(param_types);
    return fdef;
  }

  // Variable declaration: int x = 5; or int[][] arr = [[1], [2]]; or Vector2 v;
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
    } else {
      // Default zero-value instantiation
      if (strcmp(type_tok->sval, "int") == 0 || strcmp(type_tok->sval, "byte") == 0 ||
          strcmp(type_tok->sval, "bool") == 0 || strcmp(type_tok->sval, "var") == 0 ||
          strcmp(type_tok->sval, "let") == 0) {
        val = ast_int_lit(p->arena, 0, t->line);
      } else if (strcmp(type_tok->sval, "float") == 0) {
        val = ast_float_lit(p->arena, 0.0, t->line);
      } else if (strcmp(type_tok->sval, "string") == 0) {
        val = ast_string_lit(p->arena, "", t->line);
      } else if (strcmp(type_tok->sval, "array") == 0) {
        val = ast_array_lit(p->arena, NULL, 0, t->line);
      } else {
        // Struct type zero value
        val = ast_struct_lit(p->arena, type_tok->sval, NULL, NULL, 0, t->line);
      }
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

    // Step — check for assignment or inc/dec: IDENT = ..., IDENT++, etc.
    ASTNode *step = NULL;
    const Token *t1 = peek(p);
    const Token *t2 = (p->pos + 1 < p->tl->count)
                          ? &p->tl->tokens[p->pos + 1]
                          : t1;
    if (t1->type == TOK_IDENT && t2->type == TOK_OP) {
      if (strcmp(t2->sval, "=") == 0) {
        const Token *name_tok = advance(p);
        advance(p); // skip '='
        ASTNode *val = parse_expr(p);
        step = ast_assign(p->arena, name_tok->sval, val, t1->line);
      } else if (strcmp(t2->sval, "+=") == 0) {
        const Token *name_tok = advance(p);
        advance(p); // skip '+='
        ASTNode *val = parse_expr(p);
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t1->line);
        ASTNode *add = ast_binary_op(p->arena, OP_ADD, cur, val, t1->line);
        step = ast_assign(p->arena, name_tok->sval, add, t1->line);
      } else if (strcmp(t2->sval, "-=") == 0) {
        const Token *name_tok = advance(p);
        advance(p); // skip '-='
        ASTNode *val = parse_expr(p);
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t1->line);
        ASTNode *sub = ast_binary_op(p->arena, OP_SUB, cur, val, t1->line);
        step = ast_assign(p->arena, name_tok->sval, sub, t1->line);
      } else if (strcmp(t2->sval, "*=") == 0) {
        const Token *name_tok = advance(p);
        advance(p); // skip '*='
        ASTNode *val = parse_expr(p);
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t1->line);
        ASTNode *mul = ast_binary_op(p->arena, OP_MUL, cur, val, t1->line);
        step = ast_assign(p->arena, name_tok->sval, mul, t1->line);
      } else if (strcmp(t2->sval, "/=") == 0) {
        const Token *name_tok = advance(p);
        advance(p); // skip '/='
        ASTNode *val = parse_expr(p);
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t1->line);
        ASTNode *div = ast_binary_op(p->arena, OP_DIV, cur, val, t1->line);
        step = ast_assign(p->arena, name_tok->sval, div, t1->line);
      } else if (strcmp(t2->sval, "++") == 0) {
        const Token *name_tok = advance(p);
        advance(p); // skip '++'
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t1->line);
        ASTNode *one = ast_int_lit(p->arena, 1, t1->line);
        ASTNode *add = ast_binary_op(p->arena, OP_ADD, cur, one, t1->line);
        step = ast_assign(p->arena, name_tok->sval, add, t1->line);
      } else if (strcmp(t2->sval, "--") == 0) {
        const Token *name_tok = advance(p);
        advance(p); // skip '--'
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t1->line);
        ASTNode *one = ast_int_lit(p->arena, 1, t1->line);
        ASTNode *sub = ast_binary_op(p->arena, OP_SUB, cur, one, t1->line);
        step = ast_assign(p->arena, name_tok->sval, sub, t1->line);
      } else {
        step = parse_expr(p);
      }
    } else {
      step = parse_expr(p);
    }
    match_op(p, ")");

    ASTNode *body = parse_statement(p);

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

  // Chained member / index assignment or expression statement
  if (t->type == TOK_IDENT && !is_reserved_keyword(t->sval)) {
    int start_pos = p->pos;
    const Token *name_tok = advance(p);
    ASTNode *target = ast_ident(p->arena, name_tok->sval, name_tok->line);

    while (1) {
      if (match_op(p, ".")) {
        const Token *mtok = peek(p);
        if (mtok->type != TOK_IDENT) break;
        advance(p);
        const char *member = mtok->sval;

        if (match_op(p, "=")) {
          ASTNode *val = parse_expr(p);
          match_op(p, ";");
          return ast_member_assign(p->arena, target, member, OP_ADD, val, name_tok->line);
        }
        if (match_op(p, "+=")) {
          ASTNode *val = parse_expr(p);
          match_op(p, ";");
          return ast_member_assign(p->arena, target, member, OP_ADD_ASSIGN, val, name_tok->line);
        }
        if (match_op(p, "-=")) {
          ASTNode *val = parse_expr(p);
          match_op(p, ";");
          return ast_member_assign(p->arena, target, member, OP_SUB_ASSIGN, val, name_tok->line);
        }
        if (match_op(p, "++")) {
          match_op(p, ";");
          return ast_member_assign(p->arena, target, member, OP_INC, NULL, name_tok->line);
        }
        if (match_op(p, "--")) {
          match_op(p, ";");
          return ast_member_assign(p->arena, target, member, OP_DEC, NULL, name_tok->line);
        }
        target = ast_member_access(p->arena, target, member, name_tok->line);
      } else if (match_op(p, "[")) {
        ASTNode *idx = parse_expr(p);
        expect_op(p, "]");
        if (match_op(p, "=")) {
          ASTNode *val = parse_expr(p);
          match_op(p, ";");
          return ast_index_assign(p->arena, target, idx, val, name_tok->line);
        }
        if (match_op(p, "+=")) {
          ASTNode *val = parse_expr(p);
          match_op(p, ";");
          ASTNode *cur = ast_index(p->arena, target, idx, name_tok->line);
          ASTNode *add = ast_binary_op(p->arena, OP_ADD, cur, val, name_tok->line);
          return ast_index_assign(p->arena, target, idx, add, name_tok->line);
        }
        if (match_op(p, "-=")) {
          ASTNode *val = parse_expr(p);
          match_op(p, ";");
          ASTNode *cur = ast_index(p->arena, target, idx, name_tok->line);
          ASTNode *sub = ast_binary_op(p->arena, OP_SUB, cur, val, name_tok->line);
          return ast_index_assign(p->arena, target, idx, sub, name_tok->line);
        }
        target = ast_index(p->arena, target, idx, name_tok->line);
      } else if (match_op(p, "=")) {
        ASTNode *val = parse_expr(p);
        match_op(p, ";");
        return ast_assign(p->arena, name_tok->sval, val, t->line);
      } else if (match_op(p, "+=")) {
        ASTNode *val = parse_expr(p);
        match_op(p, ";");
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t->line);
        ASTNode *add = ast_binary_op(p->arena, OP_ADD, cur, val, t->line);
        return ast_assign(p->arena, name_tok->sval, add, t->line);
      } else if (match_op(p, "-=")) {
        ASTNode *val = parse_expr(p);
        match_op(p, ";");
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t->line);
        ASTNode *sub = ast_binary_op(p->arena, OP_SUB, cur, val, t->line);
        return ast_assign(p->arena, name_tok->sval, sub, t->line);
      } else if (match_op(p, "*=")) {
        ASTNode *val = parse_expr(p);
        match_op(p, ";");
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t->line);
        ASTNode *mul = ast_binary_op(p->arena, OP_MUL, cur, val, t->line);
        return ast_assign(p->arena, name_tok->sval, mul, t->line);
      } else if (match_op(p, "/=")) {
        ASTNode *val = parse_expr(p);
        match_op(p, ";");
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t->line);
        ASTNode *div = ast_binary_op(p->arena, OP_DIV, cur, val, t->line);
        return ast_assign(p->arena, name_tok->sval, div, t->line);
      } else if (match_op(p, "++")) {
        match_op(p, ";");
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t->line);
        ASTNode *one = ast_int_lit(p->arena, 1, t->line);
        ASTNode *add = ast_binary_op(p->arena, OP_ADD, cur, one, t->line);
        return ast_assign(p->arena, name_tok->sval, add, t->line);
      } else if (match_op(p, "--")) {
        match_op(p, ";");
        ASTNode *cur = ast_ident(p->arena, name_tok->sval, t->line);
        ASTNode *one = ast_int_lit(p->arena, 1, t->line);
        ASTNode *sub = ast_binary_op(p->arena, OP_SUB, cur, one, t->line);
        return ast_assign(p->arena, name_tok->sval, sub, t->line);
      } else {
        break;
      }
    }
    // Backtrack if not an assignment
    p->pos = start_pos;
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
