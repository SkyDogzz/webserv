#include "JsonValue.hpp"
#include <iostream>

JsonValue::JsonValue() : _type(JSON_NULL), _boolVal(false), _numberVal(0), 
			_stringVal(NULL), _arrayVal(NULL), _objectVal(NULL) {}

JsonValue::JsonValue(JsonType t) : _type(t), _boolVal(false), _numberVal(0), 
			_stringVal(NULL), _arrayVal(NULL), _objectVal(NULL) {
	if (t == JSON_ARRAY) _arrayVal = new std::vector<JsonValue*>();
	if (t == JSON_OBJET) _objectVal = new std::map<std::string, JsonValue*>();
}

JsonValue::JsonValue(const std::string &s) : _type(JSON_STRING), _boolVal(false), _numberVal(0),
						_arrayVal(NULL), _objectVal(NULL) {
	_stringVal = new std::string(s);
}

JsonValue::JsonValue(bool b) : _type(JSON_BOOL), _boolVal(b), _numberVal(0),
				_stringVal(NULL), _arrayVal(NULL), _objectVal(NULL)
{
}

JsonValue::JsonValue(double n) : _type(JSON_NUMBER), _boolVal(false), _numberVal(n),
				_stringVal(NULL), _arrayVal(NULL), _objectVal(NULL)
{
}

JsonValue::~JsonValue()
{
	_clear();
}

void JsonValue::_clear()
{
	if (_stringVal) delete _stringVal;
	if (_arrayVal)
	{
		for (size_t i = 0; i < _arrayVal->size(); ++i)
			delete (*_arrayVal)[i];
		delete _arrayVal;
	}
	if (_objectVal)
	{
		std::map<std::string, JsonValue*>::iterator it;
		for (it = _objectVal->begin(); it != _objectVal->end(); ++it)
			delete it->second;
		delete _objectVal;
	}
	_stringVal = NULL;
	_arrayVal = NULL;
	_objectVal = NULL;
}

void	JsonValue::addArrayElement(JsonValue *val)
{
	if (!_arrayVal) {
		_arrayVal = new std::vector<JsonValue*>();
		_type = JSON_ARRAY;
	}
	_arrayVal->push_back(val);
}

void	JsonValue::addObjetMember(const std::string &key, JsonValue *val)
{
	if (!_objectVal) {
		_objectVal = new std::map<std::string, JsonValue*>();
		_type = JSON_OBJET;
	}
	(*_objectVal)[key] = val;
}

JsonType JsonValue::getType() const {
	return _type;
}

bool JsonValue::asBool() const {
	return _boolVal;
}

double JsonValue::asNumber() const {
	return _numberVal;
}

const std::string &JsonValue::asString() const {
	static const std::string empty = "";
	if (_stringVal)
		return *_stringVal;
	return empty;
}

const std::vector<JsonValue*> &JsonValue::asArray() const {
	static const std::vector<JsonValue*> empty;
	if (_arrayVal)
		return *_arrayVal;
	return empty;
}

const std::map<std::string, JsonValue*> &JsonValue::asObject() const {
	static const std::map<std::string, JsonValue*> empty;
	if (_objectVal)
		return *_objectVal;
	return empty;
}

void	JsonValue::print(int indent) const {
	std::string spaces(indent, ' ');
    switch (_type) {
        case JSON_NULL:   std::cout << "null"; break;
        case JSON_BOOL:   std::cout << (_boolVal ? "true" : "false"); break;
        case JSON_NUMBER: std::cout << _numberVal; break;
        case JSON_STRING: std::cout << "\"" << *_stringVal << "\""; break;
        case JSON_ARRAY:
            std::cout << "[\n";
            for (size_t i = 0; i < _arrayVal->size(); ++i) {
                std::cout << spaces << "  ";
                (*_arrayVal)[i]->print(indent + 2);
                if (i < _arrayVal->size() - 1) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << spaces << "]";
            break;
        case JSON_OBJET:
            std::cout << "{\n";
            std::map<std::string, JsonValue*>::iterator it;
            for (it = _objectVal->begin(); it != _objectVal->end(); ++it) {
                std::cout << spaces << "  \"" << it->first << "\": ";
                it->second->print(indent + 2);
                std::map<std::string, JsonValue*>::iterator next = it;
                if (++next != _objectVal->end()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << spaces << "}";
            break;
    }
}

bool JsonValue::isValid(const JsonValue *val) {
	if (!val)
		return false;
	if (val->getType() == JSON_OBJET) {
		const std::map<std::string, JsonValue*> &obj = val->asObject();
		if (obj.empty()) return false;
	}
	return true;
}
