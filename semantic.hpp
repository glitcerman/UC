// semantic.hpp
#pragma once
#include "ast.hpp"
#include <unordered_map>
#include <stdexcept>

struct VarInfo {
    std::string typeName;
};

struct FuncInfo {
    std::string returnType;
    std::vector<std::string> paramTypes;
};

struct FieldInfo {
    std::string typeName;
    bool isPublic;
};

struct ClassInfo {
    std::string parent;
    std::unordered_map<std::string, FieldInfo> fields;
    std::unordered_map<std::string, FuncInfo> methods;
};

class SemanticAnalyzer {
private:
    std::unordered_map<std::string, VarInfo> variables;
    std::unordered_map<std::string, FuncInfo> functions;
    std::unordered_map<std::string, ClassInfo> classes;
    std::unordered_map<std::string, std::string> objectTypes; // varName -> className

    void error(const std::string& msg, int line, int col) {
        throw std::runtime_error("UC Semantic Error at line " +
            std::to_string(line) + ", column " + std::to_string(col) + ": " + msg);
    }

    std::string inferType(Node* node) {
        if (!node) return "void";

        switch (node->type) {
            case NodeType::INT_LITERAL:    return "int";
            case NodeType::FLOAT_LITERAL:  return "float";
            case NodeType::STRING_LITERAL: return "string";
            case NodeType::BOOL_LITERAL:   return "bool";

            case NodeType::IDENT: {
                auto* n = static_cast<Ident*>(node);
                if (variables.find(n->name) == variables.end())
                    error("undefined variable '" + n->name + "'", n->line, n->column);
                return variables[n->name].typeName;
            }

            case NodeType::BINARY_EXPR: {
                auto* n = static_cast<BinaryExpr*>(node);
                std::string left  = inferType(n->left.get());
                std::string right = inferType(n->right.get());
                if (n->op == "==" || n->op == "!=" || n->op == "<" ||
                    n->op == ">"  || n->op == "<=" || n->op == ">=")
                    return "bool";
                if (left != right)
                    error("type mismatch: '" + left + "' and '" + right + "'",
                          n->line, n->column);
                return left;
            }

            case NodeType::UNARY_EXPR: {
                auto* n = static_cast<UnaryExpr*>(node);
                std::string t = inferType(n->operand.get());
                if (n->op == "!" && t != "bool")
                    error("operator '!' requires a bool operand", n->line, n->column);
                if (n->op == "-" && t != "int" && t != "float")
                    error("operator '-' requires a numeric operand", n->line, n->column);
                return t;
            }

            case NodeType::CALL_EXPR: {
                auto* n = static_cast<CallExpr*>(node);
                if (n->callee == "print") return "void";
                if (n->callee == "input") return "int";
                if (functions.find(n->callee) == functions.end())
                    error("undefined function '" + n->callee + "'", n->line, n->column);
                return functions[n->callee].returnType;
            }

            case NodeType::MEMBER_ACCESS: {
                auto* n = static_cast<MemberAccess*>(node);
                if (objectTypes.find(n->object) == objectTypes.end())
                    error("undefined object '" + n->object + "'", n->line, n->column);
                std::string className = objectTypes[n->object];
                if (classes.find(className) == classes.end())
                    error("undefined class '" + className + "'", n->line, n->column);
                auto& cls = classes[className];
                if (n->isCall) {
                    if (cls.methods.find(n->member) == cls.methods.end())
                        error("undefined method '" + n->member + "' in class '" + className + "'",
                              n->line, n->column);
                    return cls.methods[n->member].returnType;
                } else {
                    if (cls.fields.find(n->member) == cls.fields.end())
                        error("undefined field '" + n->member + "' in class '" + className + "'",
                              n->line, n->column);
                    return cls.fields[n->member].typeName;
                }
            }

            default:
                return "";
        }
    }

