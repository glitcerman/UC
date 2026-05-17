// parser.hpp
#pragma once
#include "lexer.hpp"
#include "ast.hpp"
#include <stdexcept>

class Parser {
private:
    std::vector<Token> tokens;
    int pos;

    // Retourne le token actuel
    Token current() {
        return tokens[pos];
    }

    // Regarde le token suivant sans avancer
    Token peek() {
        return pos + 1 < tokens.size() ? tokens[pos + 1] : tokens[pos];
    }

    // Avance d'un token et retourne le précédent
    Token advance() {
        Token t = current();
        if (pos < tokens.size() - 1) pos++;
        return t;
    }

    // Vérifie si le token actuel est du bon type
    bool check(TokenType type) {
        return current().type == type;
    }

    // Ignore les sauts de ligne
    void skipNewlines() {
        while (check(TokenType::NEWLINE))
            advance();
    }

    // Consomme le token attendu, sinon erreur
    Token expect(TokenType type, const std::string& errorMsg) {
        if (!check(type))
            throw std::runtime_error("UC Error at line " + std::to_string(current().line) +
                ", column " + std::to_string(current().column) + ": " + errorMsg);
        return advance();
    }

    // --- Parsing des expressions ---

    // Niveau le plus bas : littéraux, identifiants, appels de fonction
    NodePtr parsePrimary() {
        Token t = current();

        // Nombre entier
        if (check(TokenType::INT_LITERAL)) {
            advance();
            return std::make_unique<IntLiteral>(std::stoi(t.value), t.line, t.column);
        }

        // Nombre flottant
        if (check(TokenType::FLOAT_LITERAL)) {
            advance();
            return std::make_unique<FloatLiteral>(std::stof(t.value), t.line, t.column);
        }

        // Chaîne de caractères
        if (check(TokenType::STRING_LITERAL)) {
            advance();
            return std::make_unique<StringLiteral>(t.value, t.line, t.column);
        }

        // Booléen
        if (check(TokenType::BOOL_LITERAL)) {
            advance();
            return std::make_unique<BoolLiteral>(t.value == "true", t.line, t.column);
        }

        // Identifiant ou appel de fonction
        if (check(TokenType::IDENT)) {
            advance();

            // Appel de fonction : add(1, 2)
            if (check(TokenType::LPAREN)) {
                advance(); // consomme '('
                std::vector<NodePtr> args;

                while (!check(TokenType::RPAREN)) {
                    args.push_back(parseExpr());
                    if (check(TokenType::COMMA)) advance();
                }

                expect(TokenType::RPAREN, "expected ')' after arguments");
                return std::make_unique<CallExpr>(t.value, std::move(args), t.line, t.column);
            }

            // input() utilisé comme expression
            if (check(TokenType::INPUT)) {
                Token t = advance();
                expect(TokenType::LPAREN, "expected '('");
                expect(TokenType::RPAREN, "expected ')'");
                return std::make_unique<CallExpr>("input", std::vector<NodePtr>(), t.line, t.column);
            }

            // Simple identifiant
            return std::make_unique<Ident>(t.value, t.line, t.column);
        }

        // Expression entre parenthèses : (a + b)
        if (check(TokenType::LPAREN)) {
            advance();
            NodePtr expr = parseExpr();
            expect(TokenType::RPAREN, "expected ')'");
            return expr;
        }

        throw std::runtime_error("UC Error at line " + std::to_string(t.line) +
            ", column " + std::to_string(t.column) + ": unexpected token '" + t.value + "'");
    }

    // Opérateurs unaires : -x, !b
    NodePtr parseUnary() {
        Token t = current();

        if (check(TokenType::MINUS) || current().value == "!") {
            std::string op = advance().value;
            return std::make_unique<UnaryExpr>(op, parseUnary(), t.line, t.column);
        }

        return parsePrimary();
    }

    // Opérateurs * et /
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

    // Opérateurs + et -
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

    // Opérateurs de comparaison : ==, !=, <, >, <=, >=
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

    // Expression complète (point d'entrée des expressions)
    NodePtr parseExpr() {
        return parseComparison();
    }

    // --- Parsing des instructions ---

    // return <expr>
    NodePtr parseReturn() {
        Token t = advance(); // consomme 'return'

        // return sans valeur (fonction void)
        if (check(TokenType::NEWLINE) || check(TokenType::END_OF_FILE)) {
            return std::make_unique<ReturnStmt>(nullptr, t.line, t.column);
        }

        return std::make_unique<ReturnStmt>(parseExpr(), t.line, t.column);
    }

    // if <condition> { ... } else { ... }
    NodePtr parseIf() {
        Token t = advance(); // consomme 'if'

        NodePtr condition = parseExpr();
        NodePtr thenBlock = parseBlock();
        NodePtr elseBlock = nullptr;

        skipNewlines();

        if (check(TokenType::ELSE)) {
            advance(); // consomme 'else'
            elseBlock = parseBlock();
        }

        return std::make_unique<IfStmt>(std::move(condition),
            std::move(thenBlock), std::move(elseBlock), t.line, t.column);
    }

