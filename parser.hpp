// parser.hpp
#pragma once
#include "lexer.hpp"
#include "ast.hpp"
#include <stdexcept>

class Parser {
private:
    std::vector<Token> tokens;
    int pos;

    Token current() { return tokens[pos]; }

    Token peek() {
        return pos + 1 < (int)tokens.size() ? tokens[pos + 1] : tokens[pos];
    }

    Token advance() {
        Token t = current();
        if (pos < (int)tokens.size() - 1) pos++;
        return t;
    }

    bool check(TokenType type) { return current().type == type; }

    void skipNewlines() {
        while (check(TokenType::NEWLINE)) advance();
    }

    Token expect(TokenType type, const std::string& errorMsg) {
        if (!check(type))
            throw std::runtime_error("UC Error at line " + std::to_string(current().line) +
                ", column " + std::to_string(current().column) + ": " + errorMsg);
        return advance();
    }

    // --- Parsing des expressions ---

    NodePtr parsePrimary() {
        Token t = current();

        if (check(TokenType::INT_LITERAL)) {
            advance();
            return std::make_unique<IntLiteral>(std::stoi(t.value), t.line, t.column);
        }

        if (check(TokenType::FLOAT_LITERAL)) {
            advance();
            return std::make_unique<FloatLiteral>(std::stof(t.value), t.line, t.column);
        }

        if (check(TokenType::STRING_LITERAL)) {
            advance();
            return std::make_unique<StringLiteral>(t.value, t.line, t.column);
        }

        if (check(TokenType::BOOL_LITERAL)) {
            advance();
            return std::make_unique<BoolLiteral>(t.value == "true", t.line, t.column);
        }

        if (check(TokenType::INPUT)) {
            advance();
            expect(TokenType::LPAREN, "expected '('");
            expect(TokenType::RPAREN, "expected ')'");
            return std::make_unique<CallExpr>("input", std::vector<NodePtr>(), t.line, t.column);
        }

        if (check(TokenType::IDENT)) {
            advance();

            // Accès membre : a:method() ou a:field
            if (check(TokenType::COLON)) {
                advance(); // consomme ':'
                Token member = expect(TokenType::IDENT, "expected member name after ':'");

                // Appel de méthode : a:speak()
                if (check(TokenType::LPAREN)) {
                    advance();
                    std::vector<NodePtr> args;
                    while (!check(TokenType::RPAREN)) {
                        args.push_back(parseExpr());
                        if (check(TokenType::COMMA)) advance();
                    }
                    expect(TokenType::RPAREN, "expected ')' after arguments");
                    return std::make_unique<MemberAccess>(t.value, member.value,
                        std::move(args), true, t.line, t.column);
                }

                // Accès champ : a:age
                return std::make_unique<MemberAccess>(t.value, member.value,
                    std::vector<NodePtr>(), false, t.line, t.column);
            }

            // Appel de fonction normal : add(1, 2)
            if (check(TokenType::LPAREN)) {
                advance();
                std::vector<NodePtr> args;
                while (!check(TokenType::RPAREN)) {
                    args.push_back(parseExpr());
                    if (check(TokenType::COMMA)) advance();
                }
                expect(TokenType::RPAREN, "expected ')' after arguments");
                return std::make_unique<CallExpr>(t.value, std::move(args), t.line, t.column);
            }

            return std::make_unique<Ident>(t.value, t.line, t.column);
        }

        if (check(TokenType::LPAREN)) {
            advance();
            NodePtr expr = parseExpr();
            expect(TokenType::RPAREN, "expected ')'");
            return expr;
        }

        throw std::runtime_error("UC Error at line " + std::to_string(t.line) +
            ", column " + std::to_string(t.column) + ": unexpected token '" + t.value + "'");
    }

    NodePtr parseUnary() {
        Token t = current();
        if (check(TokenType::MINUS) || current().value == "!") {
            std::string op = advance().value;
            return std::make_unique<UnaryExpr>(op, parseUnary(), t.line, t.column);
        }
        return parsePrimary();
    }

    NodePtr parseFactor() {
        NodePtr left = parseUnary();
        while (check(TokenType::STAR) || check(TokenType::SLASH)) {
            Token op = advance();
            NodePtr right = parseUnary();
            left = std::make_unique<BinaryExpr>(op.value, std::move(left),
                                                std::move(right), op.line, op.column);
        }
        return left;
    }

