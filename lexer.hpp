// lexer.hpp
#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>

enum class TokenType {
    // Mots-clés
    FUNC, VAR, RETURN, IF, ELSE, FOR, WHILE, PRINT, INPUT,
    CLASS, PUBLIC, PRIVATE, NEW,

    // Types
    TYPE_INT, TYPE_FLOAT, TYPE_STRING, TYPE_BOOL,

    // Littéraux
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    BOOL_LITERAL,

    // Identifiants
    IDENT,

    // Opérateurs
    PLUS, MINUS, STAR, SLASH,
    ASSIGN,
    EQ, NEQ, LT, GT, LEQ, GEQ,
    ARROW,
    COLON,      // :  (accès membre : a:method())
    DCOLON,     // :: (public: / private:)

    // Ponctuation
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    COMMA,

    // Spéciaux
    NEWLINE,
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};

class Lexer {
private:
    std::string source;
    int pos;
    int line;
    int column;

    char current() {
        return pos < (int)source.size() ? source[pos] : '\0';
    }

    void advance() {
        if (current() == '\n') { line++; column = 1; }
        else column++;
        pos++;
    }

    void skipWhitespace() {
        while (current() == ' ' || current() == '\t' || current() == '\r')
            advance();
    }

    void skipComment() {
        while (current() != '\n' && current() != '\0')
            advance();
    }

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

    Token readString() {
        int startCol = column;
        advance();
        std::string value;

        while (current() != '"' && current() != '\0') {
            value += current(); advance();
        }

        if (current() == '\0')
            throw std::runtime_error("UC Error: unterminated string at line " + std::to_string(line));

        advance();
        return { TokenType::STRING_LITERAL, value, line, startCol };
    }

    Token readIdent() {
        int startCol = column;
        std::string value;

        while (isalnum(current()) || current() == '_') {
            value += current(); advance();
        }

        if (value == "func")    return { TokenType::FUNC,        value, line, startCol };
        if (value == "var")     return { TokenType::VAR,         value, line, startCol };
        if (value == "return")  return { TokenType::RETURN,      value, line, startCol };
        if (value == "if")      return { TokenType::IF,          value, line, startCol };
        if (value == "else")    return { TokenType::ELSE,        value, line, startCol };
        if (value == "for")     return { TokenType::FOR,         value, line, startCol };
        if (value == "while")   return { TokenType::WHILE,       value, line, startCol };
        if (value == "true" ||
            value == "false")   return { TokenType::BOOL_LITERAL,value, line, startCol };
        if (value == "int")     return { TokenType::TYPE_INT,    value, line, startCol };
        if (value == "float")   return { TokenType::TYPE_FLOAT,  value, line, startCol };
        if (value == "string")  return { TokenType::TYPE_STRING, value, line, startCol };
        if (value == "bool")    return { TokenType::TYPE_BOOL,   value, line, startCol };
        if (value == "print")   return { TokenType::PRINT,       value, line, startCol };
        if (value == "input")   return { TokenType::INPUT,       value, line, startCol };
        if (value == "class")   return { TokenType::CLASS,       value, line, startCol };
        if (value == "public")  return { TokenType::PUBLIC,      value, line, startCol };
        if (value == "private") return { TokenType::PRIVATE,     value, line, startCol };
        if (value == "new")     return { TokenType::NEW,         value, line, startCol };

        return { TokenType::IDENT, value, line, startCol };
    }

public:
    Lexer(const std::string& source) : source(source), pos(0), line(1), column(1) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;

        while (current() != '\0') {
            skipWhitespace();

            if (current() == '\0') break;

            if (current() == '/' && pos + 1 < (int)source.size() && source[pos + 1] == '/') {
                skipComment(); continue;
            }

            if (current() == '\n') {
                tokens.push_back({ TokenType::NEWLINE, "\\n", line, column });
                advance(); continue;
            }

            if (isdigit(current())) { tokens.push_back(readNumber()); continue; }
            if (current() == '"')   { tokens.push_back(readString()); continue; }
            if (isalpha(current()) || current() == '_') { tokens.push_back(readIdent()); continue; }

            int startCol = column;
            char c = current(); advance();
            switch (c) {
                case '+': tokens.push_back({ TokenType::PLUS,  "+", line, startCol }); break;
                case '-':
                    if (current() == '>') {
                        advance();
                        tokens.push_back({ TokenType::ARROW, "->", line, startCol });
                    } else {
                        tokens.push_back({ TokenType::MINUS, "-", line, startCol });
                    }
                    break;
                case '*': tokens.push_back({ TokenType::STAR,  "*", line, startCol }); break;
                case '/': tokens.push_back({ TokenType::SLASH, "/", line, startCol }); break;
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
                case ':':
                    tokens.push_back({ TokenType::COLON, ":", line, startCol });
                    break;
                default:
                    throw std::runtime_error("UC Error: unknown character '" + std::string(1, c) +
                        "' at line " + std::to_string(line) + ", column " + std::to_string(startCol));
            }
        }

        tokens.push_back({ TokenType::END_OF_FILE, "", line, column });
        return tokens;
    }
};
