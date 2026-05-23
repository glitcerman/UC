// codegen.hpp
#pragma once
#include "ast.hpp"
#include <string>
#include <unordered_map>
#include <sstream>

class CodeGen {
private:
    std::ostringstream output;
    int tempCounter = 0;
    int labelCounter = 0;

    std::string newTemp() { return "%t" + std::to_string(tempCounter++); }
    std::string newLabel() { return "label" + std::to_string(labelCounter++); }

    // Table des variables locales : nom -> registre LLVM
    std::unordered_map<std::string, std::string> varRegs;

    // Table des objets : varName -> { className, { fieldName -> fieldIndex } }
    struct ObjectInfo {
        std::string className;
        std::unordered_map<std::string, int> fieldIndex;
        std::string ptrReg; // registre du pointeur vers la struct
    };
    std::unordered_map<std::string, ObjectInfo> objects;

    // Table des structs : className -> liste des champs
    struct StructInfo {
        std::vector<std::string> fields; // noms des champs dans l'ordre
    };
    std::unordered_map<std::string, StructInfo> structs;

    // --- Génération des expressions ---

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

            case NodeType::STRING_LITERAL: {
                auto* n = static_cast<StringLiteral*>(node);
                int len = (int)n->value.size() + 2;
                std::string lenStr = std::to_string(len);
                std::string fmt = newTemp();
                std::string fmtPtr = newTemp();

                output << "  " << fmt << " = alloca [" << lenStr << " x i8]\n";
                output << "  store [" << lenStr << " x i8] c\""
                       << n->value << "\\0A\\00\", [" << lenStr << " x i8]* " << fmt << "\n";
                output << "  " << fmtPtr << " = getelementptr ["
                       << lenStr << " x i8], [" << lenStr << " x i8]* "
                       << fmt << ", i32 0, i32 0\n";
                return fmtPtr;
            }

            case NodeType::IDENT: {
                auto* n = static_cast<Ident*>(node);
                std::string t = newTemp();

                // Vérifie si c'est un champ de self
                if (objects.find("self") != objects.end()) {
                    auto& self = objects["self"];
                    if (self.fieldIndex.find(n->name) != self.fieldIndex.end()) {
                        int idx = self.fieldIndex[n->name];
                        std::string ptr = newTemp();
                        output << "  " << ptr << " = getelementptr %struct." << self.className
                            << ", %struct." << self.className << "* %self, i32 0, i32 " << idx << "\n";
                        output << "  " << t << " = load i32, i32* " << ptr << "\n";
                        return t;
                    }
                }

                // Variable locale normale
                output << "  " << t << " = load i32, i32* " << varRegs[n->name] << "\n";
                return t;
            }

