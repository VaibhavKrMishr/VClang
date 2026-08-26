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
    AST_CONTINUE,
    AST_FUNC_DEF,
    AST_ARRAY_LIT,
    AST_INDEX,
    AST_INDEX_ASSIGN,
    AST_NEW,
    AST_STRUCT_DEF,
    AST_STRUCT_LIT,
    AST_MEMBER_ACCESS,
    AST_MEMBER_ASSIGN,
    AST_METHOD_CALL
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

        struct {
            const char *name;
            const char **param_types;
            const char **params;
            int param_count;
            const ASTNode *body;
        } func_def;

        struct {
            const ASTNode **elements;
            int count;
        } array_lit;

        struct {
            const ASTNode *target;
            const ASTNode *index;
        } index_expr;

        struct {
            const ASTNode *target;
            const ASTNode *index;
            const ASTNode *value;
        } index_assign;

        struct {
            const ASTNode *allocator;
            const char *type_name;
            const ASTNode *size_expr;
            const ASTNode *init_expr;
        } new_expr;

        struct {
            const char *name;
            const char **field_types;
            const char **field_names;
            int field_count;
            const ASTNode **methods;
            int method_count;
        } struct_def;

        struct {
            const char *type_name;
            const char **field_names;
            const ASTNode **field_values;
            int field_count;
        } struct_lit;

        struct {
            const ASTNode *target;
            const char *member;
        } member_access;

        struct {
            const ASTNode *target;
            const char *member;
            OpType op;
            const ASTNode *value;
        } member_assign;

        struct {
            const ASTNode *target;
            const char *method_name;
            const ASTNode **args;
            int arg_count;
        } method_call;
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
ASTNode *ast_func_def(Arena *a, const char *name,
                      const char **param_types, const char **params, int param_count,
                      const ASTNode *body, int line);
ASTNode *ast_array_lit(Arena *a, const ASTNode **elements, int count, int line);
ASTNode *ast_index(Arena *a, const ASTNode *target, const ASTNode *index, int line);
ASTNode *ast_index_assign(Arena *a, const ASTNode *target, const ASTNode *index,
                          const ASTNode *value, int line);
ASTNode *ast_new_expr(Arena *a, const ASTNode *allocator, const char *type_name,
                      const ASTNode *size_expr, const ASTNode *init_expr, int line);
ASTNode *ast_struct_def(Arena *a, const char *name,
                        const char **field_types, const char **field_names, int field_count,
                        const ASTNode **methods, int method_count, int line);
ASTNode *ast_struct_lit(Arena *a, const char *type_name,
                        const char **field_names, const ASTNode **field_values, int field_count, int line);
ASTNode *ast_member_access(Arena *a, const ASTNode *target, const char *member, int line);
ASTNode *ast_member_assign(Arena *a, const ASTNode *target, const char *member,
                           OpType op, const ASTNode *value, int line);
ASTNode *ast_method_call(Arena *a, const ASTNode *target, const char *method_name,
                         const ASTNode **args, int arg_count, int line);

#endif // VCLANG_AST_H
