// ast.hpp
#pragma once
#include <string>
#include <vector>
#include <memory>

using NodePtr = std::unique_ptr<struct Node>;

enum class NodeType {
    // Déclarations
    PROGRAM,
    FUNC_DECL,
    VAR_DECL,
    PARAM,
    CLASS_DECL,     // class Animal { ... }
    METHOD_DECL,    // méthode dans une classe

    // Instructions
    RETURN_STMT,
    IF_STMT,
    WHILE_STMT,
    FOR_STMT,
    BLOCK,
    OBJECT_DECL,    // Animal a(5)

    // Expressions
    BINARY_EXPR,
    UNARY_EXPR,
    CALL_EXPR,
    ASSIGN_EXPR,
    MEMBER_ACCESS,  // a:method() ou a:field
    MEMBER_ASSIGN,  // a:field = value

    // Littéraux et identifiants
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    BOOL_LITERAL,
    IDENT
};

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

struct Ident : Node {
    std::string name;
    Ident(const std::string& name, int line, int col)
        : Node(NodeType::IDENT, line, col), name(name) {}
};

// --- Expressions ---

struct BinaryExpr : Node {
    std::string op;
    NodePtr left;
    NodePtr right;

    BinaryExpr(const std::string& op, NodePtr left, NodePtr right, int line, int col)
        : Node(NodeType::BINARY_EXPR, line, col), op(op),
          left(std::move(left)), right(std::move(right)) {}
};

struct UnaryExpr : Node {
    std::string op;
    NodePtr operand;

    UnaryExpr(const std::string& op, NodePtr operand, int line, int col)
        : Node(NodeType::UNARY_EXPR, line, col), op(op),
          operand(std::move(operand)) {}
};

struct CallExpr : Node {
    std::string callee;
    std::vector<NodePtr> args;

    CallExpr(const std::string& callee, std::vector<NodePtr> args, int line, int col)
        : Node(NodeType::CALL_EXPR, line, col), callee(callee),
          args(std::move(args)) {}
};

struct AssignExpr : Node {
    std::string name;
    NodePtr value;

    AssignExpr(const std::string& name, NodePtr value, int line, int col)
        : Node(NodeType::ASSIGN_EXPR, line, col), name(name),
          value(std::move(value)) {}
};

// a:method(args) ou a:field
struct MemberAccess : Node {
    std::string object;     // nom de l'objet : "a"
    std::string member;     // nom du membre : "speak"
    std::vector<NodePtr> args;  // arguments si c'est un appel de méthode
    bool isCall;            // true si appel de méthode, false si accès champ

    MemberAccess(const std::string& object, const std::string& member,
                 std::vector<NodePtr> args, bool isCall, int line, int col)
        : Node(NodeType::MEMBER_ACCESS, line, col), object(object), member(member),
          args(std::move(args)), isCall(isCall) {}
};

// a:field = value
struct MemberAssign : Node {
    std::string object;
    std::string member;
    NodePtr value;

    MemberAssign(const std::string& object, const std::string& member,
                 NodePtr value, int line, int col)
        : Node(NodeType::MEMBER_ASSIGN, line, col), object(object), member(member),
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
    NodePtr value;

    ReturnStmt(NodePtr value, int line, int col)
        : Node(NodeType::RETURN_STMT, line, col), value(std::move(value)) {}
};

struct IfStmt : Node {
    NodePtr condition;
    NodePtr thenBlock;
    NodePtr elseBlock;

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
    NodePtr init;
    NodePtr condition;
    NodePtr increment;
    NodePtr body;

    ForStmt(NodePtr init, NodePtr condition, NodePtr increment, NodePtr body, int line, int col)
        : Node(NodeType::FOR_STMT, line, col), init(std::move(init)),
          condition(std::move(condition)), increment(std::move(increment)),
          body(std::move(body)) {}
};

// --- Déclarations ---

struct Param : Node {
    std::string typeName;
    std::string name;

    Param(const std::string& typeName, const std::string& name, int line, int col)
        : Node(NodeType::PARAM, line, col), typeName(typeName), name(name) {}
};

struct VarDecl : Node {
    std::string name;
    NodePtr value;
    std::string typeName;

    VarDecl(const std::string& name, NodePtr value, const std::string& typeName, int line, int col)
        : Node(NodeType::VAR_DECL, line, col), name(name),
          value(std::move(value)), typeName(typeName) {}
};

struct FuncDecl : Node {
    std::string name;
    std::vector<Param> params;
    NodePtr body;
    std::string returnType;

    FuncDecl(const std::string& name, std::vector<Param> params,
             NodePtr body, const std::string& returnType, int line, int col)
        : Node(NodeType::FUNC_DECL, line, col), name(name), params(std::move(params)),
          body(std::move(body)), returnType(returnType) {}
};

// Méthode d'une classe (comme FuncDecl mais avec className)
struct MethodDecl : Node {
    std::string className;
    std::string name;
    std::vector<Param> params;
    NodePtr body;
    std::string returnType;
    bool isPublic;

    MethodDecl(const std::string& className, const std::string& name,
               std::vector<Param> params, NodePtr body,
               const std::string& returnType, bool isPublic, int line, int col)
        : Node(NodeType::METHOD_DECL, line, col), className(className), name(name),
          params(std::move(params)), body(std::move(body)),
          returnType(returnType), isPublic(isPublic) {}
};

// Champ d'une classe
struct FieldDecl {
    std::string name;
    NodePtr defaultValue;
    bool isPublic;

    FieldDecl(const std::string& name, NodePtr defaultValue, bool isPublic)
        : name(name), defaultValue(std::move(defaultValue)), isPublic(isPublic) {}
};

// Constructeur d'une classe
struct ConstructorDecl {
    std::vector<Param> params;
    std::vector<std::pair<std::string, std::string>> assignments; // field -> param
    NodePtr body;

    ConstructorDecl(std::vector<Param> params,
                    std::vector<std::pair<std::string, std::string>> assignments,
                    NodePtr body)
        : params(std::move(params)), assignments(std::move(assignments)),
          body(std::move(body)) {}
};

// Déclaration d'une classe
struct ClassDecl : Node {
    std::string name;
    std::string parent;           // nom de la classe parente, vide si aucune
    std::vector<FieldDecl> fields;
    std::vector<std::unique_ptr<MethodDecl>> methods;
    std::unique_ptr<ConstructorDecl> constructor;

    ClassDecl(const std::string& name, const std::string& parent, int line, int col)
        : Node(NodeType::CLASS_DECL, line, col), name(name), parent(parent) {}
};

// Instanciation d'un objet : Animal a(5)
struct ObjectDecl : Node {
    std::string className;
    std::string varName;
    std::vector<NodePtr> args;

    ObjectDecl(const std::string& className, const std::string& varName,
               std::vector<NodePtr> args, int line, int col)
        : Node(NodeType::OBJECT_DECL, line, col), className(className),
          varName(varName), args(std::move(args)) {}
};

// Le programme entier
struct Program : Node {
    std::vector<NodePtr> declarations;

    Program(std::vector<NodePtr> declarations, int line, int col)
        : Node(NodeType::PROGRAM, line, col),
          declarations(std::move(declarations)) {}
};
