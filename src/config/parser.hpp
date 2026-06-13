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
	bool _hasError;
	int  _depth;
	static const int MAX_DEPTH = 100;

	void	getNextToken(TokenType type);
	JsonValue	*parseValue();
	JsonValue	*parseObjet();
	JsonValue	*parseArray();
};

#endif
