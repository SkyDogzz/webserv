#ifndef PARSER_HPP
# define PARSER_HPP

# include "lexer.hpp"
# include <cstdlib>
# include "JsonValue.hpp"

class Parser
{
public:
	Parser(Lexer &lexer);
	~Parser();
	JsonValue *parse();
private:
	Lexer &_lexer;
	Token _currentToken;

	void	getNextToken(TokenType type);
	JsonValue	*parseValue();
	JsonValue	*parseObjet();
	JsonValue	*parseArray();
};

#endif
