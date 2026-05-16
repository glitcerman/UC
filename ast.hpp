// ast.hpp
#pragma once
#include <string>
#include <vector>
#include <memory>

// On utilise des smart pointers pour éviter les fuites mémoire
// unique_ptr = un seul propriétaire, libéré automatiquement
using NodePtr = std::unique_ptr<struct Node>;

// Type de chaque noeud de l'AST
enum class NodeType {
    // Déclarations
    PROGRAM,        // le programme entier
    FUNC_DECL,      // func add(int a, int b) { ... }
    VAR_DECL,       // var x = 42
    PARAM,          // int a (paramètre de fonction)

    // Instructions
    RETURN_STMT,    // return a + b
    IF_STMT,        // if x > 0 { ... }
    WHILE_STMT,     // while x > 0 { ... }
    FOR_STMT,       // for ... { ... }
    BLOCK,          // { ... }

    // Expressions
    BINARY_EXPR,    // a + b, x == y ...
    UNARY_EXPR,     // -x, !b
    CALL_EXPR,      // add(1, 2)
    ASSIGN_EXPR,    // x = 42

    // Littéraux et identifiants
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    BOOL_LITERAL,
    IDENT
};

// Noeud de base dont tous les autres héritent
struct Node {
    NodeType type;
    int line, column;

    Node(NodeType type, int line, int column)
        : type(type), line(line), column(column) {}

    virtual ~Node() = default;
};

// --- Littéraux ---

struct IntLiteral : Node {
    int value;
    IntLiteral(int value, int line, int col)
        : Node(NodeType::INT_LITERAL, line, col), value(value) {}
};

struct FloatLiteral : Node {
    float value;
    FloatLiteral(float value, int line, int col)
        : Node(NodeType::FLOAT_LITERAL, line, col), value(value) {}
};

struct StringLiteral : Node {
    std::string value;
    StringLiteral(const std::string& value, int line, int col)
        : Node(NodeType::STRING_LITERAL, line, col), value(value) {}
};

struct BoolLiteral : Node {
    bool value;
    BoolLiteral(bool value, int line, int col)
        : Node(NodeType::BOOL_LITERAL, line, col), value(value) {}
};

// --- Identifiant ---

struct Ident : Node {
    std::string name;
    Ident(const std::string& name, int line, int col)
        : Node(NodeType::IDENT, line, col), name(name) {}
};

// --- Expressions ---

struct BinaryExpr : Node {
    std::string op;   // "+", "-", "==", "<" ...
    NodePtr left;
    NodePtr right;

    BinaryExpr(const std::string& op, NodePtr left, NodePtr right, int line, int col)
        : Node(NodeType::BINARY_EXPR, line, col), op(op),
          left(std::move(left)), right(std::move(right)) {}
};

struct UnaryExpr : Node {
    std::string op;   // "-" ou "!"
    NodePtr operand;

    UnaryExpr(const std::string& op, NodePtr operand, int line, int col)
        : Node(NodeType::UNARY_EXPR, line, col), op(op),
          operand(std::move(operand)) {}
};

struct CallExpr : Node {
    std::string callee;               // nom de la fonction appelée
    std::vector<NodePtr> args;        // arguments

    CallExpr(const std::string& callee, std::vector<NodePtr> args, int line, int col)
        : Node(NodeType::CALL_EXPR, line, col), callee(callee),
          args(std::move(args)) {}
};

struct AssignExpr : Node {
    std::string name;   // variable cible
    NodePtr value;

    AssignExpr(const std::string& name, NodePtr value, int line, int col)
        : Node(NodeType::ASSIGN_EXPR, line, col), name(name),
          value(std::move(value)) {}
};

// --- Instructions ---

struct Block : Node {
    std::vector<NodePtr> statements;

    Block(std::vector<NodePtr> statements, int line, int col)
        : Node(NodeType::BLOCK, line, col),
          statements(std::move(statements)) {}
};

struct ReturnStmt : Node {
    NodePtr value;   // peut être nullptr si pas de valeur

    ReturnStmt(NodePtr value, int line, int col)
        : Node(NodeType::RETURN_STMT, line, col), value(std::move(value)) {}
};

struct IfStmt : Node {
    NodePtr condition;
    NodePtr thenBlock;
    NodePtr elseBlock;   // peut être nullptr si pas de else

    IfStmt(NodePtr condition, NodePtr thenBlock, NodePtr elseBlock, int line, int col)
        : Node(NodeType::IF_STMT, line, col), condition(std::move(condition)),
          thenBlock(std::move(thenBlock)), elseBlock(std::move(elseBlock)) {}
};

struct WhileStmt : Node {
    NodePtr condition;
    NodePtr body;

    WhileStmt(NodePtr condition, NodePtr body, int line, int col)
        : Node(NodeType::WHILE_STMT, line, col), condition(std::move(condition)),
          body(std::move(body)) {}
};

struct ForStmt : Node {
    NodePtr init;       // var i = 0
    NodePtr condition;  // i < 10
    NodePtr increment;  // i = i + 1
    NodePtr body;

    ForStmt(NodePtr init, NodePtr condition, NodePtr increment, NodePtr body, int line, int col)
        : Node(NodeType::FOR_STMT, line, col), init(std::move(init)),
          condition(std::move(condition)), increment(std::move(increment)),
          body(std::move(body)) {}
};

// --- Déclarations ---

struct Param : Node {
    std::string typeName;   // "int", "float"...
    std::string name;

    Param(const std::string& typeName, const std::string& name, int line, int col)
        : Node(NodeType::PARAM, line, col), typeName(typeName), name(name) {}
};

struct VarDecl : Node {
    std::string name;
    NodePtr value;           // valeur initiale
    std::string typeName;    // vide si inféré

    VarDecl(const std::string& name, NodePtr value, const std::string& typeName, int line, int col)
        : Node(NodeType::VAR_DECL, line, col), name(name),
          value(std::move(value)), typeName(typeName) {}
};

struct FuncDecl : Node {
    std::string name;
    std::vector<Param> params;
    NodePtr body;
    std::string returnType;   // vide si inféré

    FuncDecl(const std::string& name, std::vector<Param> params,
             NodePtr body, const std::string& returnType, int line, int col)
        : Node(NodeType::FUNC_DECL, line, col), name(name), params(std::move(params)),
          body(std::move(body)), returnType(returnType) {}
};

// Le programme entier = une liste de déclarations
struct Program : Node {
    std::vector<NodePtr> declarations;

    Program(std::vector<NodePtr> declarations, int line, int col)
        : Node(NodeType::PROGRAM, line, col),
          declarations(std::move(declarations)) {}
};