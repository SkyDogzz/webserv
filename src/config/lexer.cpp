#include "lexer.hpp"
#include <fstream>
#include <sstream>
Lexer::Lexer(const char *input) : _start(0), _index(0)
{
	std::cout << "lexer class instanciated\n";
	readFile(input);
}

int	fileHasValidExtension(const char *filename)
{
	std::string name = filename;
	size_t pos = name.find_last_of('.');
	if (pos == std::string::npos)
		return 0;
	std::string ext = name.substr(pos);
	if (ext == ".json")
		return 1;
	else
		return 0;
}

void Lexer::readFile(const char *filename)
{
	if (!fileHasValidExtension(filename))
	{
		std::cerr << "Error: only .json file accepted" << std::endl;
		return ;
	}
	std::ifstream ifs(filename);
	if (!ifs.is_open())
	{
		std::cerr << "Error: file missing or invalid " << filename << std::endl;
		return ;
	}
	std::cout << filename << std::endl;
	std::string line;
	while (std::getline(ifs, line))
		_content += line + "\n";
	_start = _content.c_str();
	_index = _start;
	std::cout << _content << std::endl;
}

Token Lexer::handleString()
{
	Token	tok;
	tok.type = TOK_STRING;
	tok.value = "";

	_index++;
	while (*_index != '\0' && *_index != '"')
	{
		tok.value += *_index;
		_index++;
	}
	if (*_index == '"')
		_index++;
	else
	{
		tok.type = TOK_ERROR;
		tok.value = "Unterminated string";
	}
	return tok;
}

Token Lexer::getNextToken()
{
	while (*_index && isspace(*_index))
		_index++;
	if (*_index == '\0')
	{
		Token eof = {TOK_EOF, ""};
		return eof;
	}
	char current = *_index;
	Token tok;
	tok.value = current;
	switch (current)
	{
		case '{': tok.type = TOK_LBRACE; _index++; return tok;
		case '}': tok.type = TOK_RBRACE; _index++; return tok;
		case ':': tok.type = TOK_COLON; _index++; return tok;
		case ',': tok.type = TOK_COMMA; _index++; return tok;
		case '[': tok.type = TOK_LBRACKET; _index++; return tok;
		case ']': tok.type = TOK_RBRACKET; _index++; return tok;
	}
	if (current == '"')
		return handleString();
	tok.type = TOK_ERROR;
	tok.value = "Unknow character";
	return tok;
}
