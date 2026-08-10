#define _POSIX_C_SOURCE 200809L
#include "vclang.h"
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>


// --- Constructors ---

val* val_num(long x) {
    val* v = malloc(sizeof(val));
    v->type = VAL_INT;
    v->num = x;
    return v;
}

val* val_float(double x) {
    val* v = malloc(sizeof(val));
    v->type = VAL_FLOAT;
    v->dec = x;
    return v;
}

val* val_str(char* s) {
    val* v = malloc(sizeof(val));
    v->type = VAL_STR;
    v->str = malloc(strlen(s) + 1);
    strcpy(v->str, s);
    return v;
}

val* val_break(void) {
    val* v = malloc(sizeof(val));
    v->type = VAL_BREAK;
    return v;
}

val* val_continue(void) {
    val* v = malloc(sizeof(val));
    v->type = VAL_CONTINUE;
    return v;
}

val* val_err(char* m) {
    val* v = malloc(sizeof(val));
    v->type = VAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

val* val_sym(char* s) {
    val* v = malloc(sizeof(val));
    v->type = VAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

val* val_sexpr(void) {
    val* v = malloc(sizeof(val));
    v->type = VAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

val* val_fun(lbuiltin func) {
    val* v = malloc(sizeof(val));
    v->type = VAL_FUN;
    v->fun = func;
    return v;
}

val* val_qexpr(void) {
    val* v = malloc(sizeof(val));
    v->type = VAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

// --- Destructor ---

void val_del(val* v) {
    switch (v->type) {
        case VAL_INT: break;
        case VAL_FLOAT: break;
        case VAL_BREAK: break;
        case VAL_CONTINUE: break;
        case VAL_FUN: break;
        case VAL_ERR: free(v->err); break;
        case VAL_SYM: free(v->sym); break;
        case VAL_STR: free(v->str); break;
        case VAL_QEXPR:
        case VAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                val_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }
    free(v);
}

// --- AST Manipulation ---

val* val_add(val* v, val* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(val*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

val* val_pop(val* v, int i) {
    val* x = v->cell[i];
    memmove(&v->cell[i], &v->cell[i+1],
        sizeof(val*) * (v->count-i-1));
    v->count--;
    v->cell = realloc(v->cell, sizeof(val*) * v->count);
    return x;
}

val* val_take(val* v, int i) {
    val* x = val_pop(v, i);
    val_del(v);
    return x;
}

val* val_copy(val* v) {
    val* x = malloc(sizeof(val));
    x->type = v->type;
    
    switch (v->type) {
        case VAL_INT: x->num = v->num; break;
        case VAL_FLOAT: x->dec = v->dec; break;
        case VAL_BREAK: break;
        case VAL_CONTINUE: break;
        case VAL_FUN: x->fun = v->fun; break;
        case VAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case VAL_STR:
            x->str = malloc(strlen(v->str) + 1);
            strcpy(x->str, v->str);
            break;
        case VAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;
        case VAL_SEXPR:
        case VAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(val*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = val_copy(v->cell[i]);
            }
            break;
    }
    return x;
}

// --- Printing ---

void val_expr_print(val* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        val_print(v->cell[i]);
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void val_print(val* v) {
    switch (v->type) {
        case VAL_INT: printf("%li", v->num); break;
        case VAL_FLOAT: printf("%lf", v->dec); break;
        case VAL_BREAK: printf("break"); break;
        case VAL_CONTINUE: printf("continue"); break;
        case VAL_STR: printf("%s", v->str); break;
        case VAL_ERR: printf("Error: %s", v->err); break;
        case VAL_SYM: printf("%s", v->sym); break;
        case VAL_FUN: printf("<function>"); break;
        case VAL_SEXPR: val_expr_print(v, '(', ')'); break; // Re-adding these for AST printing debug
        case VAL_QEXPR: val_expr_print(v, '{', '}'); break;
    }
}

void val_println(val* v) { val_print(v); putchar('\n'); fflush(stdout); }

// --- AST Construction ---

/*
  The parser will produce an AST.
  For now, we will reuse VAL_SEXPR as a general "node" in the AST.
  The first element of the cell can be a VAL_SYM indicating the node type or operator.
  
  e.g. 1 + 2 -> ( "+" 1 2 )
  int x = 10; -> ( "decl" "int" "x" 10 )
  if (x) { ... } -> ( "if" cond block )
  while (x) { ... } -> ( "while" cond block )
  block -> ( "block" st1 st2 ... )
*/

// val_op removed (unused)

val* val_node(char* op, val* x, val* y) {
    val* v = val_sexpr();
    val_add(v, val_sym(op));
    val_add(v, x);
    val_add(v, y);
    return v;
}

// --- Tokenizer ---

// Reads a number (int or float)
val* val_read_num(char* s, int* i) {
    char* end;
    long x = strtol(s + *i, &end, 10);
    // Check for decimal point
    if (*end == '.') {
        double d = strtod(s + *i, &end);
        *i = end - s;
        return val_float(d);
    }
    *i = end - s;
    return val_num(x);
}

// Reads a symbol or keyword or identifier
// Reads a symbol or keyword or identifier
val* val_read_sym(char* s, int* i) {
    char* part = calloc(1, 1);
    // Allow alphanumeric and underscore for identifiers
    while ((isalnum(s[*i]) || s[*i] == '_') && s[*i] != '\0') {
        part = realloc(part, strlen(part) + 2);
        part[strlen(part)+1] = '\0';
        part[strlen(part)] = s[*i];
        (*i)++;
    }
    val* v = val_sym(part);
    free(part);
    return v;
}

// Reads a string enclosed in delim
val* val_read_str(char* s, int* i, char delim) {
    char* part = calloc(1, 1);
    (*i)++; // Skip opening quote
    while (s[*i] != delim && s[*i] != '\0') {
         char c = s[*i];
         if (c == '\\') {
             (*i)++;
             if (s[*i] == 'n') c = '\n';
             else if (s[*i] == 't') c = '\t';
             else if (s[*i] == 'r') c = '\r';
             else if (s[*i] == '\\') c = '\\';
             else if (s[*i] == '"') c = '"';
             else if (s[*i] == '\'') c = '\'';
             else c = s[*i]; // Literal escape
         }
         
         part = realloc(part, strlen(part) + 2);
         part[strlen(part)+1] = '\0';
         part[strlen(part)] = c;
         (*i)++;
    }
    if (s[*i] == delim) (*i)++; // Skip closing quote
    val* v = val_str(part);
    free(part);
    return v;
}

// Reads an operator or punctuation
val* val_read_op(char* s, int* i) {
    char* part = calloc(1, 1);
    char c = s[*i];
    
    // Single char punctuation
    if (strchr("(){};,", c)) {
        part = realloc(part, 2);
        part[0] = c; part[1] = '\0';
        (*i)++;
        val* v = val_sym(part);
        free(part);
        return v;
    }
    
    // Multi-char operators like ==, !=, >=, <=
    while (strchr("+-*/%=<>!&|", s[*i]) && s[*i] != '\0') {
         part = realloc(part, strlen(part) + 2);
         part[strlen(part)+1] = '\0';
         part[strlen(part)] = s[*i];
         (*i)++;
    }
    
    val* v = val_sym(part);
    free(part);
    return v;
}

val* val_tokenize(char* s) {
    val* tokens = val_sexpr(); // Use sexpr container for list of tokens
    int i = 0;
    while (s[i] != '\0') {
        if (isspace(s[i])) {
            i++;
            continue;
        }
        
        if (s[i] == '/' && s[i+1] == '/') {
            i += 2;
            while (s[i] != '\0') {
                if (s[i] == '/' && s[i+1] == '/') {
                    i += 2;
                    break;
                }
                i++;
            }
            continue;
        }
        
        if (isdigit(s[i])) {
            val_add(tokens, val_read_num(s, &i));
            continue;
        }
        
        if (isalpha(s[i]) || s[i] == '_') {
            val_add(tokens, val_read_sym(s, &i));
            continue;
        }
        
        if (strchr("(){};,+-*/%=<>!&|", s[i])) {
            val_add(tokens, val_read_op(s, &i));
            continue;
        }
        
        if (s[i] == '"' || s[i] == '\'') {
            val_add(tokens, val_read_str(s, &i, s[i]));
            continue;
        }
        
        i++; // Unknown?
    }
    return tokens;
}

// --- Recursive Descent Parser ---

// Forward declarations
val* val_parse_expr(val* tokens, int* i);
val* val_parse_statement(val* tokens, int* i);
val* val_parse_block(val* tokens, int* i);

// Helper to look ahead
char* peek(val* tokens, int i) {
    if (i >= tokens->count) return "";
    if (tokens->cell[i]->type != VAL_SYM) return "";
    return tokens->cell[i]->sym;
}

// Helper to consume expected token
int expect(val* tokens, int* i, char* sym) {
    if (*i >= tokens->count) return 0;
    if (tokens->cell[*i]->type == VAL_SYM && strcmp(tokens->cell[*i]->sym, sym) == 0) {
        (*i)++;
        return 1;
    }
    return 0;
}

// Factors: number, (expr), unary ops
val* val_parse_factor(val* tokens, int* i) {
    if (*i >= tokens->count) return val_err("Unexpected end of input");
    
    val* t = tokens->cell[*i];
    
    // Number or String
    if (t->type == VAL_INT || t->type == VAL_FLOAT || t->type == VAL_STR) {
        (*i)++;
        return val_copy(t);
    }
    
    // Parentheses
    if (expect(tokens, i, "(")) {
        val* e = val_parse_expr(tokens, i);
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
            val* call = val_sexpr();
            val_add(call, val_sym("call"));
            val_add(call, val_copy(t)); // Function name
            
            val* args = val_sexpr();
            while (*i < tokens->count && !expect(tokens, i, ")")) {
                 val_add(args, val_parse_expr(tokens, i));
                 if (expect(tokens, i, ",")) continue; // Optional comma
            }
            val_add(call, args);
            return call;
        }
        return val_copy(t);
    }
    
    return val_err("Unexpected token in factor");
}

// Term: * / %
val* val_parse_term(val* tokens, int* i) {
    val* lhs = val_parse_factor(tokens, i);
    
    while (1) {
        char* op = peek(tokens, *i);
        if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
            (*i)++;
            val* rhs = val_parse_factor(tokens, i);
            lhs = val_node(op, lhs, rhs);
        } else {
            break;
        }
    }
    return lhs;
}

// Additive: + -
val* val_parse_add(val* tokens, int* i) {
    val* lhs = val_parse_term(tokens, i);
    
    while (1) {
        char* op = peek(tokens, *i);
        if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) {
            (*i)++;
            val* rhs = val_parse_term(tokens, i);
            lhs = val_node(op, lhs, rhs);
        } else {
            break;
        }
    }
    return lhs;
}

// Comparison: < > <= >= == !=
val* val_parse_cmp(val* tokens, int* i) {
    val* lhs = val_parse_add(tokens, i);
    
    while (1) {
        char* op = peek(tokens, *i);
        if (strchr("=<!>", op[0])) { // Simple check, careful with '=' vs '=='
             if (strcmp(op, "=") == 0) break; // Assignment is handled elsewhere if not assignment is distinct
             // Treat =, !=, == etc.
             if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || 
                 strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
                 strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
                 (*i)++;
                 val* rhs = val_parse_add(tokens, i);
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

val* val_parse_expr(val* tokens, int* i) {
    return val_parse_cmp(tokens, i);
}

// Block: { stmts }
val* val_parse_block(val* tokens, int* i) {
    if (!expect(tokens, i, "{")) return val_err("Expected '{'");
    
    val* block = val_sexpr();
    val_add(block, val_sym("block"));
    
    while (*i < tokens->count && !expect(tokens, i, "}")) {
        val_add(block, val_parse_statement(tokens, i));
    }
    return block;
}

// Statements
val* val_parse_statement(val* tokens, int* i) {
     if (*i >= tokens->count) return val_err("Unexpected end of input");
     
     char* t = peek(tokens, *i);
     
     // Block
     if (strcmp(t, "{") == 0) {
         return val_parse_block(tokens, i);
     }
     
     // Variable Declaration: int/float/string x = 10;
     if (strcmp(t, "int") == 0 || strcmp(t, "float") == 0 || strcmp(t, "string") == 0) {
         (*i)++;
         val* name = val_copy(tokens->cell[*i]); (*i)++; // Expect identifier name
         if (!expect(tokens, i, "=")) { val_del(name); return val_err("Expected '=' in declaration"); }
         val* v_val = val_parse_expr(tokens, i);
         if (!expect(tokens, i, ";")) { val_del(name); val_del(v_val); return val_err("Expected ';'"); }
         
         val* decl = val_sexpr();
         val_add(decl, val_sym("decl"));
         val_add(decl, name);
         val_add(decl, v_val);
         return decl;
     }
     
     // If Statement: if (cond) stmt else stmt
     if (strcmp(t, "if") == 0) {
         (*i)++;
         if (!expect(tokens, i, "(")) return val_err("Expected '(' after if");
         val* cond = val_parse_expr(tokens, i);
         if (!expect(tokens, i, ")")) { val_del(cond); return val_err("Expected ')'"); }
         val* then_body = val_parse_statement(tokens, i);
         
         val* node = val_sexpr();
         val_add(node, val_sym("if"));
         val_add(node, cond);
         val_add(node, then_body);
         
         if (strcmp(peek(tokens, *i), "else") == 0) {
             (*i)++;
             val* else_body = val_parse_statement(tokens, i);
             val_add(node, else_body);
         } else {
             val_add(node, val_sexpr()); // Empty else
         }
         return node;
     }

     // While Loop: while (cond) stmt;
     if (strcmp(t, "while") == 0) {
         (*i)++;
         if (!expect(tokens, i, "(")) return val_err("Expected '(' after while");
         val* cond = val_parse_expr(tokens, i);
         if (!expect(tokens, i, ")")) { val_del(cond); return val_err("Expected ')'"); }
         val* body = val_parse_statement(tokens, i);
         
         val* node = val_sexpr();
         val_add(node, val_sym("while"));
         val_add(node, cond);
         val_add(node, body);
         return node;
     }
     
     
     
     // For Loop: for (init; cond; step) body -> (block init (while cond (block body step)))
     if (strcmp(t, "for") == 0) {
         (*i)++;
         if (!expect(tokens, i, "(")) return val_err("Expected '(' after for");
         
         // Init (statement or decl)
         val* init = val_parse_statement(tokens, i);
         // Note: val_parse_statement consumes the semicolon usually
         
         // Condition
         val* cond = val_parse_expr(tokens, i);
         if (!expect(tokens, i, ";")) { val_del(init); val_del(cond); return val_err("Expected ';' after loop condition"); }
         
         // Step (expression usually, but we need it as a statement to run it)
         // We parse carefully. Usually i=i+1 is an assignment, which is a statement if followed by ;
         // But inside 'for', the step does NOT have a trailing ;
         // So we need to parse it as an expression or assignment-expression
         
         val* step;
         // Check for assignment: IDENT = ...
         if (tokens->cell[*i]->type == VAL_SYM && (*i+1 < tokens->count) && 
             tokens->cell[*i+1]->type == VAL_SYM && strcmp(tokens->cell[*i+1]->sym, "=") == 0) {
              val* name = val_copy(tokens->cell[*i]);
              (*i) += 2; 
              val* v_val = val_parse_expr(tokens, i);
              step = val_sexpr();
              val_add(step, val_sym("assign"));
              val_add(step, name);
              val_add(step, v_val);
         } else {
              step = val_parse_expr(tokens, i);
         }
         
         if (!expect(tokens, i, ")")) return val_err("Expected ')' after for loop");
         
         val* body = val_parse_block(tokens, i);
         
         // Construct: (block init (while cond (for_body body step)))
         val* fbody = val_sexpr();
         val_add(fbody, val_sym("for_body"));
         val_add(fbody, body);
         val_add(fbody, step);

         val* while_node = val_sexpr();
         val_add(while_node, val_sym("while"));
         val_add(while_node, cond);
         val_add(while_node, fbody);
         
         val* outer = val_sexpr();
         val_add(outer, val_sym("block"));
         val_add(outer, init);
         val_add(outer, while_node);
         
         return outer;
     }

     // Return Statement
     if (strcmp(t, "return") == 0) {
         (*i)++;
         val* expr = val_parse_expr(tokens, i);
         if (!expect(tokens, i, ";")) { val_del(expr); return val_err("Expected ';'"); }
         val* node = val_sexpr();
         val_add(node, val_sym("return"));
         val_add(node, expr);
         return node;
     }
     
     // Break Statement
     if (strcmp(t, "break") == 0) {
         (*i)++;
         if (!expect(tokens, i, ";")) return val_err("Expected ';'");
         return val_break();
     }
     
     // Continue Statement
     if (strcmp(t, "continue") == 0) {
         (*i)++;
         if (!expect(tokens, i, ";")) return val_err("Expected ';'");
         return val_continue();
     }

     // Expression Statement or Assignment
     // Try to parse expression
     // Check for assignment: x = 10;
     // Hacky lookahead for assignment: IDENT = ...
     if (tokens->cell[*i]->type == VAL_SYM && (*i+1 < tokens->count) && 
         tokens->cell[*i+1]->type == VAL_SYM && strcmp(tokens->cell[*i+1]->sym, "=") == 0) {
         val* name = val_copy(tokens->cell[*i]);
         (*i) += 2; // Skip name and =
         val* v_val = val_parse_expr(tokens, i);
         if (!expect(tokens, i, ";")) { val_del(name); val_del(v_val); return val_err("Expected ';'"); }
         
         val* node = val_sexpr();
         val_add(node, val_sym("assign"));
         val_add(node, name);
         val_add(node, v_val);
         return node;
     }
     
     val* expr = val_parse_expr(tokens, i);
     if (!expect(tokens, i, ";")) { val_del(expr); return val_err("Expected ';'"); }
     return expr;
}

val* val_parse(val* tokens) {
    int i = 0;
    val* ast = val_sexpr();
    val_add(ast, val_sym("program"));
    
    while (i < tokens->count) {
        val_add(ast, val_parse_statement(tokens, &i));
    }
    return ast;
}

val* val_read(char* s) {
    val* tokens = val_tokenize(s);
    val* ast = val_parse(tokens);
    val_del(tokens);
    return ast;
}

// --- Environment ---

// --- Environment ---

struct lenv {
    int count;
    char** syms;
    val** vals;
};

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        val_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

val* lenv_get(lenv* e, val* k) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return val_copy(e->vals[i]);
        }
    }
    return val_err("Unbound Symbol!");
}

void lenv_put(lenv* e, val* k, val* v) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            val_del(e->vals[i]);
            e->vals[i] = val_copy(v);
            return;
        }
    }
    
    e->count++;
    e->vals = realloc(e->vals, sizeof(val*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    
    e->vals[e->count-1] = val_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count-1], k->sym);
}

// --- Builtins Replaced by Direct Eval Logic ---

long val_eval_int(lenv* e, val* v) {
    val* r = val_eval(e, v);
    if (r->type != VAL_INT) {
        printf("Runtime Error: Expected integer, got %d\n", r->type);
        val_del(r);
        return 0; // Error handling usually returns val* but for simple extraction we return 0
    }
    long n = r->num;
    val_del(r);
    return n;
}


val* val_eval(lenv* e, val* v) {
    // printf("Eval Type: %d ", v->type); if(v->type==VAL_SYM) printf("Sym: %s", v->sym); printf("\n");
    if (v->type == VAL_SYM) {
        val* x = lenv_get(e, v);
        val_del(v);
        return x;
    }
    if (v->type == VAL_BREAK || v->type == VAL_CONTINUE) {
        return v;
    }
    
    if (v->type == VAL_SEXPR) {
        if (v->count == 0) return v;
        
        // Check for specific forms based on first symbol
        val* f = v->cell[0];
        if (f->type == VAL_SYM) {
             char* op = f->sym;
             
             // Arithmetic / Logic
             if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0 ||
                 strcmp(op, "%") == 0 || strcmp(op, ">") == 0 || strcmp(op, "<") == 0 || strcmp(op, "==") == 0 ||
                 strcmp(op, "!=") == 0 || strcmp(op, ">=") == 0 || strcmp(op, "<=") == 0) {
                 
                 val* f = val_pop(v, 0); // Pop operator to keep it alive
                 
                 if (v->count != 2) {
                     val_del(v); val_del(f);
                     return val_err("Operator expects 2 arguments");
                 }
                 
                 val* arg1 = v->cell[0];
                 val* arg2 = v->cell[1];
                 
                 // If either is float, result is float
                  // Evaluate args
                  val* val1 = val_eval(e, val_copy(arg1));
                  val* val2 = val_eval(e, val_copy(arg2));
                  
                  val* result = NULL;
                  char* s = f->sym;
                  
                  // String concatenation
                  if (strcmp(s, "+") == 0 && (val1->type == VAL_STR || val2->type == VAL_STR)) {
                      char* s1 = (val1->type == VAL_STR) ? val1->str : NULL;
                      char* s2 = (val2->type == VAL_STR) ? val2->str : NULL;
                      
                      char buf1[128], buf2[128];
                      if (!s1) {
                          if (val1->type == VAL_INT) sprintf(buf1, "%li", val1->num);
                          else if (val1->type == VAL_FLOAT) sprintf(buf1, "%g", val1->dec);
                          else sprintf(buf1, "?");
                          s1 = buf1;
                      }
                      if (!s2) {
                          if (val2->type == VAL_INT) sprintf(buf2, "%li", val2->num);
                          else if (val2->type == VAL_FLOAT) sprintf(buf2, "%g", val2->dec);
                          else sprintf(buf2, "?");
                          s2 = buf2;
                      }
                      
                      char* combined = malloc(strlen(s1) + strlen(s2) + 1);
                      strcpy(combined, s1);
                      strcat(combined, s2);
                      result = val_str(combined);
                      free(combined);
                      
                      val_del(val1); val_del(val2);
                      val_del(v); val_del(f);
                      return result;
                  }
                  
                  // Numeric arithmetic
                  if (val1->type == VAL_STR || val2->type == VAL_STR) {
                      val_del(val1); val_del(val2); val_del(v); val_del(f);
                      return val_err("Invalid types for operator");
                  }

                  int is_float = (val1->type == VAL_FLOAT || val2->type == VAL_FLOAT);
                  double x = (val1->type == VAL_FLOAT) ? val1->dec : (double)val1->num;
                  double y = (val2->type == VAL_FLOAT) ? val2->dec : (double)val2->num;
                  
                  val_del(val1); val_del(val2);
                  val_del(v);
                 
                 // val* result; // Already declared above
                 // char* s = f->sym; // Already declared above
                 
                 if (is_float) {
                     // Float arithmetic
                     if (strcmp(s, "+") == 0) result = val_float(x + y);
                     else if (strcmp(s, "-") == 0) result = val_float(x - y);
                     else if (strcmp(s, "*") == 0) result = val_float(x * y);
                     else if (strcmp(s, "/") == 0) {
                        if (y == 0.0) result = val_err("Div zero");
                        else result = val_float(x / y);
                     }
                     else if (strcmp(s, ">") == 0) result = val_num(x > y);
                     else if (strcmp(s, "<") == 0) result = val_num(x < y);
                     else if (strcmp(s, "==") == 0) result = val_num(x == y);
                     else if (strcmp(s, "!=") == 0) result = val_num(x != y);
                     else if (strcmp(s, ">=") == 0) result = val_num(x >= y);
                     else if (strcmp(s, "<=") == 0) result = val_num(x <= y);
                     else result = val_err("Bad float op");
                 } else {
                     // Integer arithmetic (preserves existing behavior)
                     long ix = (long)x;
                     long iy = (long)y;
                     
                     if (strcmp(s, "+") == 0) result = val_num(ix + iy);
                     else if (strcmp(s, "-") == 0) result = val_num(ix - iy);
                     else if (strcmp(s, "*") == 0) result = val_num(ix * iy);
                     else if (strcmp(s, "/") == 0) {
                        if (iy == 0) result = val_err("Div zero");
                        else result = val_num(ix / iy);
                     }
                     else if (strcmp(s, "%") == 0) {
                        if (iy == 0) result = val_err("Div zero");
                        else result = val_num(ix % iy);
                     }
                     else if (strcmp(s, ">") == 0) result = val_num(ix > iy);
                     else if (strcmp(s, "<") == 0) result = val_num(ix < iy);
                     else if (strcmp(s, "==") == 0) result = val_num(ix == iy);
                     else if (strcmp(s, "!=") == 0) result = val_num(ix != iy);
                     else if (strcmp(s, ">=") == 0) result = val_num(ix >= iy);
                     else if (strcmp(s, "<=") == 0) result = val_num(ix <= iy);
                     else result = val_err("Bad int op");
                 }
                 
                 val_del(f);
                 return result;
             }
             
              // Call: (call fname args)
              if (strcmp(op, "call") == 0) {
                  val* fname = v->cell[1];
                  val* args = v->cell[2];
                  
                  // Builtin Print/Println
                  if (strcmp(fname->sym, "print") == 0 || strcmp(fname->sym, "println") == 0) {
                      if (strcmp(fname->sym, "println") == 0) putchar('\n');
                      for (int i = 0; i < args->count; i++) {
                          val* v_val = val_eval(e, val_copy(args->cell[i]));
                          if (v_val->type == VAL_STR) {
                              printf("%s", v_val->str);
                          } else {
                              val_print(v_val);
                          }
                          val_del(v_val);
                          if (i != args->count - 1) putchar(' ');
                      }
                      val_del(v);
                      return val_num(0);
                   }
                   
                   // Builtin Type Conversions
                   if (strcmp(fname->sym, "int") == 0) {
                       if (args->count != 1) { val_del(v); return val_err("int() takes 1 argument"); }
                       val* v_val = val_eval(e, val_copy(args->cell[0]));
                       val* res;
                       if (v_val->type == VAL_INT) res = val_copy(v_val);
                       else if (v_val->type == VAL_FLOAT) res = val_num((long)v_val->dec);
                       else if (v_val->type == VAL_STR) {
                           if (strlen(v_val->str) == 1 && !isdigit(v_val->str[0])) {
                                res = val_num((long)v_val->str[0]);
                           } else {
                                res = val_num(strtol(v_val->str, NULL, 10));
                           }
                       }
                       else res = val_err("Cannot convert to int");
                       val_del(v_val); val_del(v);
                       return res;
                   }
                   
                   if (strcmp(fname->sym, "float") == 0) {
                       if (args->count != 1) { val_del(v); return val_err("float() takes 1 argument"); }
                       val* v_val = val_eval(e, val_copy(args->cell[0]));
                       val* res;
                       if (v_val->type == VAL_FLOAT) res = val_copy(v_val);
                       else if (v_val->type == VAL_INT) res = val_float((double)v_val->num);
                       else if (v_val->type == VAL_STR) res = val_float(strtod(v_val->str, NULL));
                       else res = val_err("Cannot convert to float");
                       val_del(v_val); val_del(v);
                       return res;
                   }
                   
                   if (strcmp(fname->sym, "string") == 0) {
                       if (args->count != 1) { val_del(v); return val_err("string() takes 1 argument"); }
                       val* v_val = val_eval(e, val_copy(args->cell[0]));
                       char buf[128];
                       if (v_val->type == VAL_INT) sprintf(buf, "%li", v_val->num);
                       else if (v_val->type == VAL_FLOAT) sprintf(buf, "%g", v_val->dec);
                       else if (v_val->type == VAL_STR) { val_del(v); return v_val; }
                       else { val_del(v_val); val_del(v); return val_err("Cannot convert to string"); }
                       val* res = val_str(buf);
                       val_del(v_val); val_del(v);
                       return res;
                   }
                   
                   if (strcmp(fname->sym, "char") == 0) {
                       if (args->count != 1) { val_del(v); return val_err("char() takes 1 argument"); }
                       val* v_val = val_eval(e, val_copy(args->cell[0]));
                       if (v_val->type != VAL_INT) { val_del(v_val); val_del(v); return val_err("char() takes an int ASCII value"); }
                       char buf[2] = {(char)v_val->num, '\0'};
                       val* res = val_str(buf);
                       val_del(v_val); val_del(v);
                       return res;
                   }

                   if (strcmp(fname->sym, "input") == 0) {
                        if (args->count > 1) { val_del(v); return val_err("input() takes 0 or 1 argument"); }
                        if (args->count == 1) {
                            val* prompt = val_eval(e, val_copy(args->cell[0]));
                            if (prompt->type == VAL_STR) printf("%s", prompt->str);
                            else val_print(prompt);
                            val_del(prompt);
                        }
                        
                        char buf[2048];
                        if (!fgets(buf, 2048, stdin)) {
                            val_del(v);
                            return val_str("");
                        }
                        buf[strcspn(buf, "\n")] = '\0';
                        val* res = val_str(buf);
                        val_del(v);
                        return res;
                    }

                    if (strcmp(fname->sym, "len") == 0) {
                        if (args->count != 1) { val_del(v); return val_err("len() takes 1 argument"); }
                        val* v_val = val_eval(e, val_copy(args->cell[0]));
                        if (v_val->type != VAL_STR) { val_del(v_val); val_del(v); return val_err("len() expects a string"); }
                        val* res = val_num((long)strlen(v_val->str));
                        val_del(v_val); val_del(v);
                        return res;
                    }

                    val_del(v);
                    return val_err("Function not defined");
              }

              // Internal for_body: (for_body body step)
              if (strcmp(op, "for_body") == 0) {
                  val* body = v->cell[1];
                  val* step = v->cell[2];
                  val* res = val_eval(e, val_copy(body));
                  if (res->type == VAL_BREAK) {
                      val_del(res); val_del(v);
                      return val_num(0); // Exit while loop
                  }
                  // Even if continue, we run the step
                  val_del(res);
                  val* sres = val_eval(e, val_copy(step));
                  val_del(sres);
                  val_del(v);
                  return val_num(0);
              }

             // Block: (block st1 st2 ...)
             if (strcmp(op, "block") == 0) {
                 val* res = val_num(0);
                 for (int i = 1; i < v->count; i++) {
                     val_del(res);
                     res = val_eval(e, val_copy(v->cell[i]));
                     if (res->type == VAL_BREAK || res->type == VAL_CONTINUE || res->type == VAL_ERR) {
                         val_del(v);
                         return res;
                     }
                 }
                 val_del(v);
                 return res;
             }

             // Declaration: (decl name val)
             if (strcmp(op, "decl") == 0) {
                 val* name = v->cell[1];
                 val* v_val = val_eval(e, val_copy(v->cell[2]));
                 lenv_put(e, name, v_val);
                 val_del(v);
                 return v_val;
             }

             // Assignment: (assign name val)
             if (strcmp(op, "assign") == 0) {
                 val* name = v->cell[1];
                 val* v_val = val_eval(e, val_copy(v->cell[2]));
                 lenv_put(e, name, v_val);
                 val_del(v);
                 return v_val;
             }

             // While: (while cond body)
             if (strcmp(op, "while") == 0) {
                 val* res = val_num(0);
                 while (1) {
                     val* cond = val_eval(e, val_copy(v->cell[1]));
                     int c = (cond->type == VAL_INT && cond->num != 0) || (cond->type == VAL_FLOAT && cond->dec != 0.0);
                     val_del(cond);
                     if (!c) break;
                     
                     val_del(res);
                     res = val_eval(e, val_copy(v->cell[2]));
                     if (res->type == VAL_BREAK) { val_del(res); res = val_num(0); break; }
                     if (res->type == VAL_ERR) break;
                     // continue just skips to next iteration of while
                 }
                 val_del(v);
                 return res;
             }

             // If: (if cond then [else])
             if (strcmp(op, "if") == 0) {
                 val* cond = val_eval(e, val_copy(v->cell[1]));
                 int c = (cond->type == VAL_INT && cond->num != 0) || (cond->type == VAL_FLOAT && cond->dec != 0.0);
                 val_del(cond);
                 val* res;
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
                 val* res = val_num(0);
                 for (int i = 1; i < v->count; i++) {
                     val_del(res);
                     res = val_eval(e, val_copy(v->cell[i]));
                     if (res->type == VAL_ERR) break;
                 }
                 val_del(v);
                 return res;
             }
        }
    }
    return v;
}

