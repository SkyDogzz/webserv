#ifndef LEXER_HPP
# define LEXER_HPP

# include <string>
# include <iostream>

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
	explicit Lexer(const char *input);
	void readFile(const char *filename);
	Token getNextToken();
	Token handleString();
	Token handleNumber();
	Token handleIdentifier();
private:
	std::string	_content;
	const char	*_start;
	const char	*_index;
};

#endif
