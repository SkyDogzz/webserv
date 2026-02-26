#ifndef LEXER_HPP
# define LEXER_HPP

enum TokenType {
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_LBRACKET,
	TOK_RBRACKET,
	TOK_COLON,
	TOK_COMMA,
	TOK_STRING,
	TOK_NUMBER,
	TOK_BOOL,
	TOK_NULL,
	TOK_EOF,
	TOK_ERROR
};

struct Token {
	TokenType	type;
	std::string	value;
};

class Lexer
{
public:
	Lexer(const char *input);
	Token getNextToken();
private:
	const char	*_start;
	const char	*_index;
};
