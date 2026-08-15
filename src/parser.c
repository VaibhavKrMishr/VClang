#include "../include/parser.h"
#include "../include/lexer.h"

// --- Recursive Descent Parser ---

// Helper to look ahead
char *peek(val *tokens, int i) {
  if (i >= tokens->count)
    return "";
  if (tokens->cell[i]->type != VAL_SYM)
    return "";
  return tokens->cell[i]->sym;
}

// Helper to consume expected token
int expect(val *tokens, int *i, char *sym) {
  if (*i >= tokens->count)
    return 0;
  if (tokens->cell[*i]->type == VAL_SYM &&
      strcmp(tokens->cell[*i]->sym, sym) == 0) {
    (*i)++;
    return 1;
  }
  return 0;
}

// Factors: number, (expr), unary ops
val *val_parse_factor(val *tokens, int *i) {
  if (*i >= tokens->count)
    return val_err("Unexpected end of input");

  val *t = tokens->cell[*i];

  // Unary NOT
  if (t->type == VAL_SYM && strcmp(t->sym, "!") == 0) {
    (*i)++;
    val *op = val_sym("!");
    val *rhs = val_parse_factor(tokens, i);
    return val_node(op->sym, val_sexpr(), rhs); // We'll just evaluate ! rhs
  }

  // Number or String
  if (t->type == VAL_INT || t->type == VAL_FLOAT || t->type == VAL_STR) {
    (*i)++;
    return val_copy(t);
  }

  // Parentheses
  if (expect(tokens, i, "(")) {
    val *e = val_parse_expr(tokens, i);
    if (!expect(tokens, i, ")")) {
      val_del(e);
      return val_err("Expected ')'");
    }
    return e;
  }

  // Variable / Identifier (treated as symbol lookup) or Function Call
  if (t->type == VAL_SYM) {
    (*i)++;
    // Check for Function Call: SYMBOL ( args )
    if (expect(tokens, i, "(")) {
      val *call = val_sexpr();
      val_add(call, val_sym("call"));
      val_add(call, val_copy(t)); // Function name

      val *args = val_sexpr();
      while (*i < tokens->count && !expect(tokens, i, ")")) {
        val_add(args, val_parse_expr(tokens, i));

        // If we hit a ')', we are done with arguments
        if (expect(tokens, i, ")"))
          break;

        // Otherwise, we must have a comma separating the next argument
        if (!expect(tokens, i, ",")) {
          val_del(call);
          val_del(args);
          return val_err("Expected ',' or ')' after argument");
        }
      }
      val_add(call, args);
      return call;
    }
    return val_copy(t);
  }

  return val_err("Unexpected token in factor");
}

// Term: * / %
val *val_parse_term(val *tokens, int *i) {
  val *lhs = val_parse_factor(tokens, i);

  while (1) {
    char *op = peek(tokens, *i);
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
      (*i)++;
      val *rhs = val_parse_factor(tokens, i);
      lhs = val_node(op, lhs, rhs);
    } else {
      break;
    }
  }
  return lhs;
}

// Additive: + -
val *val_parse_add(val *tokens, int *i) {
  val *lhs = val_parse_term(tokens, i);

  while (1) {
    char *op = peek(tokens, *i);
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) {
      (*i)++;
      val *rhs = val_parse_term(tokens, i);
      lhs = val_node(op, lhs, rhs);
    } else {
      break;
    }
  }
  return lhs;
}

// Comparison: < > <= >= == !=
val *val_parse_cmp(val *tokens, int *i) {
  val *lhs = val_parse_add(tokens, i);

  while (1) {
    char *op = peek(tokens, *i);
    if (strchr("=<!>", op[0])) { // Simple check, careful with '=' vs '=='
      if (strcmp(op, "=") == 0)
        break; // Assignment is handled elsewhere if not assignment is distinct
      // Treat =, !=, == etc.
      if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
          strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
          strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
        (*i)++;
        val *rhs = val_parse_add(tokens, i);
        lhs = val_node(op, lhs, rhs);
      } else {
        break;
      }
    } else {
      break;
    }
  }
  return lhs;
}

