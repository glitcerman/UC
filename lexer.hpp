// lexer.hpp
#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>

// Tous les types de tokens possibles en UC
enum class TokenType {
    // Mots-clés
    FUNC, VAR, RETURN, IF, ELSE, FOR, WHILE, PRINT, INPUT,

    // Types
    TYPE_INT, TYPE_FLOAT, TYPE_STRING, TYPE_BOOL,

    // Littéraux (valeurs concrètes)
    INT_LITERAL,    // 42
    FLOAT_LITERAL,  // 3.14
    STRING_LITERAL, // "hello"
    BOOL_LITERAL,   // true / false

    // Identifiants (noms de variables, fonctions...)
    IDENT,

    // Opérateurs
    PLUS, MINUS, STAR, SLASH,  // + - * /
    ASSIGN,                     // =
    EQ, NEQ, LT, GT, LEQ, GEQ, // == != < > <= >=
    ARROW,                      // ->

    // Ponctuation
    LPAREN, RPAREN, // ( )
    LBRACE, RBRACE, // { }
    COMMA,          // ,

    // Spéciaux
    NEWLINE,        // fin de ligne (remplace ; )
    END_OF_FILE
};

// Un token = un type + sa valeur textuelle + sa position
struct Token {
    TokenType type;
    std::string value;
    int line;   // numéro de ligne (pour les messages d'erreur)
    int column; // numéro de colonne
};

class Lexer {
private:
    std::string source;  // le code source UC complet
    int pos;             // position actuelle dans le source
    int line;            // ligne actuelle
    int column;          // colonne actuelle

    // Retourne le caractère actuel
    char current() {
        return pos < source.size() ? source[pos] : '\0';
    }

    // Avance d'un caractère
    void advance() {
        if (current() == '\n') { line++; column = 1; }
        else column++;
        pos++;
    }

    // Ignore les espaces (mais pas les '\n' !)
    void skipWhitespace() {
        while (current() == ' ' || current() == '\t' || current() == '\r')
            advance();
    }

    // Ignore les commentaires // jusqu'à la fin de ligne
    void skipComment() {
        while (current() != '\n' && current() != '\0')
            advance();
    }

    // Lit un nombre entier ou flottant
    Token readNumber() {
        int startCol = column;
        std::string value;
        bool isFloat = false;

        while (isdigit(current())) { value += current(); advance(); }

        if (current() == '.') {
            isFloat = true;
            value += current(); advance();
            while (isdigit(current())) { value += current(); advance(); }
        }

        return { isFloat ? TokenType::FLOAT_LITERAL : TokenType::INT_LITERAL, value, line, startCol };
    }

    // Lit une chaîne de caractères "..."
    Token readString() {
        int startCol = column;
        advance(); // ignore le '"' ouvrant
        std::string value;

        while (current() != '"' && current() != '\0') {
            value += current(); advance();
        }

        if (current() == '\0')
            throw std::runtime_error("UC Error: unterminated string at line " + std::to_string(line));

        advance(); // ignore le '"' fermant
        return { TokenType::STRING_LITERAL, value, line, startCol };
    }

    // Lit un mot-clé ou identifiant
    Token readIdent() {
        int startCol = column;
        std::string value;

        while (isalnum(current()) || current() == '_') {
            value += current(); advance();
        }

        // Est-ce un mot-clé ?
        if (value == "func")   return { TokenType::FUNC,        value, line, startCol };
        if (value == "var")    return { TokenType::VAR,         value, line, startCol };
        if (value == "return") return { TokenType::RETURN,      value, line, startCol };
        if (value == "if")     return { TokenType::IF,          value, line, startCol };
        if (value == "else")   return { TokenType::ELSE,        value, line, startCol };
        if (value == "for")    return { TokenType::FOR,         value, line, startCol };
        if (value == "while")  return { TokenType::WHILE,       value, line, startCol };
        if (value == "true" ||
            value == "false")  return { TokenType::BOOL_LITERAL,value, line, startCol };
        if (value == "int")    return { TokenType::TYPE_INT,    value, line, startCol };
        if (value == "float")  return { TokenType::TYPE_FLOAT,  value, line, startCol };
        if (value == "string") return { TokenType::TYPE_STRING, value, line, startCol };
        if (value == "bool")   return { TokenType::TYPE_BOOL,   value, line, startCol };
        if (value == "print")  return { TokenType::PRINT,       value, line, startCol };

        // Sinon c'est un identifiant (nom de variable, fonction...)
        return { TokenType::IDENT, value, line, startCol };
    }

public:
    Lexer(const std::string& source) : source(source), pos(0), line(1), column(1) {}

