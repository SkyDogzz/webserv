#ifndef PARSER_HPP
#define PARSER_HPP

#include "JsonValue.hpp"
#include "lexer.hpp"
#include <cstdlib>

class Parser {
public:
    Parser(Lexer& lexer);
    ~Parser();
    JsonValue* parse();

private:
    Lexer& _lexer;
    Token _currentToken;

    void getNextToken(TokenType type);
    JsonValue* parseValue();
    JsonValue* parseObjet();
    JsonValue* parseArray();
};

#endif
