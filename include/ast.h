#ifndef VCLANG_AST_H
#define VCLANG_AST_H

#include "memory.h"
#include "opcodes.h"

typedef enum {
    AST_INT_LIT,
    AST_FLOAT_LIT,
    AST_STRING_LIT,
    AST_IDENT,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_CALL,
    AST_DECL,
    AST_ASSIGN,
    AST_IF,
    AST_WHILE,
    AST_FOR_BODY,
    AST_BLOCK,
    AST_PROGRAM,
    AST_RETURN,
    AST_BREAK,
    AST_CONTINUE
} ASTNodeType;

typedef struct ASTNode ASTNode;

struct ASTNode {
    ASTNodeType type;
    int line;

    union {
        long int_val;                   // AST_INT_LIT
        double float_val;               // AST_FLOAT_LIT
        const char *str_val;            // AST_STRING_LIT, AST_IDENT

        struct { OpType op; const ASTNode *lhs; const ASTNode *rhs; } binary;
        struct { OpType op; const ASTNode *operand; } unary;

        struct {
            const char *func_name;
            const ASTNode **args;
            int arg_count;
        } call;

        struct { const char *name; const ASTNode *value; } decl; // AST_DECL, AST_ASSIGN

        struct {
            const ASTNode *cond;
            const ASTNode *then_body;
            const ASTNode *else_body;
        } if_stmt;

        struct { const ASTNode *cond; const ASTNode *body; } while_stmt;
        struct { const ASTNode *body; const ASTNode *step; } for_body;

        struct {
            const ASTNode **stmts;
            int stmt_count;
        } block;                        // AST_BLOCK, AST_PROGRAM

        struct { const ASTNode *expr; } ret;
    };
};

ASTNode *ast_int_lit(Arena *a, long val, int line);
ASTNode *ast_float_lit(Arena *a, double val, int line);
ASTNode *ast_string_lit(Arena *a, const char *s, int line);
ASTNode *ast_ident(Arena *a, const char *name, int line);
ASTNode *ast_binary_op(Arena *a, OpType op,
                       const ASTNode *lhs, const ASTNode *rhs, int line);
ASTNode *ast_unary_op(Arena *a, OpType op,
                      const ASTNode *operand, int line);
ASTNode *ast_call(Arena *a, const char *func_name,
                  const ASTNode **args, int arg_count, int line);
ASTNode *ast_decl(Arena *a, const char *name,
                  const ASTNode *value, int line);
ASTNode *ast_assign(Arena *a, const char *name,
                    const ASTNode *value, int line);
ASTNode *ast_if(Arena *a, const ASTNode *cond,
                const ASTNode *then_body, const ASTNode *else_body, int line);
ASTNode *ast_while(Arena *a, const ASTNode *cond,
                   const ASTNode *body, int line);
ASTNode *ast_for_body(Arena *a, const ASTNode *body,
                      const ASTNode *step, int line);
ASTNode *ast_block(Arena *a, const ASTNode **stmts, int count, int line);
ASTNode *ast_program(Arena *a, const ASTNode **stmts, int count);
ASTNode *ast_return(Arena *a, const ASTNode *expr, int line);
ASTNode *ast_break(Arena *a, int line);
ASTNode *ast_continue(Arena *a, int line);

#endif // VCLANG_AST_H
