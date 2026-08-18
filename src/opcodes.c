#include "../include/opcodes.h"
#include <string.h>

OpType op_from_string(const char *s) {
    // Single-char operators (most common — check first)
    if (s[1] == '\0') {
        switch (s[0]) {
        case '+': return OP_ADD;
        case '-': return OP_SUB;
        case '*': return OP_MUL;
        case '/': return OP_DIV;
        case '%': return OP_MOD;
        case '<': return OP_LT;
        case '>': return OP_GT;
        case '!': return OP_NOT;
        default:  return OP_UNKNOWN;
        }
    }
    // Two-char operators
    if (s[0] == '=' && s[1] == '=') return OP_EQ;
    if (s[0] == '!' && s[1] == '=') return OP_NE;
    if (s[0] == '<' && s[1] == '=') return OP_LE;
    if (s[0] == '>' && s[1] == '=') return OP_GE;
    if (s[0] == '&' && s[1] == '&') return OP_AND;
    if (s[0] == '|' && s[1] == '|') return OP_OR;

    return OP_UNKNOWN;
}

const char *op_to_string(OpType op) {
    switch (op) {
    case OP_ADD: return "+";
    case OP_SUB: return "-";
    case OP_MUL: return "*";
    case OP_DIV: return "/";
    case OP_MOD: return "%";
    case OP_EQ:  return "==";
    case OP_NE:  return "!=";
    case OP_LT:  return "<";
    case OP_GT:  return ">";
    case OP_LE:  return "<=";
    case OP_GE:  return ">=";
    case OP_AND: return "&&";
    case OP_OR:  return "||";
    case OP_NOT: return "!";
    case OP_UNKNOWN: return "?";
    }
    return "?";
}