val *val_parse_logical(val *tokens, int *i) {
  val *lhs = val_parse_cmp(tokens, i);

  while (1) {
    char *op = peek(tokens, *i);
    if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
      (*i)++;
      val *rhs = val_parse_cmp(tokens, i);
      lhs = val_node(op, lhs, rhs);
    } else {
      break;
    }
  }
  return lhs;
}

val *val_parse_expr(val *tokens, int *i) {
  return val_parse_logical(tokens, i);
}

// Block: { stmts }
val *val_parse_block(val *tokens, int *i) {
  if (!expect(tokens, i, "{"))
    return val_err("Expected '{'");

  val *block = val_sexpr();
  val_add(block, val_sym("block"));

  while (*i < tokens->count && !expect(tokens, i, "}")) {
    val_add(block, val_parse_statement(tokens, i));
  }
  return block;
}

// Statements
val *val_parse_statement(val *tokens, int *i) {
  if (*i >= tokens->count)
    return val_err("Unexpected end of input");

  char *t = peek(tokens, *i);

  // Block
  if (strcmp(t, "{") == 0) {
    return val_parse_block(tokens, i);
  }

  // Variable Declaration: int/float/string x = 10;
  if (strcmp(t, "int") == 0 || strcmp(t, "float") == 0 ||
      strcmp(t, "string") == 0) {
    (*i)++;
    val *name = val_copy(tokens->cell[*i]);
    (*i)++; // Expect identifier name
    if (!expect(tokens, i, "=")) {
      val_del(name);
      return val_err("Expected '=' in declaration");
    }
    val *v_val = val_parse_expr(tokens, i);
    if (!expect(tokens, i, ";")) {
      val_del(name);
      val_del(v_val);
      return val_err("Expected ';'");
    }

    val *decl = val_sexpr();
    val_add(decl, val_sym("decl"));
    val_add(decl, name);
    val_add(decl, v_val);
    return decl;
  }

  // If Statement: if (cond) stmt else stmt
  if (strcmp(t, "if") == 0) {
    (*i)++;
    if (!expect(tokens, i, "("))
      return val_err("Expected '(' after if");
    val *cond = val_parse_expr(tokens, i);
    if (!expect(tokens, i, ")")) {
      val_del(cond);
      return val_err("Expected ')'");
    }
    val *then_body = val_parse_statement(tokens, i);

    val *node = val_sexpr();
    val_add(node, val_sym("if"));
    val_add(node, cond);
    val_add(node, then_body);

    if (strcmp(peek(tokens, *i), "else") == 0) {
      (*i)++;
      val *else_body = val_parse_statement(tokens, i);
      val_add(node, else_body);
    } else {
      val_add(node, val_sexpr()); // Empty else
    }
    return node;
  }

  // While Loop: while (cond) stmt;
  if (strcmp(t, "while") == 0) {
    (*i)++;
    if (!expect(tokens, i, "("))
      return val_err("Expected '(' after while");
    val *cond = val_parse_expr(tokens, i);
    if (!expect(tokens, i, ")")) {
      val_del(cond);
      return val_err("Expected ')'");
    }
    val *body = val_parse_statement(tokens, i);

    val *node = val_sexpr();
    val_add(node, val_sym("while"));
    val_add(node, cond);
    val_add(node, body);
    return node;
  }

  // For Loop: for (init; cond; step) body -> (block init (while cond (block
  // body step)))
  if (strcmp(t, "for") == 0) {
    (*i)++;
    if (!expect(tokens, i, "("))
      return val_err("Expected '(' after for");

    // Init (statement or decl)
    val *init = val_parse_statement(tokens, i);
    // Note: val_parse_statement consumes the semicolon usually

    // Condition
    val *cond = val_parse_expr(tokens, i);
    if (!expect(tokens, i, ";")) {
      val_del(init);
      val_del(cond);
      return val_err("Expected ';' after loop condition");
    }

    // Step (expression usually, but we need it as a statement to run it)
    // We parse carefully. Usually i=i+1 is an assignment, which is a statement
    // if followed by ; But inside 'for', the step does NOT have a trailing ; So
    // we need to parse it as an expression or assignment-expression

    val *step;
    // Check for assignment: IDENT = ...
    if (tokens->cell[*i]->type == VAL_SYM && (*i + 1 < tokens->count) &&
        tokens->cell[*i + 1]->type == VAL_SYM &&
        strcmp(tokens->cell[*i + 1]->sym, "=") == 0) {
      val *name = val_copy(tokens->cell[*i]);
      (*i) += 2;
      val *v_val = val_parse_expr(tokens, i);
      step = val_sexpr();
      val_add(step, val_sym("assign"));
      val_add(step, name);
      val_add(step, v_val);
    } else {
      step = val_parse_expr(tokens, i);
    }

    if (!expect(tokens, i, ")"))
      return val_err("Expected ')' after for loop");

    val *body = val_parse_block(tokens, i);

    // Construct: (block init (while cond (for_body body step)))
    val *fbody = val_sexpr();
    val_add(fbody, val_sym("for_body"));
    val_add(fbody, body);
    val_add(fbody, step);

    val *while_node = val_sexpr();
    val_add(while_node, val_sym("while"));
    val_add(while_node, cond);
    val_add(while_node, fbody);

    val *outer = val_sexpr();
    val_add(outer, val_sym("block"));
    val_add(outer, init);
    val_add(outer, while_node);

    return outer;
  }

  // Return Statement
  if (strcmp(t, "return") == 0) {
    (*i)++;
    val *expr = val_parse_expr(tokens, i);
    if (!expect(tokens, i, ";")) {
      val_del(expr);
      return val_err("Expected ';'");
    }
    val *node = val_sexpr();
    val_add(node, val_sym("return"));
    val_add(node, expr);
    return node;
  }

  // Break Statement
  if (strcmp(t, "break") == 0) {
    (*i)++;
    if (!expect(tokens, i, ";"))
      return val_err("Expected ';'");
    return val_break();
  }

  // Continue Statement
  if (strcmp(t, "continue") == 0) {
    (*i)++;
    if (!expect(tokens, i, ";"))
      return val_err("Expected ';'");
    return val_continue();
  }

  // Expression Statement or Assignment
  // Try to parse expression
  // Check for assignment: x = 10;
  // Hacky lookahead for assignment: IDENT = ...
  if (tokens->cell[*i]->type == VAL_SYM && (*i + 1 < tokens->count) &&
      tokens->cell[*i + 1]->type == VAL_SYM &&
      strcmp(tokens->cell[*i + 1]->sym, "=") == 0) {
    val *name = val_copy(tokens->cell[*i]);
    (*i) += 2; // Skip name and =
    val *v_val = val_parse_expr(tokens, i);
    if (!expect(tokens, i, ";")) {
      val_del(name);
      val_del(v_val);
      return val_err("Expected ';'");
    }

    val *node = val_sexpr();
    val_add(node, val_sym("assign"));
    val_add(node, name);
    val_add(node, v_val);
    return node;
  }

  val *expr = val_parse_expr(tokens, i);
  if (!expect(tokens, i, ";")) {
    val_del(expr);
    return val_err("Expected ';'");
  }
  return expr;
}

val *val_parse(val *tokens) {
  int i = 0;
  val *ast = val_sexpr();
  val_add(ast, val_sym("program"));

  while (i < tokens->count) {
    val_add(ast, val_parse_statement(tokens, &i));
  }
  return ast;
}

val *val_read(char *s) {
  val *tokens = val_tokenize(s);
  val *ast = val_parse(tokens);
  val_del(tokens);
  return ast;
}
