#include "parser.hpp"

Parser::Parser(Lexer &lexer) : _lexer(lexer), _hasError(false), _depth(0)
{
	_currentToken = _lexer.getNextToken();
	if (_currentToken.type == TOK_ERROR)
		_hasError = true;
}

Parser::~Parser()
{

}

JsonValue	*Parser::parse()
{
	JsonValue *root = parseValue();
	if (_hasError || _currentToken.type != TOK_EOF)
	{
		if (_currentToken.type != TOK_EOF && !_hasError)
		{
			std::cerr << "Unexpected extra content after JSON: " << _currentToken.value << std::endl;
			_hasError = true;
		}
		delete root;
		return NULL;
	}
	return root;
}

void	Parser::getNextToken(TokenType type)
{
	if (_hasError)
		return;
	if (_currentToken.type == type)
	{
		_currentToken = _lexer.getNextToken();
		if (_currentToken.type == TOK_ERROR)
		{
			std::cerr << "Lexer error: " << _currentToken.value << std::endl;
			_hasError = true;
		}
	}
	else
	{
		std::cerr << "Unexpected token: " << _currentToken.value << " (expected type " << type << ")" << std::endl;
		_hasError = true;
	}
}

JsonValue	*Parser::parseValue()
{
	if (_hasError) return NULL;
	if (_depth > MAX_DEPTH)
	{
		std::cerr << "Max nesting depth exceeded" << std::endl;
		_hasError = true;
		return NULL;
	}
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
			double val = std::atof(_currentToken.value.c_str());
			JsonValue *v = new JsonValue(val);
			getNextToken(TOK_NUMBER);
			return v;
		}
		case TOK_BOOL: {
			JsonValue *v = new JsonValue(_currentToken.value == "true" || _currentToken.value == "TRUE");
			getNextToken(TOK_BOOL);
			return v;
		}
		case TOK_NULL: {
			JsonValue *v = new JsonValue();
			getNextToken(TOK_NULL);
			return v;
		}
		default:
			std::cerr << "Unexpected token in value: " << _currentToken.value << std::endl;
			_hasError = true;
			return NULL;
	}
}

JsonValue	*Parser::parseObjet()
{
	_depth++;
	JsonValue *obj = new JsonValue(JSON_OBJET);
	getNextToken(TOK_LBRACE);


	while (!_hasError && _currentToken.type != TOK_RBRACE && _currentToken.type != TOK_EOF)
	{
		if (_currentToken.type == TOK_STRING)
		{
			std::string key = _currentToken.value;
			getNextToken(TOK_STRING);
			getNextToken(TOK_COLON);
			JsonValue *val = parseValue();
			if (!_hasError)
			{
				if (obj->getType() == JSON_OBJET && obj->asObject().count(key) > 0)
				{
					std::cerr << "Duplicate key in object: " << key << std::endl;
					_hasError = true;
					delete val;
				}
				else
					obj->addObjetMember(key, val);
			}
			else
				delete val;
		}
		else
		{
			std::cerr << "Expected string key in object, got: " << _currentToken.value << std::endl;
			_hasError = true;
			break;
		}

		if (_currentToken.type == TOK_COMMA)
		{
			getNextToken(TOK_COMMA);
			if (_currentToken.type == TOK_RBRACE)
			{
				std::cerr << "Trailing comma in object" << std::endl;
				_hasError = true;
			}
		}
		else if (_currentToken.type != TOK_RBRACE)
		{
			std::cerr << "Expected ',' or '}' in object, got: " << _currentToken.value << std::endl;
			_hasError = true;
		}
	}
	getNextToken(TOK_RBRACE);
	_depth--;
	if (_hasError)
	{
		delete obj;
		return NULL;
	}
	return obj;
}

JsonValue	*Parser::parseArray()
{
	_depth++;
	JsonValue *arr = new JsonValue(JSON_ARRAY);
	getNextToken(TOK_LBRACKET);
	
	while (!_hasError && _currentToken.type != TOK_RBRACKET && _currentToken.type != TOK_EOF)
	{
		JsonValue *val = parseValue();
		if (!_hasError && val)
			arr->addArrayElement(val);
		else
			delete val;

		if (_hasError) break;

		if (_currentToken.type == TOK_COMMA)
		{
			getNextToken(TOK_COMMA);
			if (_currentToken.type == TOK_RBRACKET)
			{
				std::cerr << "Trailing comma in array" << std::endl;
				_hasError = true;
			}
		}
		else if (_currentToken.type != TOK_RBRACKET)
		{
			std::cerr << "Expected ',' or ']' in array, got: " << _currentToken.value << std::endl;
			_hasError = true;
		}
	}
	getNextToken(TOK_RBRACKET);
	_depth--;
	if (_hasError)
	{
		delete arr;
		return NULL;
	}
	return arr;
}