    NodePtr parseTerm() {
        NodePtr left = parseFactor();
        while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
            Token op = advance();
            NodePtr right = parseFactor();
            left = std::make_unique<BinaryExpr>(op.value, std::move(left),
                                                std::move(right), op.line, op.column);
        }
        return left;
    }

    NodePtr parseComparison() {
        NodePtr left = parseTerm();
        while (check(TokenType::EQ)  || check(TokenType::NEQ) ||
               check(TokenType::LT)  || check(TokenType::GT)  ||
               check(TokenType::LEQ) || check(TokenType::GEQ)) {
            Token op = advance();
            NodePtr right = parseTerm();
            left = std::make_unique<BinaryExpr>(op.value, std::move(left),
                                                std::move(right), op.line, op.column);
        }
        return left;
    }

    NodePtr parseExpr() { return parseComparison(); }

    // --- Parsing des instructions ---

    NodePtr parseReturn() {
        Token t = advance();
        if (check(TokenType::NEWLINE) || check(TokenType::END_OF_FILE))
            return std::make_unique<ReturnStmt>(nullptr, t.line, t.column);
        return std::make_unique<ReturnStmt>(parseExpr(), t.line, t.column);
    }

    NodePtr parseIf() {
        Token t = advance();
        NodePtr condition = parseExpr();
        NodePtr thenBlock = parseBlock();
        NodePtr elseBlock = nullptr;
        skipNewlines();
        if (check(TokenType::ELSE)) {
            advance();
            elseBlock = parseBlock();
        }
        return std::make_unique<IfStmt>(std::move(condition),
            std::move(thenBlock), std::move(elseBlock), t.line, t.column);
    }

    NodePtr parseWhile() {
        Token t = advance();
        NodePtr condition = parseExpr();
        NodePtr body = parseBlock();
        return std::make_unique<WhileStmt>(std::move(condition),
            std::move(body), t.line, t.column);
    }

    NodePtr parseFor() {
        Token t = advance();
        NodePtr init = parseVarDecl();
        expect(TokenType::NEWLINE, "expected newline after for init");
        NodePtr condition = parseExpr();
        expect(TokenType::NEWLINE, "expected newline after for condition");
        NodePtr increment = parseAssign();
        NodePtr body = parseBlock();
        return std::make_unique<ForStmt>(std::move(init), std::move(condition),
            std::move(increment), std::move(body), t.line, t.column);
    }

    NodePtr parseVarDecl() {
        Token t = advance();
        Token name = expect(TokenType::IDENT, "expected variable name after 'var'");
        expect(TokenType::ASSIGN, "expected '=' after variable name");
        NodePtr value = parseExpr();
        return std::make_unique<VarDecl>(name.value, std::move(value), "", t.line, t.column);
    }

    NodePtr parseAssign() {
        Token name = expect(TokenType::IDENT, "expected identifier");

        // Assignation membre : a:field = value
        if (check(TokenType::COLON)) {
            advance();
            Token member = expect(TokenType::IDENT, "expected member name");
            expect(TokenType::ASSIGN, "expected '='");
            NodePtr value = parseExpr();
            return std::make_unique<MemberAssign>(name.value, member.value,
                std::move(value), name.line, name.column);
        }

        expect(TokenType::ASSIGN, "expected '='");
        NodePtr value = parseExpr();
        return std::make_unique<AssignExpr>(name.value, std::move(value),
            name.line, name.column);
    }

    NodePtr parseBlock() {
        Token t = expect(TokenType::LBRACE, "expected '{'");
        std::vector<NodePtr> stmts;
        skipNewlines();
        while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
            stmts.push_back(parseStatement());
            skipNewlines();
        }
        expect(TokenType::RBRACE, "expected '}'");
        return std::make_unique<Block>(std::move(stmts), t.line, t.column);
    }

    NodePtr parseStatement() {
        skipNewlines();

        if (check(TokenType::RETURN)) return parseReturn();
        if (check(TokenType::IF))     return parseIf();
        if (check(TokenType::WHILE))  return parseWhile();
        if (check(TokenType::FOR))    return parseFor();
        if (check(TokenType::VAR))    return parseVarDecl();
        if (check(TokenType::PRINT))  return parsePrint();
        if (check(TokenType::INPUT))  return parseInput();

        // Instanciation d'objet : Animal a(5)
        // Détecte deux IDENT consécutifs suivis de '('
        if (check(TokenType::IDENT)) {
            Token t1 = tokens[pos];
            Token t2 = pos + 1 < (int)tokens.size() ? tokens[pos + 1] : tokens[pos];
            Token t3 = pos + 2 < (int)tokens.size() ? tokens[pos + 2] : tokens[pos];

            if (t2.type == TokenType::IDENT && t3.type == TokenType::LPAREN)
                return parseObjectDecl();

            // Assignation membre : a:field = value
            // Accès membre : a:method() ou a:field = value
            if (t2.type == TokenType::COLON) {
                Token t4 = pos + 3 < (int)tokens.size() ? tokens[pos + 3] : tokens[pos];
                // Si après a:membre il y a '(' c'est un appel de méthode → parseExpr
                // Si après a:membre il y a '=' c'est une assignation → parseAssign
                if (t4.type == TokenType::ASSIGN)
                    return parseAssign();
                else
                    return parseExpr(); // appel de méthode ou accès champ
            }

            // Assignation simple
            if (t2.type == TokenType::ASSIGN)
                return parseAssign();
        }

        return parseExpr();
    }

    NodePtr parseFuncDecl() {
        Token t = advance();
        Token name = expect(TokenType::IDENT, "expected function name after 'func'");
        expect(TokenType::LPAREN, "expected '(' after function name");

        std::vector<Param> params;
        while (!check(TokenType::RPAREN)) {
            Token paramType = advance();
            Token paramName = expect(TokenType::IDENT, "expected parameter name");
            params.push_back(Param(paramType.value, paramName.value,
                                   paramType.line, paramType.column));
            if (check(TokenType::COMMA)) advance();
        }
        expect(TokenType::RPAREN, "expected ')' after parameters");

        std::string returnType = "";
        if (check(TokenType::ARROW)) {
            advance();
            returnType = advance().value;
        }

        NodePtr body = parseBlock();
        return std::make_unique<FuncDecl>(name.value, std::move(params),
            std::move(body), returnType, t.line, t.column);
    }

    // Parse une déclaration de classe
    NodePtr parseClassDecl() {
        Token t = advance(); // consomme 'class'
        Token name = expect(TokenType::IDENT, "expected class name");

        // Héritage : class Chien : Animal
        std::string parent = "";
        if (check(TokenType::COLON)) {
            advance();
            Token parentName = expect(TokenType::IDENT, "expected parent class name");
            parent = parentName.value;
        }

        expect(TokenType::LBRACE, "expected '{'");
        skipNewlines();

        auto classDecl = std::make_unique<ClassDecl>(name.value, parent, t.line, t.column);

        bool isPublic = true; // public par défaut

        while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
            skipNewlines();

            // public: ou private:
            if (check(TokenType::PUBLIC)) {
                advance();
                expect(TokenType::COLON, "expected ':' after 'public'");
                isPublic = true;
                skipNewlines();
                continue;
            }

            if (check(TokenType::PRIVATE)) {
                advance();
                expect(TokenType::COLON, "expected ':' after 'private'");
                isPublic = false;
                skipNewlines();
                continue;
            }

            // Constructeur : Animal(int a) -> field = a { ... }
            if (check(TokenType::IDENT) && current().value == name.value) {
                classDecl->constructor = parseConstructor(name.value);
                skipNewlines();
                continue;
            }

            // Méthode : func speak() { ... }
            if (check(TokenType::FUNC)) {
                advance();
                Token methodName = expect(TokenType::IDENT, "expected method name");
                expect(TokenType::LPAREN, "expected '('");

                std::vector<Param> params;
                while (!check(TokenType::RPAREN)) {
                    Token paramType = advance();
                    Token paramName = expect(TokenType::IDENT, "expected parameter name");
                    params.push_back(Param(paramType.value, paramName.value,
                                          paramType.line, paramType.column));
                    if (check(TokenType::COMMA)) advance();
                }
                expect(TokenType::RPAREN, "expected ')'");

                std::string returnType = "";
                if (check(TokenType::ARROW)) {
                    advance();
                    returnType = advance().value;
                }

                NodePtr body = parseBlock();
                classDecl->methods.push_back(std::make_unique<MethodDecl>(
                    name.value, methodName.value, std::move(params),
                    std::move(body), returnType, isPublic, methodName.line, methodName.column));
                skipNewlines();
                continue;
            }

            // Champ : var age = 0
            if (check(TokenType::VAR)) {
                advance();
                Token fieldName = expect(TokenType::IDENT, "expected field name");
                expect(TokenType::ASSIGN, "expected '='");
                NodePtr defaultVal = parseExpr();
                classDecl->fields.push_back(
                    FieldDecl(fieldName.value, std::move(defaultVal), isPublic));
                skipNewlines();
                continue;
            }

            if (check(TokenType::NEWLINE)) { advance(); continue; }

            break;
        }

        expect(TokenType::RBRACE, "expected '}'");
        return classDecl;
    }

    // Parse un constructeur : Animal(int a) -> field = a { ... }
    std::unique_ptr<ConstructorDecl> parseConstructor(const std::string& className) {
        advance(); // consomme le nom de la classe
        expect(TokenType::LPAREN, "expected '('");

        std::vector<Param> params;
        while (!check(TokenType::RPAREN)) {
            Token paramType = advance();
            Token paramName = expect(TokenType::IDENT, "expected parameter name");
            params.push_back(Param(paramType.value, paramName.value,
                                   paramType.line, paramType.column));
            if (check(TokenType::COMMA)) advance();
        }
        expect(TokenType::RPAREN, "expected ')'");

        // Assignments : -> field1 = param1, field2 = param2
        std::vector<std::pair<std::string, std::string>> assignments;
        if (check(TokenType::ARROW)) {
            advance();
            do {
                Token field = expect(TokenType::IDENT, "expected field name");
                expect(TokenType::ASSIGN, "expected '='");
                Token param = expect(TokenType::IDENT, "expected parameter name");
                assignments.push_back({field.value, param.value});
                if (check(TokenType::COMMA)) advance();
                else break;
            } while (true);
        }

        // Corps optionnel
        NodePtr body = nullptr;
        if (check(TokenType::LBRACE)) {
            body = parseBlock();
        }

        return std::make_unique<ConstructorDecl>(std::move(params),
            std::move(assignments), std::move(body));
    }

    // Instanciation d'objet : Animal a(5)
    NodePtr parseObjectDecl() {
        Token className = advance(); // nom de la classe
        Token varName = advance();   // nom de la variable

        expect(TokenType::LPAREN, "expected '(' after object name");
        std::vector<NodePtr> args;
        while (!check(TokenType::RPAREN)) {
            args.push_back(parseExpr());
            if (check(TokenType::COMMA)) advance();
        }
        expect(TokenType::RPAREN, "expected ')'");

        return std::make_unique<ObjectDecl>(className.value, varName.value,
            std::move(args), className.line, className.column);
    }

    NodePtr parseInput() {
        Token t = advance();
        expect(TokenType::LPAREN, "expected '(' after 'input'");
        expect(TokenType::RPAREN, "expected ')' after '('");
        return std::make_unique<CallExpr>("input", std::vector<NodePtr>(), t.line, t.column);
    }

    NodePtr parsePrint() {
        Token t = advance();
        expect(TokenType::LPAREN, "expected '(' after 'print'");
        std::vector<NodePtr> args;
        while (!check(TokenType::RPAREN)) {
            args.push_back(parseExpr());
            if (check(TokenType::COMMA)) advance();
        }
        expect(TokenType::RPAREN, "expected ')' after print arguments");
        return std::make_unique<CallExpr>("print", std::move(args), t.line, t.column);
    }

public:
    Parser(std::vector<Token> tokens) : tokens(std::move(tokens)), pos(0) {}

    NodePtr parse() {
        std::vector<NodePtr> declarations;
        skipNewlines();

        while (!check(TokenType::END_OF_FILE)) {
            if (check(TokenType::FUNC))
                declarations.push_back(parseFuncDecl());
            else if (check(TokenType::VAR))
                declarations.push_back(parseVarDecl());
            else if (check(TokenType::CLASS))
                declarations.push_back(parseClassDecl());
            else
                throw std::runtime_error("UC Error at line " +
                    std::to_string(current().line) + ": expected 'func', 'var' or 'class'");
            skipNewlines();
        }

        return std::make_unique<Program>(std::move(declarations), 1, 1);
    }
};
