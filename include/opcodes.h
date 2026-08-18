#ifndef VCLANG_OPCODES_H
#define VCLANG_OPCODES_H

// Operator enum — assigned at parse time, used by eval via switch/jump table.
// Replaces the old design where operators were heap-allocated strings
// that required strcmp chains (~14 comparisons) on every evaluation.

typedef enum {
    OP_ADD,     // +
    OP_SUB,     // -
    OP_MUL,     // *
    OP_DIV,     // /
    OP_MOD,     // %
    OP_EQ,      // ==
    OP_NE,      // !=
    OP_LT,      // <
    OP_GT,      // >
    OP_LE,      // <=
    OP_GE,      // >=
    OP_AND,     // &&
    OP_OR,      // ||
    OP_NOT,     // !
    OP_UNKNOWN
} OpType;

// Convert a string operator (from the lexer) to an enum value.
// Called once at parse time; the enum is then stored in the AST node.
OpType op_from_string(const char *s);

// Convert an enum back to a display string (for error messages).
const char *op_to_string(OpType op);

#endif // VCLANG_OPCODES_H
