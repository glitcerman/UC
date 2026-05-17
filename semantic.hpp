// semantic.hpp
#pragma once
#include "ast.hpp"
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

// Représente une variable connue du compilateur
struct VarInfo {
    std::string typeName;  // "int", "float", "string", "bool", ou "" si pas encore résolu
};

// Représente une fonction connue du compilateur
struct FuncInfo {
    std::string returnType;
    std::vector<std::string> paramTypes;
};

class SemanticAnalyzer {
private:
    // Table des variables : nom -> info
    std::unordered_map<std::string, VarInfo> variables;

    // Table des fonctions : nom -> info
    std::unordered_map<std::string, FuncInfo> functions;

    // Erreur sémantique
    void error(const std::string& msg, int line, int col) {
        throw std::runtime_error("UC Semantic Error at line " +
            std::to_string(line) + ", column " + std::to_string(col) + ": " + msg);
    }

    // --- Inférence de type ---

    // Déduit le type d'une expression
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

                // Comparaisons retournent toujours un bool
                if (n->op == "==" || n->op == "!=" ||
                    n->op == "<"  || n->op == ">"  ||
                    n->op == "<=" || n->op == ">=")
                    return "bool";

                // Les deux côtés doivent avoir le même type
                if (left != right)
                    error("type mismatch in expression: '" + left +
                          "' and '" + right + "'", n->line, n->column);

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

                if (functions.find(n->callee) == functions.end())
                    error("undefined function '" + n->callee + "'", n->line, n->column);

                FuncInfo& func = functions[n->callee];

                // Vérification du nombre d'arguments
                if (n->args.size() != func.paramTypes.size())
                    error("function '" + n->callee + "' expects " +
                          std::to_string(func.paramTypes.size()) + " argument(s), got " +
                          std::to_string(n->args.size()), n->line, n->column);

                // Vérification des types des arguments
                for (int i = 0; i < n->args.size(); i++) {
                    std::string argType = inferType(n->args[i].get());
                    if (argType != func.paramTypes[i])
                        error("argument " + std::to_string(i + 1) + " of '" + n->callee +
                              "' expects '" + func.paramTypes[i] + "', got '" + argType + "'",
                              n->line, n->column);
                }

                return func.returnType;
            }

            default:
                return "";
        }
    }

    // --- Analyse des instructions ---

    void analyzeStatement(Node* node, std::string& inferredReturn) {
        if (!node) return;

        switch (node->type) {

            case NodeType::VAR_DECL: {
                auto* n = static_cast<VarDecl*>(node);
                std::string t = inferType(n->value.get());
                variables[n->name] = { t };
                break;
            }

            case NodeType::ASSIGN_EXPR: {
                auto* n = static_cast<AssignExpr*>(node);
                if (variables.find(n->name) == variables.end())
                    error("undefined variable '" + n->name + "'", n->line, n->column);

                std::string newType = inferType(n->value.get());
                std::string oldType = variables[n->name].typeName;

                if (oldType != newType)
                    error("cannot assign '" + newType + "' to variable '" +
                          n->name + "' of type '" + oldType + "'", n->line, n->column);
                break;
            }

            case NodeType::RETURN_STMT: {
                auto* n = static_cast<ReturnStmt*>(node);
                std::string t = inferType(n->value.get());

                // Premier return rencontré → on note le type
                if (inferredReturn.empty()) {
                    inferredReturn = t;
                }
                // Return suivant → doit avoir le même type
                else if (inferredReturn != t) {
                    error("ambiguous return type: '" + inferredReturn +
                          "' and '" + t + "' — please specify return type with '->'",
                          n->line, n->column);
                }
                break;
            }

            case NodeType::IF_STMT: {
                auto* n = static_cast<IfStmt*>(node);
                std::string condType = inferType(n->condition.get());
                if (condType != "bool")
                    error("if condition must be a bool, got '" + condType + "'",
                          n->line, n->column);
                analyzeBlock(n->thenBlock.get(), inferredReturn);
                if (n->elseBlock)
                    analyzeBlock(n->elseBlock.get(), inferredReturn);
                break;
            }

            case NodeType::WHILE_STMT: {
                auto* n = static_cast<WhileStmt*>(node);
                std::string condType = inferType(n->condition.get());
                if (condType != "bool")
                    error("while condition must be a bool, got '" + condType + "'",
                          n->line, n->column);
                analyzeBlock(n->body.get(), inferredReturn);
                break;
            }

            case NodeType::FOR_STMT: {
                auto* n = static_cast<ForStmt*>(node);
                analyzeStatement(n->init.get(), inferredReturn);
                std::string condType = inferType(n->condition.get());
                if (condType != "bool")
                    error("for condition must be a bool, got '" + condType + "'",
                          n->line, n->column);
                analyzeStatement(n->increment.get(), inferredReturn);
                analyzeBlock(n->body.get(), inferredReturn);
                break;
            }

            // Appel de fonction seul (sans récupérer la valeur)
            case NodeType::CALL_EXPR: {
                inferType(node);
                break;
            }

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
        // Sauvegarde les variables existantes (portée locale)
        auto savedVars = variables;

        // Ajoute les paramètres comme variables locales
        for (auto& param : func->params)
            variables[param.name] = { param.typeName };

        // Analyse le corps et infère le type de retour
        std::string inferredReturn = "";
        analyzeBlock(func->body.get(), inferredReturn);

        // Si le type de retour est explicite, vérifie la cohérence
        if (!func->returnType.empty() && !inferredReturn.empty()) {
            if (func->returnType != inferredReturn)
                error("function '" + func->name + "' declares return type '" +
                      func->returnType + "' but returns '" + inferredReturn + "'",
                      func->line, func->column);
        }

        // Met à jour le type de retour dans la table des fonctions
        functions[func->name].returnType = func->returnType.empty()
            ? inferredReturn : func->returnType;

        // Restaure les variables de la portée parente
        variables = savedVars;
    }

public:
    void analyze(Node* program) {
        auto* prog = static_cast<Program*>(program);

        // Fonctions built-in
        functions["print"] = { "void", { "int" } };
        functions["input"] = { "int", {} }; // <- NOUVEAU : pas de paramètres, retourne un int

        // Première passe : enregistre toutes les fonctions
        for (auto& decl : prog->declarations) {
            if (decl->type == NodeType::FUNC_DECL) {
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