#include "JsonValue.hpp"

JsonValue::JsonValue() : _type(JSON_NULL), _boolVal(false), _numberVal(0), 
			_stringVal(NULL), _arrayVal(NULL), _objectVal(NULL) {}

JsonValue::JsonValue(const std::string &s) : _type(JSON_STRING), _boolVal(false), _numberVal(0),
						_arrayVal(NULL), _objectVal(NULL) {
	_stringVal = new std::string(s);
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