    // Fonction principale : transforme tout le source en liste de tokens
    std::vector<Token> tokenize() {
        std::vector<Token> tokens;

        while (current() != '\0') {
            skipWhitespace();

            if (current() == '\0') break;

            // Commentaire
            if (current() == '/' && pos + 1 < source.size() && source[pos + 1] == '/') {
                skipComment(); continue;
            }

            // Fin de ligne
            if (current() == '\n') {
                tokens.push_back({ TokenType::NEWLINE, "\\n", line, column });
                advance(); continue;
            }

            // Nombre
            if (isdigit(current())) { tokens.push_back(readNumber()); continue; }

            // Chaîne
            if (current() == '"') { tokens.push_back(readString()); continue; }

            // Mot-clé ou identifiant
            if (isalpha(current()) || current() == '_') { tokens.push_back(readIdent()); continue; }

            // Opérateurs et ponctuation
            int startCol = column;
            char c = current(); advance();
            switch (c) {
                case '+': tokens.push_back({ TokenType::PLUS,   "+", line, startCol }); break;
                case '-':
                    if (current() == '>') {
                        advance();
                        tokens.push_back({ TokenType::ARROW, "->", line, startCol });
                    } else {
                        tokens.push_back({ TokenType::MINUS, "-", line, startCol });
                    }
                    break;
                case '*': tokens.push_back({ TokenType::STAR,   "*", line, startCol }); break;
                case '/': tokens.push_back({ TokenType::SLASH,  "/", line, startCol }); break;
                case '=':
                    if (current() == '=') {
                        advance();
                        tokens.push_back({ TokenType::EQ, "==", line, startCol });
                    } else {
                        tokens.push_back({ TokenType::ASSIGN, "=", line, startCol });
                    }
                    break;
                case '!':
                    if (current() == '=') {
                        advance();
                        tokens.push_back({ TokenType::NEQ, "!=", line, startCol });
                    }
                    break;
                case '<':
                    if (current() == '=') {
                        advance();
                        tokens.push_back({ TokenType::LEQ, "<=", line, startCol });
                    } else {
                        tokens.push_back({ TokenType::LT, "<", line, startCol });
                    }
                    break;
                case '>':
                    if (current() == '=') {
                        advance();
                        tokens.push_back({ TokenType::GEQ, ">=", line, startCol });
                    } else {
                        tokens.push_back({ TokenType::GT, ">", line, startCol });
                    }
                    break;
                case '(': tokens.push_back({ TokenType::LPAREN, "(", line, startCol }); break;
                case ')': tokens.push_back({ TokenType::RPAREN, ")", line, startCol }); break;
                case '{': tokens.push_back({ TokenType::LBRACE, "{", line, startCol }); break;
                case '}': tokens.push_back({ TokenType::RBRACE, "}", line, startCol }); break;
                case ',': tokens.push_back({ TokenType::COMMA,  ",", line, startCol }); break;
                default:
                    throw std::runtime_error("UC Error: unknown character '" + std::string(1, c) +
                        "' at line " + std::to_string(line) + ", column " + std::to_string(startCol));
            }
        }

        tokens.push_back({ TokenType::END_OF_FILE, "", line, column });
        return tokens;
    }
};