// codegen.hpp
#pragma once
#include "ast.hpp"
#include <string>
#include <unordered_map>
#include <sstream>

class CodeGen {
private:
    std::ostringstream output;  // le code LLVM IR généré
    int tempCounter = 0;        // compteur pour les variables temporaires
    int labelCounter = 0;       // compteur pour les labels (if, while, for)

    // Génère un nom de variable temporaire unique : %t0, %t1, %t2...
    std::string newTemp() {
        return "%t" + std::to_string(tempCounter++);
    }

    // Génère un nom de label unique : label0, label1...
    std::string newLabel() {
        return "label" + std::to_string(labelCounter++);
    }

    // Convertit un type UC en type LLVM
    std::string llvmType(const std::string& type) {
        if (type == "int")    return "i32";
        if (type == "float")  return "float";
        if (type == "bool")   return "i1";
        if (type == "string") return "i8*";
        if (type == "void")   return "void";
        return "i32"; // défaut
    }

    // Table des variables locales : nom -> registre LLVM
    std::unordered_map<std::string, std::string> varRegs;

    // --- Génération des expressions ---

    // Retourne le registre LLVM contenant le résultat de l'expression
    std::string genExpr(Node* node) {
        switch (node->type) {

            case NodeType::INT_LITERAL: {
                auto* n = static_cast<IntLiteral*>(node);
                std::string t = newTemp();
                output << "  " << t << " = add i32 0, " << n->value << "\n";
                return t;
            }

            case NodeType::FLOAT_LITERAL: {
                auto* n = static_cast<FloatLiteral*>(node);
                std::string t = newTemp();
                output << "  " << t << " = fadd float 0.0, " << n->value << "\n";
                return t;
            }

            case NodeType::BOOL_LITERAL: {
                auto* n = static_cast<BoolLiteral*>(node);
                std::string t = newTemp();
                output << "  " << t << " = add i1 0, " << (n->value ? "1" : "0") << "\n";
                return t;
            }

            case NodeType::IDENT: {
                auto* n = static_cast<Ident*>(node);
                std::string t = newTemp();
                output << "  " << t << " = load i32, i32* " << varRegs[n->name] << "\n";
                return t;
            }

            case NodeType::BINARY_EXPR: {
                auto* n = static_cast<BinaryExpr*>(node);
                std::string left  = genExpr(n->left.get());
                std::string right = genExpr(n->right.get());
                std::string t     = newTemp();

                if      (n->op == "+")  output << "  " << t << " = add i32 "  << left << ", " << right << "\n";
                else if (n->op == "-")  output << "  " << t << " = sub i32 "  << left << ", " << right << "\n";
                else if (n->op == "*")  output << "  " << t << " = mul i32 "  << left << ", " << right << "\n";
                else if (n->op == "/")  output << "  " << t << " = sdiv i32 " << left << ", " << right << "\n";
                else if (n->op == "==") output << "  " << t << " = icmp eq i32 "  << left << ", " << right << "\n";
                else if (n->op == "!=") output << "  " << t << " = icmp ne i32 "  << left << ", " << right << "\n";
                else if (n->op == "<")  output << "  " << t << " = icmp slt i32 " << left << ", " << right << "\n";
                else if (n->op == ">")  output << "  " << t << " = icmp sgt i32 " << left << ", " << right << "\n";
                else if (n->op == "<=") output << "  " << t << " = icmp sle i32 " << left << ", " << right << "\n";
                else if (n->op == ">=") output << "  " << t << " = icmp sge i32 " << left << ", " << right << "\n";

                return t;
            }

            case NodeType::UNARY_EXPR: {
                auto* n = static_cast<UnaryExpr*>(node);
                std::string operand = genExpr(n->operand.get());
                std::string t = newTemp();

                if (n->op == "-")
                    output << "  " << t << " = sub i32 0, " << operand << "\n";
                else if (n->op == "!")
                    output << "  " << t << " = xor i1 " << operand << ", 1\n";

                return t;
            }

            case NodeType::CALL_EXPR: {
                auto* n = static_cast<CallExpr*>(node);

                // Cas spécial : print()
                if (n->callee == "print") {
                    for (auto& arg : n->args) {
                        std::string val = genExpr(arg.get());
                        std::string fmt = newTemp();
                        std::string fmtPtr = newTemp();

                        output << "  " << fmt << " = alloca [4 x i8]\n";
                        output << "  store [4 x i8] c\"%d\\0A\\00\", [4 x i8]* " << fmt << "\n";
                        output << "  " << fmtPtr << " = getelementptr [4 x i8], [4 x i8]* "
                               << fmt << ", i32 0, i32 0\n";
                        output << "  call i32 (i8*, ...) @printf(i8* " << fmtPtr
                               << ", i32 " << val << ")\n";
                    }
                    std::string t = newTemp();
                    output << "  " << t << " = add i32 0, 0\n";
                    return t;
                }

                std::vector<std::string> argRegs;
                for (auto& arg : n->args)
                    argRegs.push_back(genExpr(arg.get()));

                std::string t = newTemp();
                output << "  " << t << " = call i32 @" << n->callee << "(";

                for (int i = 0; i < argRegs.size(); i++) {
                    output << "i32 " << argRegs[i];
                    if (i < argRegs.size() - 1) output << ", ";
                }

                output << ")\n";
                return t;
            }
        default:
            return "";    
        }
    }