    void analyzeStatement(Node* node, std::string& inferredReturn) {
        if (!node) return;

        switch (node->type) {

            case NodeType::VAR_DECL: {
                auto* n = static_cast<VarDecl*>(node);
                std::string t = inferType(n->value.get());
                variables[n->name] = { t };
                break;
            }

            case NodeType::OBJECT_DECL: {
                auto* n = static_cast<ObjectDecl*>(node);
                if (classes.find(n->className) == classes.end())
                    error("undefined class '" + n->className + "'", n->line, n->column);
                objectTypes[n->varName] = n->className;
                variables[n->varName] = { n->className };
                break;
            }

            case NodeType::ASSIGN_EXPR: {
                auto* n = static_cast<AssignExpr*>(node);
                if (variables.find(n->name) == variables.end())
                    error("undefined variable '" + n->name + "'", n->line, n->column);
                break;
            }

            case NodeType::MEMBER_ASSIGN: {
                auto* n = static_cast<MemberAssign*>(node);
                if (objectTypes.find(n->object) == objectTypes.end())
                    error("undefined object '" + n->object + "'", n->line, n->column);
                break;
            }

            case NodeType::RETURN_STMT: {
                auto* n = static_cast<ReturnStmt*>(node);
                std::string t = inferType(n->value.get());
                if (inferredReturn.empty()) inferredReturn = t;
                else if (inferredReturn != t)
                    error("ambiguous return type: '" + inferredReturn + "' and '" + t +
                          "' — please specify return type with '->'", n->line, n->column);
                break;
            }

            case NodeType::IF_STMT: {
                auto* n = static_cast<IfStmt*>(node);
                analyzeBlock(n->thenBlock.get(), inferredReturn);
                if (n->elseBlock) analyzeBlock(n->elseBlock.get(), inferredReturn);
                break;
            }

            case NodeType::WHILE_STMT: {
                auto* n = static_cast<WhileStmt*>(node);
                analyzeBlock(n->body.get(), inferredReturn);
                break;
            }

            case NodeType::FOR_STMT: {
                auto* n = static_cast<ForStmt*>(node);
                analyzeStatement(n->init.get(), inferredReturn);
                analyzeStatement(n->increment.get(), inferredReturn);
                analyzeBlock(n->body.get(), inferredReturn);
                break;
            }

            case NodeType::CALL_EXPR:
            case NodeType::MEMBER_ACCESS:
                inferType(node);
                break;

            default:
                break;
        }
    }

    void analyzeBlock(Node* node, std::string& inferredReturn) {
        auto* block = static_cast<Block*>(node);
        for (auto& stmt : block->statements)
            analyzeStatement(stmt.get(), inferredReturn);
    }

    void analyzeFuncDecl(FuncDecl* func) {
        auto savedVars = variables;
        auto savedObjs = objectTypes;

        for (auto& param : func->params)
            variables[param.name] = { param.typeName };

        std::string inferredReturn = "";
        analyzeBlock(func->body.get(), inferredReturn);

        functions[func->name].returnType = func->returnType.empty()
            ? inferredReturn : func->returnType;

        variables = savedVars;
        objectTypes = savedObjs;
    }

    void analyzeClassDecl(ClassDecl* cls) {
        ClassInfo info;
        info.parent = cls->parent;

        // Enregistre les champs
        for (auto& field : cls->fields) {
            std::string fieldType = "int"; // type par défaut
            if (field.defaultValue) {
                // On infère le type de la valeur par défaut
                switch (field.defaultValue->type) {
                    case NodeType::INT_LITERAL:    fieldType = "int"; break;
                    case NodeType::FLOAT_LITERAL:  fieldType = "float"; break;
                    case NodeType::STRING_LITERAL: fieldType = "string"; break;
                    case NodeType::BOOL_LITERAL:   fieldType = "bool"; break;
                    default: break;
                }
            }
            info.fields[field.name] = { fieldType, field.isPublic };
        }

        // Enregistre les méthodes
        for (auto& method : cls->methods) {
            FuncInfo mInfo;
            mInfo.returnType = method->returnType;
            for (auto& p : method->params)
                mInfo.paramTypes.push_back(p.typeName);
            info.methods[method->name] = mInfo;
        }

        classes[cls->name] = info;
    }

public:
    void analyze(Node* program) {
        auto* prog = static_cast<Program*>(program);

        functions["print"] = { "void", {} };
        functions["input"] = { "int",  {} };

        // Première passe : enregistre classes et fonctions
        for (auto& decl : prog->declarations) {
            if (decl->type == NodeType::CLASS_DECL) {
                analyzeClassDecl(static_cast<ClassDecl*>(decl.get()));
            } else if (decl->type == NodeType::FUNC_DECL) {
                auto* func = static_cast<FuncDecl*>(decl.get());
                FuncInfo info;
                info.returnType = func->returnType;
                for (auto& p : func->params)
                    info.paramTypes.push_back(p.typeName);
                functions[func->name] = info;
            }
        }

        // Deuxième passe : analyse complète
        for (auto& decl : prog->declarations) {
            if (decl->type == NodeType::FUNC_DECL)
                analyzeFuncDecl(static_cast<FuncDecl*>(decl.get()));
            else if (decl->type == NodeType::VAR_DECL) {
                std::string dummy = "";
                analyzeStatement(decl.get(), dummy);
            }
        }
    }
};