val* builtin_load(lenv* e, val* a) {
    val* filename = a->cell[0];
    FILE* f = fopen(filename->str, "rb");
    if (!f) {
        val_del(a);
        return val_err("Could not open file");
    }
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* input = malloc(length + 1);
    fread(input, 1, length, f);
    input[length] = '\0';
    fclose(f);

    val* expr = val_read(input);
    free(input);
    val* x = val_eval(e, expr);
    val_del(a);
    return x;
}

int main(int argc, char** argv) {
    lenv* e = lenv_new();
    
    if (argc >= 2) {
        clock_t start = clock();
        val* args = val_sexpr();
        val_add(args, val_str(argv[1]));
        val* x = builtin_load(e, args);
        // Silent mode: Only print if error
        if (x->type == VAL_ERR) {
            putchar('\n');
            val_println(x);
        }
        val_del(x);

        clock_t end = clock();
        double time_spent = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);

        printf("\n--- Runtime Statistics ---\n");
        printf("Execution Time: %.2f ms\n", time_spent);
        printf("Peak Memory: %ld KB\n", usage.ru_maxrss);
    } else {
        puts("VClang Version 0.5");
        puts("Press Ctrl+C to Exit or type ';' to end expressions.\n");

        char buffer[2048];
        while (1) {
            printf("vclang> ");
            if (!fgets(buffer, 2048, stdin)) break;
            val* x = val_read(buffer);
            val* r = val_eval(e, x);
            val_println(r);
            val_del(r);
        }
    }
    
    lenv_del(e);
    return 0;
}
