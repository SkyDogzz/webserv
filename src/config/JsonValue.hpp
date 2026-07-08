#ifndef JSONVALUE_HPP
# define JSONVALUE_HPP

# include <string>
# include <vector>
# include <map>
# include <ostream>

/**
 *@brief all possible types in JSON file
*/

enum JsonType {
	JSON_NULL,
	JSON_BOOL,
	JSON_NUMBER,
	JSON_STRING,
	JSON_ARRAY,
	JSON_OBJET
};

class JsonValue
{
public:
	JsonValue();
	explicit JsonValue(bool b);
	explicit JsonValue(double n);
	explicit JsonValue(const std::string &s);
	JsonValue(const JsonValue &src);
	JsonValue& operator=(const JsonValue &rhs);
	~JsonValue();

	void	addObjetMember(const std::string &key, JsonValue *val);
	void	addArrayElement(JsonValue *val);
	JsonType getType() const;
	void	print(int indent = 0) const;
private:	
	JsonType _type;
	bool	_boolVal;
	double	_numberVal;
	std::string	*_stringVal;
	std::vector<JsonValue*>	*_arrayVal;

	//ok bon cette ligne est sombre, en gros c'est un map avec string comme cle
	//et un pointeur sur JsonValue comment valeur pour le cas ou un objet est imbrique dans un autre
	//ca va problement etre tres somnbre a faire compiler
	std::map<std::string, JsonValue*>	*_objectVal;

	void	_clear();
	void	_copy(const JsonValue &rhs);
};

#endif 
