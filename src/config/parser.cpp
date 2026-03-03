#include "parser.hpp"

Parser::Parser(Lexer &lexer) : _lexer(lexer)
{
	_currentToken = _lexer.getNextToken();
}

void	Parser::getNextToken(TokenType type)
{
	if (_currentToken.type == type)
		_currentToken = _lexer.getNextToken();
	else
		std::cerr << "Unexpected token: " << _currentToken.value << std::endl;
}

JsonValue	*Parser::parseValue()
{
	switch (_currentToken.type)
	{
		case TOK_LBRACE: return parseObjet();
		case TOK_LBRACKET: return parseArray();
		case TOK_STRING: {
			JsonValue *v = new JsonValue(_currentToken.value);
			getNextToken(TOK_STRING);
			return v;
		}
		case TOK_NUMBER: {
			JsonValue *v = new JsonValue(_currentToken.value);
			getNextToken(TOK_NUMBER);
			return v;
		}
		case TOK_BOOL: {
			JsonValue *v = new JsonValue(_currentToken.value == "true");
			getNextToken(TOK_BOOL);
			return v;
		}
		case TOK_NULL: {
			JsonValue *v = new JsonValue();
			getNextToken(TOK_NULL);
			return v;
		}
		default:
			return NULL;
	}
}

JsonValue	*Parser::parseObjet()
{
	JsonValue *obj = new JsonValue();
	getNextToken(TOK_LBRACE);

	while (_currentToken.type != TOK_RBRACE && _currentToken.type != TOK_EOF)
	{
		if (_currentToken.type == TOK_STRING)
		{
			std::string key = _currentToken.value;
			getNextToken(TOK_STRING);
			getNextToken(TOK_COLON);
			JsonValue *val = parseValue();
			obj->addObjetMember(key, val);
		}
		if (_currentToken.type == TOK_COMMA)
			getNextToken(TOK_COMMA);
	}
	getNextToken(TOK_RBRACE);
	return obj;
}