            case NodeType::BINARY_EXPR: {
                auto* n = static_cast<BinaryExpr*>(node);
                std::string left  = genExpr(n->left.get());
                std::string right = genExpr(n->right.get());
                std::string t     = newTemp();

                if      (n->op == "+")  output << "  " << t << " = add i32 "     << left << ", " << right << "\n";
                else if (n->op == "-")  output << "  " << t << " = sub i32 "     << left << ", " << right << "\n";
                else if (n->op == "*")  output << "  " << t << " = mul i32 "     << left << ", " << right << "\n";
                else if (n->op == "/")  output << "  " << t << " = sdiv i32 "    << left << ", " << right << "\n";
                else if (n->op == "==") output << "  " << t << " = icmp eq i32 " << left << ", " << right << "\n";
                else if (n->op == "!=") output << "  " << t << " = icmp ne i32 " << left << ", " << right << "\n";
                else if (n->op == "<")  output << "  " << t << " = icmp slt i32 "<< left << ", " << right << "\n";
                else if (n->op == ">")  output << "  " << t << " = icmp sgt i32 "<< left << ", " << right << "\n";
                else if (n->op == "<=") output << "  " << t << " = icmp sle i32 "<< left << ", " << right << "\n";
                else if (n->op == ">=") output << "  " << t << " = icmp sge i32 "<< left << ", " << right << "\n";

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

                // print()
                if (n->callee == "print") {
                    for (auto& arg : n->args) {
                        if (arg->type == NodeType::STRING_LITERAL) {
                            auto* s = static_cast<StringLiteral*>(arg.get());
                            int len = (int)s->value.size() + 2;
                            std::string lenStr = std::to_string(len);
                            std::string fmt = newTemp();
                            std::string fmtPtr = newTemp();
                            output << "  " << fmt << " = alloca [" << lenStr << " x i8]\n";
                            output << "  store [" << lenStr << " x i8] c\""
                                   << s->value << "\\0A\\00\", [" << lenStr << " x i8]* " << fmt << "\n";
                            output << "  " << fmtPtr << " = getelementptr ["
                                   << lenStr << " x i8], [" << lenStr << " x i8]* "
                                   << fmt << ", i32 0, i32 0\n";
                            output << "  call i32 (i8*, ...) @printf(i8* " << fmtPtr << ")\n";
                        } else {
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
                    }
                    std::string t = newTemp();
                    output << "  " << t << " = add i32 0, 0\n";
                    return t;
                }

                // input()
                if (n->callee == "input") {
                    std::string var = newTemp();
                    std::string fmt = newTemp();
                    std::string fmtPtr = newTemp();
                    output << "  " << var << " = alloca i32\n";
                    output << "  " << fmt << " = alloca [3 x i8]\n";
                    output << "  store [3 x i8] c\"%d\\00\", [3 x i8]* " << fmt << "\n";
                    output << "  " << fmtPtr << " = getelementptr [3 x i8], [3 x i8]* "
                           << fmt << ", i32 0, i32 0\n";
                    output << "  call i32 (i8*, ...) @scanf(i8* " << fmtPtr
                           << ", i32* " << var << ")\n";
                    std::string result = newTemp();
                    output << "  " << result << " = load i32, i32* " << var << "\n";
                    return result;
                }

                // Appel de méthode via nom manglé : ClassName__method
                std::vector<std::string> argRegs;
                for (auto& arg : n->args)
                    argRegs.push_back(genExpr(arg.get()));

                std::string t = newTemp();
                output << "  " << t << " = call i32 @" << n->callee << "(";
                for (int i = 0; i < (int)argRegs.size(); i++) {
                    output << "i32 " << argRegs[i];
                    if (i < (int)argRegs.size() - 1) output << ", ";
                }
                output << ")\n";
                return t;
            }

            case NodeType::MEMBER_ACCESS: {
                auto* n = static_cast<MemberAccess*>(node);
                auto& obj = objects[n->object];

                if (n->isCall) {
                    // Appel de méthode : génère un appel à ClassName__method
                    std::string mangledName = obj.className + "__" + n->member;
                    std::vector<std::string> argRegs;

                    // Premier argument = pointeur vers l'objet (this)
                    argRegs.push_back(obj.ptrReg);

                    for (auto& arg : n->args)
                        argRegs.push_back(genExpr(arg.get()));

                    std::string t = newTemp();
                    output << "  " << t << " = call i32 @" << mangledName << "(";
                    for (int i = 0; i < (int)argRegs.size(); i++) {
                        if (i == 0)
                            output << "%struct." << obj.className << "* " << argRegs[i];
                        else
                            output << "i32 " << argRegs[i];
                        if (i < (int)argRegs.size() - 1) output << ", ";
                    }
                    output << ")\n";
                    return t;
                } else {
                    // Accès champ
                    int idx = obj.fieldIndex[n->member];
                    std::string ptr = newTemp();
                    std::string val = newTemp();
                    output << "  " << ptr << " = getelementptr %struct." << obj.className
                           << ", %struct." << obj.className << "* " << obj.ptrReg
                           << ", i32 0, i32 " << idx << "\n";
                    output << "  " << val << " = load i32, i32* " << ptr << "\n";
                    return val;
                }
            }

            default:
                return "";
        }
    }

    // --- Génération des instructions ---

    void genStatement(Node* node) {
        if (!node) return;

        switch (node->type) {

            case NodeType::VAR_DECL: {
                auto* n = static_cast<VarDecl*>(node);
                std::string reg = "%" + n->name;
                output << "  " << reg << " = alloca i32\n";
                varRegs[n->name] = reg;
                std::string val = genExpr(n->value.get());
                output << "  store i32 " << val << ", i32* " << reg << "\n";
                break;
            }

            case NodeType::OBJECT_DECL: {
                auto* n = static_cast<ObjectDecl*>(node);
                std::string ptrReg = "%" + n->varName + "_ptr";

                // Alloue la struct
                output << "  " << ptrReg << " = alloca %struct." << n->className << "\n";

                // Appelle le constructeur
                std::string ctorName = n->className + "__ctor";
                std::vector<std::string> argRegs;
                argRegs.push_back(ptrReg);
                for (auto& arg : n->args)
                    argRegs.push_back(genExpr(arg.get()));

                output << "  call void @" << ctorName << "(";
                output << "%struct." << n->className << "* " << ptrReg;
                for (int i = 1; i < (int)argRegs.size(); i++)
                    output << ", i32 " << argRegs[i];
                output << ")\n";

                // Enregistre l'objet
                ObjectInfo info;
                info.className = n->className;
                info.ptrReg = ptrReg;
                if (structs.find(n->className) != structs.end()) {
                    auto& s = structs[n->className];
                    for (int i = 0; i < (int)s.fields.size(); i++)
                        info.fieldIndex[s.fields[i]] = i;
                }
                objects[n->varName] = info;
                break;
            }

            case NodeType::ASSIGN_EXPR: {
                auto* n = static_cast<AssignExpr*>(node);
                std::string val = genExpr(n->value.get());
                output << "  store i32 " << val << ", i32* " << varRegs[n->name] << "\n";
                break;
            }

            case NodeType::MEMBER_ASSIGN: {
                auto* n = static_cast<MemberAssign*>(node);
                auto& obj = objects[n->object];
                int idx = obj.fieldIndex[n->member];
                std::string val = genExpr(n->value.get());
                std::string ptr = newTemp();
                output << "  " << ptr << " = getelementptr %struct." << obj.className
                       << ", %struct." << obj.className << "* " << obj.ptrReg
                       << ", i32 0, i32 " << idx << "\n";
                output << "  store i32 " << val << ", i32* " << ptr << "\n";
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
                output << thenLbl << ":\n";
                genBlock(n->thenBlock.get());
                output << "  br label %" << mergeLbl << "\n";
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

                genStatement(n->init.get());
                output << "  br label %" << condLbl << "\n";
                output << condLbl << ":\n";
                std::string condReg = genExpr(n->condition.get());
                output << "  br i1 " << condReg << ", label %" << bodyLbl
                       << ", label %" << mergeLbl << "\n";
                output << bodyLbl << ":\n";
                genBlock(n->body.get());
                genStatement(n->increment.get());
                output << "  br label %" << condLbl << "\n";
                output << mergeLbl << ":\n";
                break;
            }

            case NodeType::CALL_EXPR:
            case NodeType::MEMBER_ACCESS: {
                genExpr(node);
                break;
            }

            default:
                break;
        }
    }

    void genBlock(Node* node) {
        auto* block = static_cast<Block*>(node);
        for (auto& stmt : block->statements)
            genStatement(stmt.get());
    }

    // Génère une fonction normale
    void genFuncDecl(FuncDecl* func) {
        varRegs.clear();

        output << "define i32 @" << func->name << "(";
        for (int i = 0; i < (int)func->params.size(); i++) {
            output << "i32 %" << func->params[i].name;
            if (i < (int)func->params.size() - 1) output << ", ";
        }
        output << ") {\n";
        output << "entry:\n";

        for (auto& param : func->params) {
            std::string reg = "%" + param.name + "_addr";
            output << "  " << reg << " = alloca i32\n";
            output << "  store i32 %" << param.name << ", i32* " << reg << "\n";
            varRegs[param.name] = reg;
        }

        genBlock(func->body.get());
        output << "  ret i32 0\n";
        output << "}\n\n";
    }

    // Génère une classe complète
    void genClassDecl(ClassDecl* cls) {
        // 1. Définit la struct LLVM
        output << "%struct." << cls->name << " = type { ";
        for (int i = 0; i < (int)cls->fields.size(); i++) {
            output << "i32";
            if (i < (int)cls->fields.size() - 1) output << ", ";
        }
        output << " }\n\n";

        // Enregistre la struct
        StructInfo sInfo;
        for (auto& f : cls->fields)
            sInfo.fields.push_back(f.name);
        structs[cls->name] = sInfo;

        // 2. Génère le constructeur
        if (cls->constructor) {
            auto& ctor = *cls->constructor;
            output << "define void @" << cls->name << "__ctor(%struct." << cls->name << "* %self";
            for (auto& p : ctor.params)
                output << ", i32 %" << p.name;
            output << ") {\n";
            output << "entry:\n";

            // Alloue les paramètres
            varRegs.clear();
            for (auto& p : ctor.params) {
                std::string reg = "%" + p.name + "_addr";
                output << "  " << reg << " = alloca i32\n";
                output << "  store i32 %" << p.name << ", i32* " << reg << "\n";
                varRegs[p.name] = reg;
            }

            // Initialise les champs avec les valeurs par défaut
            for (int i = 0; i < (int)cls->fields.size(); i++) {
                auto& field = cls->fields[i];
                std::string ptr = newTemp();
                output << "  " << ptr << " = getelementptr %struct." << cls->name
                       << ", %struct." << cls->name << "* %self, i32 0, i32 " << i << "\n";

                // Valeur par défaut
                std::string defaultVal = "0";
                if (field.defaultValue && field.defaultValue->type == NodeType::INT_LITERAL)
                    defaultVal = std::to_string(static_cast<IntLiteral*>(field.defaultValue.get())->value);

                output << "  store i32 " << defaultVal << ", i32* " << ptr << "\n";
            }

            // Applique les assignments du constructeur : -> field = param
            for (auto& assign : ctor.assignments) {
                // Trouve l'index du champ
                int fieldIdx = -1;
                for (int i = 0; i < (int)cls->fields.size(); i++) {
                    if (cls->fields[i].name == assign.first) {
                        fieldIdx = i;
                        break;
                    }
                }
                if (fieldIdx < 0) continue;

                std::string ptr = newTemp();
                std::string val = newTemp();
                output << "  " << ptr << " = getelementptr %struct." << cls->name
                       << ", %struct." << cls->name << "* %self, i32 0, i32 " << fieldIdx << "\n";
                output << "  " << val << " = load i32, i32* " << varRegs[assign.second] << "\n";
                output << "  store i32 " << val << ", i32* " << ptr << "\n";
            }

            // Corps du constructeur
            if (ctor.body)
                genBlock(ctor.body.get());

            output << "  ret void\n";
            output << "}\n\n";
        }

        // 3. Génère les méthodes
        for (auto& method : cls->methods) {
            varRegs.clear();
            objects.clear();

            std::string mangledName = cls->name + "__" + method->name;
            output << "define i32 @" << mangledName
                   << "(%struct." << cls->name << "* %self";
            for (auto& p : method->params)
                output << ", i32 %" << p.name;
            output << ") {\n";
            output << "entry:\n";

            // Enregistre les paramètres
            for (auto& p : method->params) {
                std::string reg = "%" + p.name + "_addr";
                output << "  " << reg << " = alloca i32\n";
                output << "  store i32 %" << p.name << ", i32* " << reg << "\n";
                varRegs[p.name] = reg;
            }

            // Enregistre 'self' comme objet courant
            ObjectInfo selfInfo;
            selfInfo.className = cls->name;
            selfInfo.ptrReg = "%self";
            for (int i = 0; i < (int)cls->fields.size(); i++)
                selfInfo.fieldIndex[cls->fields[i].name] = i;
            objects["self"] = selfInfo;

            genBlock(method->body.get());
            output << "  ret i32 0\n";
            output << "}\n\n";
        }
    }

public:
    std::string generate(Node* program) {
        auto* prog = static_cast<Program*>(program);

        output << "; UC Compiler — generated LLVM IR\n\n";
        output << "declare i32 @printf(i8*, ...)\n";
        output << "declare i32 @scanf(i8*, ...)\n\n";

        // Génère d'abord les classes (structs et méthodes)
        for (auto& decl : prog->declarations) {
            if (decl->type == NodeType::CLASS_DECL)
                genClassDecl(static_cast<ClassDecl*>(decl.get()));
        }

        // Puis les fonctions
        for (auto& decl : prog->declarations) {
            if (decl->type == NodeType::FUNC_DECL)
                genFuncDecl(static_cast<FuncDecl*>(decl.get()));
        }

        return output.str();
    }
};