    // while <condition> { ... }
    NodePtr parseWhile() {
        Token t = advance(); // consomme 'while'
        NodePtr condition = parseExpr();
        NodePtr body = parseBlock();

        return std::make_unique<WhileStmt>(std::move(condition),
            std::move(body), t.line, t.column);
    }

    // for <init> ; <condition> ; <increment> { ... }
    NodePtr parseFor() {
        Token t = advance(); // consomme 'for'

        NodePtr init = parseVarDecl();
        expect(TokenType::NEWLINE, "expected newline after for init");

        NodePtr condition = parseExpr();
        expect(TokenType::NEWLINE, "expected newline after for condition");

        NodePtr increment = parseAssign();

        NodePtr body = parseBlock();

        return std::make_unique<ForStmt>(std::move(init), std::move(condition),
            std::move(increment), std::move(body), t.line, t.column);
    }

    // var <name> = <expr>
    NodePtr parseVarDecl() {
        Token t = advance(); // consomme 'var'
        Token name = expect(TokenType::IDENT, "expected variable name after 'var'");
        expect(TokenType::ASSIGN, "expected '=' after variable name");
        NodePtr value = parseExpr();

        return std::make_unique<VarDecl>(name.value, std::move(value), "", t.line, t.column);
    }

    // <name> = <expr>
    NodePtr parseAssign() {
        Token name = expect(TokenType::IDENT, "expected identifier");
        expect(TokenType::ASSIGN, "expected '='");
        NodePtr value = parseExpr();

        return std::make_unique<AssignExpr>(name.value, std::move(value),
            name.line, name.column);
    }

    // { ... }
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

    // Détecte et parse la bonne instruction
    NodePtr parseStatement() {
        skipNewlines();

        if (check(TokenType::RETURN)) return parseReturn();
        if (check(TokenType::IF))     return parseIf();
        if (check(TokenType::WHILE))  return parseWhile();
        if (check(TokenType::FOR))    return parseFor();
        if (check(TokenType::VAR))    return parseVarDecl();
        if (check(TokenType::PRINT))  return parsePrint(); // <- NOUVEAU
        if (check(TokenType::INPUT))  return parseInput(); // <- NOUVEAU

        // Assignation ou appel de fonction
        if (check(TokenType::IDENT) && peek().type == TokenType::ASSIGN)
            return parseAssign();

        // Sinon c'est une expression seule (ex: appel de fonction)
        return parseExpr();
    }

    // func <name>(<params>) -> <type> { ... }
    NodePtr parseFuncDecl() {
        Token t = advance(); // consomme 'func'
        Token name = expect(TokenType::IDENT, "expected function name after 'func'");
        expect(TokenType::LPAREN, "expected '(' after function name");

        // Parsing des paramètres
        std::vector<Param> params;
        while (!check(TokenType::RPAREN)) {
            Token paramType = advance();
            Token paramName = expect(TokenType::IDENT, "expected parameter name");
            params.push_back(Param(paramType.value, paramName.value,
                                   paramType.line, paramType.column));
            if (check(TokenType::COMMA)) advance();
        }

        expect(TokenType::RPAREN, "expected ')' after parameters");

        // Type de retour optionnel : -> int
        std::string returnType = "";
        if (check(TokenType::ARROW)) {
            advance();
            returnType = advance().value;
        }

        NodePtr body = parseBlock();

        return std::make_unique<FuncDecl>(name.value, std::move(params),
            std::move(body), returnType, t.line, t.column);
    }

public:
    Parser(std::vector<Token> tokens) : tokens(std::move(tokens)), pos(0) {}

    // Point d'entrée : parse le programme entier
    NodePtr parse() {
        std::vector<NodePtr> declarations;
        skipNewlines();

        while (!check(TokenType::END_OF_FILE)) {
            if (check(TokenType::FUNC))
                declarations.push_back(parseFuncDecl());
            else if (check(TokenType::VAR))
                declarations.push_back(parseVarDecl());
            else
                throw std::runtime_error("UC Error at line " +
                    std::to_string(current().line) + ": expected 'func' or 'var'");

            skipNewlines();
        }

        return std::make_unique<Program>(std::move(declarations), 1, 1);
    }

    // input()
    NodePtr parseInput() {
        Token t = advance(); // consomme 'input'
        expect(TokenType::LPAREN, "expected '(' after 'input'");
        expect(TokenType::RPAREN, "expected ')' after '('");
        return std::make_unique<CallExpr>("input", std::vector<NodePtr>(), t.line, t.column);
    }

    // print(<expr>)
    NodePtr parsePrint() {
        Token t = advance(); // consomme 'print'
        expect(TokenType::LPAREN, "expected '(' after 'print'");

        std::vector<NodePtr> args;
        while (!check(TokenType::RPAREN)) {
            args.push_back(parseExpr());
            if (check(TokenType::COMMA)) advance();
        }

        expect(TokenType::RPAREN, "expected ')' after print arguments");
        return std::make_unique<CallExpr>("print", std::move(args), t.line, t.column);
    }
};