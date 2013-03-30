#include "StdAfx.h"
#include "variant.h"
#include "logger.h"

#include <boost/format.hpp>

#include <sstream>

namespace DualRPC
{

remote_error::remote_error(const string& what_arg) : 
	m_text(what_arg)
{
}

remote_error::remote_error(const char* what_arg) : 
	m_text(what_arg)
{
}

const char* remote_error::what() const
{
	return m_text.c_str();
}

/////////////////////////////////////////////////////////////////

Variant::Variant() : 
	m_type(VT_NULL) 
{
}

Variant::Variant(int i) : 
	m_type(VT_INT), 
	m_int(i) 
{
}

Variant::Variant(__int64 i) : 
	m_type(VT_INT), 
	m_int(i) 
{
}

Variant::Variant(double f) : 
	m_type(VT_REAL),
	m_real(f)
{
}

Variant::Variant(const char *str) :
	m_type(VT_STRING),
	m_stringPtr(new string(str))
{
}

Variant::Variant(const string &s) :
	m_type(VT_STRING),
	m_stringPtr(new string(s))
{
}

Variant::Variant(const Array &a) : 
	m_type(VT_ARRAY),
	m_arrayPtr(new Array(a))
{
}

Variant::Variant(const Map &m) : 
	m_type(VT_MAP),
	m_mapPtr(new Map(m))
{
}

Variant::Variant(const std::exception &e) :
	m_type(VT_EXCEPTION),
	m_stringPtr(new string(e.what()))
{
}

Variant::Variant(std::size_t len, const Variant &v) : 
	m_type(VT_ARRAY)
{
	m_arrayPtr = new Array();
	m_arrayPtr->resize(len);
	if(len > 0)
		(*m_arrayPtr)[0] = v;
}

Variant::Variant(const string &name, const Variant &v) : 
	m_type(VT_MAP)
{
	m_mapPtr = new Map();
	m_mapPtr->insert(Map::value_type(name, v));
}

Variant::Variant(IObjectPtr obj) : 
	m_type(VT_OBJECT),
	m_objectPtr(obj)
{
}


Variant::Variant(ObjectID id, bool) : 
	m_type(VT_OBJECTID),
	m_id(id)
{
}

Variant::Variant(FutureResultPtr future) : 
	m_type(VT_FUTURE),
	m_futurePtr(future)
{
}

Variant::Variant(std::istream &stream, int size) : 
	m_type(VT_PACKED),
	m_stringPtr(new string)
{
	m_stringPtr->resize(size);
	stream.read(&(*m_stringPtr)[0], size);
}

Variant::Variant(const Variant &v) : 
	m_type(VT_NULL)
{
	clone(v);
}

Variant::Variant(Variant &&v) : 
	m_type(VT_NULL),
	m_int(0)
{
	swap(v);
}

Variant::~Variant()
{
	free();
}

void Variant::free()
{
	switch(m_type)
	{
	case VT_STRING:
	case VT_EXCEPTION:
		delete m_stringPtr;
		break;

	case VT_ARRAY:
		delete m_arrayPtr;
		break;

	case VT_MAP:
		delete m_mapPtr;
		break;
	}
	m_type = VT_NULL;
	m_int = 0;
}

void Variant::clone(const Variant &v)
{
	//LOG_DEBUG_FMT(0, "Variant::clone %1%", v.repr());
	free();
	m_type = v.m_type;
	switch(m_type)
	{
	case VT_NULL:
		break;
	case VT_INT:
		m_int = v.m_int;
		break;
	case VT_REAL:
		m_real = v.m_real;
		break;
	case VT_STRING:
	case VT_EXCEPTION:
		m_stringPtr = new string(*v.m_stringPtr);
		break;
	case VT_ARRAY:
		m_arrayPtr = new Array(*v.m_arrayPtr);
		break;
	case VT_MAP:
		m_mapPtr = new Map(*v.m_mapPtr);
		break;
	case VT_OBJECT:
		m_objectPtr = v.m_objectPtr;
		break;
	case VT_OBJECTID:
		m_id = v.m_id;
		break;
	case VT_FUTURE:
		m_futurePtr = v.m_futurePtr;
		break;
	}
}

void Variant::swap(Variant &v)
{
	//LOG_DEBUG_FMT(0, "Variant::swap %1% and %2%", repr() % v.repr());

	Type t = m_type;
	m_type = v.m_type;
	v.m_type = t;

	__int64 i = m_int;
	m_int = v.m_int;
	v.m_int = i;

	m_objectPtr.swap(v.m_objectPtr);
	m_futurePtr.swap(v.m_futurePtr);
}

Variant& Variant::add(const Variant &v)
{
	if(isNull())
	{
		m_type = VT_ARRAY;
		m_arrayPtr = new Array();
	}
	if(isArray())
	{
		m_arrayPtr->push_back(v);
	}	
	else throw std::runtime_error("Can not append item to non-array type");
	return *this;
}

Variant& Variant::add(const string &name, const Variant &v)
{
	if(isNull())
	{
		m_type = VT_MAP;
		m_mapPtr = new Map();
	}
	if(isMap())
	{
		m_mapPtr->insert(Map::value_type(name, v));
	}
	else throw std::runtime_error("Can not insert pair to non-map type");
	return *this;
}

Variant Variant::item(unsigned int pos) const
{
	if(isArray())
		return m_arrayPtr->at(pos);
	else
		throw std::runtime_error("Can not return item from non-array type");
}

Variant Variant::item(const string &name, const Variant &def) const
{
	if(isMap())
	{
		Map::iterator it = m_mapPtr->find(name);
		return (it == m_mapPtr->end()) ? def : it->second;
	}
	else
		throw std::runtime_error("Can not return item from non-map type");
}

Variant& Variant::operator=(const Variant &v)
{
	clone(v);
	return *this;
}

__int64 Variant::toInt(bool convert) const
{
	if(isInt())
		return m_int;

	if(convert)
	{
		if(isReal())
			return __int64(m_real);
		if(isString())
			return std::stoi(*m_stringPtr);
		if(isArray() && m_arrayPtr->size() > 0)
			return (*m_arrayPtr)[0].toInt();
		if(isMap() && m_mapPtr->size() > 0)
			return m_mapPtr->begin()->second.toInt();
	}

	throw std::runtime_error((boost::format("Error cast type '%1%' to 'int'") % typeName()).str());
}

double Variant::toReal(bool convert) const
{
	if(isReal())
		return m_real;

	if(convert)
	{
		if(isInt())
			return double(m_int);
		if(isString())
			return std::stod(*m_stringPtr);
		if(isArray() && m_arrayPtr->size() > 0)
			return (*m_arrayPtr)[0].toReal();
		if(isMap() && m_mapPtr->size() > 0)
			return m_mapPtr->begin()->second.toReal();
	}

	throw std::runtime_error((boost::format("Error cast type '%1%' to 'real'") % typeName()).str());
}

string Variant::toString(bool convert) const
{
	if(isNull())
		return string();

	if(isString())
		return *m_stringPtr;

	if(convert)
	{
		if(isInt())
			return std::to_string(m_int);
		if(isReal())
			return std::to_string((long double)m_real);
		if(isException())
			return *m_stringPtr;
		if(isArray() && m_arrayPtr->size() > 0)
			return (*m_arrayPtr)[0].toString();
		if(isMap() && m_mapPtr->size() > 0)
			return m_mapPtr->begin()->second.toString();
	}

	throw std::runtime_error((boost::format("Error cast type '%1%' to 'string'") % typeName()).str());
}

Variant::Array Variant::toArray(bool convert) const
{
	if(isArray())
		return *m_arrayPtr;

	if(convert)
		return Array(1, *this);

	throw std::runtime_error((boost::format("Error cast type '%1%' to 'array'") % typeName()).str());
}

Variant::Map Variant::toMap(bool convert) const
{
	if(isMap())
		return *m_mapPtr;

	if(convert)
	{
		Map m;
		m.insert(Map::value_type(string(), *this));
		return m;
	}

	throw std::runtime_error((boost::format("Error cast type '%1%' to 'map'") % typeName()).str());
}

remote_error Variant::toException() const
{
	if(isException())
		return remote_error(m_stringPtr->c_str());

	throw std::runtime_error((boost::format("Error cast type '%1%' to 'exception'") % typeName()).str());
}

IObjectPtr Variant::toObject() const
{
	if(isObject())
		return m_objectPtr;

	throw std::runtime_error((boost::format("Error cast type '%1%' to 'object'") % typeName()).str());
}

ObjectID Variant::toObjectID() const
{
	if(isObjectID())
		return m_id;

	throw std::runtime_error((boost::format("Error cast type '%1%' to 'id'") % typeName()).str());
}

FutureResultPtr Variant::toFuture() const
{
	if(isFuture())
		return m_futurePtr;

	throw std::runtime_error((boost::format("Error cast type '%1%' to 'future'") % typeName()).str());
}

Variant::Array& Variant::getArray()
{
	if(isArray())
		return *m_arrayPtr;

	throw std::runtime_error("Error accessing array ref in non-array type");
}

Variant::Map& Variant::getMap()
{
	if(isMap())
		return *m_mapPtr;

	throw std::runtime_error("Error accessing map ref in non-map type");
}

string& Variant::getString()
{
	if(isString())
		return *m_stringPtr;

	throw std::runtime_error("Error accessing string ref in non-string type");
}

const Variant::Array& Variant::getArray() const
{
	if(isArray())
		return *m_arrayPtr;

	throw std::runtime_error("Error accessing array ref in non-array type");
}

const Variant::Map& Variant::getMap() const
{
	if(isMap())
		return *m_mapPtr;

	throw std::runtime_error("Error accessing map ref in non-map type");
}

const string& Variant::getString() const
{
	if(isString())
		return *m_stringPtr;

	throw std::runtime_error("Error accessing string ref in non-string type");
}

Variant::Type Variant::type() const
{
	return m_type;
}

const char* Variant::typeName() const
{
	switch(m_type)
	{
	case VT_NULL: return "null";
	case VT_INT: return "int";
	case VT_REAL: return "real";
	case VT_STRING: return "string";
	case VT_ARRAY: return "array";
	case VT_MAP: return "map";
	case VT_EXCEPTION: return "exception";
	case VT_OBJECT: return "object";
	case VT_OBJECTID: return "id";
	}
	return "unknown";
}

bool Variant::isNull() const
{
	return m_type == VT_NULL;
}

bool Variant::isInt() const
{
	return m_type == VT_INT;
}

bool Variant::isReal() const
{
	return m_type == VT_REAL;
}

bool Variant::isString() const
{
	return m_type == VT_STRING;
}

bool Variant::isArray() const
{
	return m_type == VT_ARRAY;
}

bool Variant::isMap() const
{
	return m_type == VT_MAP;
}

bool Variant::isException() const
{
	return m_type == VT_EXCEPTION;
}

bool Variant::isObject() const
{
	return m_type == VT_OBJECT;
}

bool Variant::isObjectID() const
{
	return m_type == VT_OBJECTID;
}

bool Variant::isFuture() const
{
	return m_type == VT_FUTURE;
}

bool Variant::isPacked() const
{
	return m_type == VT_PACKED;
}

void packStr(std::ostream &stream, const string &s, int sizeLen)
{
	std::size_t len = s.size();
	if(sizeLen == 1) len = len > 0xff ? 0xff : len;
	else if(sizeLen == 2) len = len > 0xffff ? 0xffff : len;
	else sizeLen = 4;
	stream.write((const char*)&len, sizeLen);
	stream.write(s.c_str(), len);
}

void unpackStr(std::istream &stream, string &s, int sizeLen) 
{
	std::size_t len = 0;
	if(sizeLen == 1) len = len > 0xff ? 0xff : len;
	else if(sizeLen == 2) len = len > 0xffff ? 0xffff : len;
	else sizeLen = 4;
	stream.read((char*)&len, sizeLen);
	if(len > 0)
	{
		s.resize(len);
		stream.read(&s[0], len);
	}
}

void Variant::pack(std::ostream &stream, const Callback &replacer) const
{
	char type = m_type;

	switch(m_type)
	{
	case VT_NULL:
		stream.write(&type, sizeof(type));
		break;

	case VT_INT:
		stream.write(&type, sizeof(type));
		stream.write((const char*)&m_int, sizeof(m_int));
		break;

	case VT_REAL:
		stream.write(&type, sizeof(type));
		stream.write((const char*)&m_real, sizeof(m_real));
		break;

	case VT_STRING:
	case VT_EXCEPTION:
		stream.write(&type, sizeof(type));
		packStr(stream, *m_stringPtr, 4);
		break;

	case VT_ARRAY:
		{
			stream.write(&type, sizeof(type));
			std::size_t len = m_arrayPtr->size();
			stream.write((const char*)&len, sizeof(len));
			for(auto it = m_arrayPtr->cbegin(); it != m_arrayPtr->cend(); ++it)
				it->pack(stream, replacer);
		}
		break;

	case VT_MAP:
		{
			stream.write(&type, sizeof(type));
			std::size_t len = m_mapPtr->size();
			stream.write((const char*)&len, sizeof(len));
			for(auto it = m_mapPtr->cbegin(); it != m_mapPtr->cend(); ++it)
			{
				packStr(stream, it->first, 1);
				it->second.pack(stream, replacer);
			}
		}
		break;

	case VT_OBJECT:
		if(replacer.empty())
			throw std::runtime_error("Can not pack VT_OBJECT");
		replacer(*this).pack(stream);
		break;

	case VT_OBJECTID:
		stream.write(&type, sizeof(type));
		stream.write((const char*)&m_id, sizeof(m_id));
		break;	

	case VT_FUTURE:
		throw std::runtime_error("Can not pack VT_FUTURE");

	case VT_PACKED:
		stream.write(m_stringPtr->c_str(), m_stringPtr->length());
		break;
	}
}

Variant& Variant::unpack(const Callback &replacer)
{
	if(m_type != VT_PACKED)
		throw std::runtime_error("Can not unpack not VT_PACKED");

	std::istringstream stream(*m_stringPtr);
	return unpack(stream, replacer);
}

Variant& Variant::unpack(std::istream &stream, const Callback &replacer)
{
	free();

	char type;
	stream.read(&type, sizeof(type));
	m_type = Type(type);

	switch(m_type)
	{
	case VT_NULL:
		break;

	case VT_INT:
		stream.read((char*)&m_int, sizeof(m_int));
		break;

	case VT_REAL:
		stream.read((char*)&m_real, sizeof(m_real));
		break;

	case VT_STRING:
	case VT_EXCEPTION:
		m_stringPtr = new string();
		unpackStr(stream, *m_stringPtr, 4);
		break;

	case VT_ARRAY:
		{
			std::size_t len = 0;
			m_arrayPtr = new Array();
			stream.read((char*)&len, sizeof(len));
			m_arrayPtr->resize(len);
			for(auto it = m_arrayPtr->begin(); it != m_arrayPtr->end(); ++it)
				it->unpack(stream, replacer);
		}
		break;

	case VT_MAP:
		{
			std::size_t len = 0;
			m_mapPtr = new Map();
			stream.read((char*)&len, sizeof(len));
			for(std::size_t i = 0; i < len; i++)
			{
				string s;
				unpackStr(stream, s, 1);
				m_mapPtr->insert(Map::value_type(s, Variant().unpack(stream, replacer)));
			}
		}
		break;

	case VT_OBJECTID:
		stream.read((char*)&m_id, sizeof(m_id));
		if(!replacer.empty())
		{
			Variant v = replacer(*this);
			m_type = VT_OBJECT;
			m_objectPtr = v.toObject();
		}
		break;

	case VT_OBJECT:
		throw std::runtime_error("Can not unpack VT_OBJECT");
		break;

	case VT_FUTURE:
		throw std::runtime_error("Can not unpack VT_FUTURE");
		break;

	case VT_PACKED:
		throw std::runtime_error("Can not unpack VT_PACKED");
		break;
	}
	return *this;
}

string Variant::repr(unsigned int maxlen) const
{
	switch(m_type)
	{
	case VT_NULL:
		return "null";

	case VT_INT:
		return std::to_string(m_int);

	case VT_REAL:
		return std::to_string((long double)m_real);

	case VT_STRING:
		return (boost::format("string[%1%]=\"%2%\"%3%)") % 
			m_stringPtr->length() % m_stringPtr->substr(0, maxlen) % 
			(m_stringPtr->length() > maxlen ? "..." : "")).str();

	case VT_ARRAY:
		{
			string s("[");
			for(Array::const_iterator it = m_arrayPtr->cbegin(); it != m_arrayPtr->cend(); ++it)
			{
				if(it != m_arrayPtr->cbegin())
					s += ", ";
				s += it->repr();
			}
			return s + "]";
		}

	case VT_MAP:
		{
			string s("{");
			for(Map::const_iterator it = m_mapPtr->cbegin(); it != m_mapPtr->cend(); ++it)
			{
				if(it != m_mapPtr->cbegin())
					s += ", ";
				s += string("\"") + it->first + "\" : " + it->second.repr();
			}
			return s + "}";
		}

	case VT_OBJECT:
		return (boost::format("object(addr=0x%X)") % int(m_objectPtr.get())).str();

	case VT_OBJECTID:
		return (boost::format("object(id=%1%)") % m_id).str();

	case VT_EXCEPTION:
		return (boost::format("exception(\"%1%\"") % *m_stringPtr).str();

	case VT_FUTURE:
		return (boost::format("future(addr=0x%X)") % int(m_futurePtr.get())).str();

	case VT_PACKED:
		return (boost::format("packed[%1%]=\"%2%\"%3%)") % 
			m_stringPtr->length() % m_stringPtr->substr(0, maxlen) % 
			(m_stringPtr->length() > maxlen ? "..." : "")).str();
	}
	return string();
}

}