    void genStatement(Node* node) {
        if (!node) return;

        switch (node->type) {

            case NodeType::VAR_DECL: {
                auto* n = static_cast<VarDecl*>(node);
                // Alloue de la mémoire pour la variable
                std::string reg = "%" + n->name;
                output << "  " << reg << " = alloca i32\n";
                varRegs[n->name] = reg;
                // Initialise avec la valeur
                std::string val = genExpr(n->value.get());
                output << "  store i32 " << val << ", i32* " << reg << "\n";
                break;
            }

            case NodeType::ASSIGN_EXPR: {
                auto* n = static_cast<AssignExpr*>(node);
                std::string val = genExpr(n->value.get());
                output << "  store i32 " << val << ", i32* " << varRegs[n->name] << "\n";
                break;
            }

            case NodeType::RETURN_STMT: {
                auto* n = static_cast<ReturnStmt*>(node);
                if (n->value) {
                    std::string val = genExpr(n->value.get());
                    output << "  ret i32 " << val << "\n";
                } else {
                    output << "  ret void\n";
                }
                break;
            }

            case NodeType::IF_STMT: {
                auto* n = static_cast<IfStmt*>(node);
                std::string condReg  = genExpr(n->condition.get());
                std::string thenLbl  = newLabel();
                std::string elseLbl  = newLabel();
                std::string mergeLbl = newLabel();

                output << "  br i1 " << condReg << ", label %" << thenLbl
                       << ", label %" << (n->elseBlock ? elseLbl : mergeLbl) << "\n";

                // Then
                output << thenLbl << ":\n";
                genBlock(n->thenBlock.get());
                output << "  br label %" << mergeLbl << "\n";

                // Else (optionnel)
                if (n->elseBlock) {
                    output << elseLbl << ":\n";
                    genBlock(n->elseBlock.get());
                    output << "  br label %" << mergeLbl << "\n";
                }

                output << mergeLbl << ":\n";
                break;
            }

            case NodeType::WHILE_STMT: {
                auto* n = static_cast<WhileStmt*>(node);
                std::string condLbl  = newLabel();
                std::string bodyLbl  = newLabel();
                std::string mergeLbl = newLabel();

                output << "  br label %" << condLbl << "\n";
                output << condLbl << ":\n";

                std::string condReg = genExpr(n->condition.get());
                output << "  br i1 " << condReg << ", label %" << bodyLbl
                       << ", label %" << mergeLbl << "\n";

                output << bodyLbl << ":\n";
                genBlock(n->body.get());
                output << "  br label %" << condLbl << "\n";

                output << mergeLbl << ":\n";
                break;
            }

            case NodeType::FOR_STMT: {
                auto* n = static_cast<ForStmt*>(node);
                std::string condLbl  = newLabel();
                std::string bodyLbl  = newLabel();
                std::string mergeLbl = newLabel();

                // Initialisation
                genStatement(n->init.get());
                output << "  br label %" << condLbl << "\n";

                // Condition
                output << condLbl << ":\n";
                std::string condReg = genExpr(n->condition.get());
                output << "  br i1 " << condReg << ", label %" << bodyLbl
                       << ", label %" << mergeLbl << "\n";

                // Corps + incrément
                output << bodyLbl << ":\n";
                genBlock(n->body.get());
                genStatement(n->increment.get());
                output << "  br label %" << condLbl << "\n";

                output << mergeLbl << ":\n";
                break;
            }

            case NodeType::CALL_EXPR:
                genExpr(node);
                break;

            default:
                break;
        }
    }

    void genBlock(Node* node) {
        auto* block = static_cast<Block*>(node);
        for (auto& stmt : block->statements)
            genStatement(stmt.get());
    }

    // --- Génération des fonctions ---

    void genFuncDecl(FuncDecl* func) {
        varRegs.clear();

        // Signature de la fonction
        output << "define i32 @" << func->name << "(";

        for (int i = 0; i < func->params.size(); i++) {
            output << "i32 %" << func->params[i].name;
            if (i < func->params.size() - 1) output << ", ";
        }

        output << ") {\n";
        output << "entry:\n";

        // Alloue et stocke les paramètres
        for (auto& param : func->params) {
            std::string reg = "%" + param.name + "_addr";
            output << "  " << reg << " = alloca i32\n";
            output << "  store i32 %" << param.name << ", i32* " << reg << "\n";
            varRegs[param.name] = reg;
        }

        genBlock(func->body.get());

        output << "}\n\n";
    }

public:
        std::string generate(Node* program) {
        auto* prog = static_cast<Program*>(program);

        // En-tête LLVM
        output << "; UC Compiler — generated LLVM IR\n\n";

        // Déclaration de printf pour print()
        output << "declare i32 @printf(i8*, ...)\n\n";

        for (auto& decl : prog->declarations) {
            if (decl->type == NodeType::FUNC_DECL)
                genFuncDecl(static_cast<FuncDecl*>(decl.get()));
        }

        return output.str();
    }
};