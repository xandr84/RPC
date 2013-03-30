#pragma once

#include <exception>
#include <vector>
#include <map>

#include "defs.h"

namespace DualRPC
{

class remote_error : public std::exception
{
public:
	explicit remote_error(const string& what_arg);
	explicit remote_error(const char* what_arg);

	virtual const char* what() const;

private:
	string m_text;
};

class Variant
{
public:
	enum Type
	{
		VT_NULL = 0,		//no value
		VT_INT = 1,			//m_int
		VT_REAL = 2,		//m_float
		VT_STRING = 3,		//m_string
		VT_ARRAY = 4,		//m_arrayPtr (delete on destroy)
		VT_MAP = 5,			//m_mapPtr (delete on destroy)
		VT_EXCEPTION = 6,	//m_stringPtr (exception text)
		VT_OBJECT = 7,		//m_objectPtr
		VT_OBJECTID = 8,	//m_id
		VT_FUTURE = 9,		//m_futurePtr
		VT_PACKED = 10
	};
	typedef std::vector<Variant> Array;
	typedef std::map<string, Variant> Map;

	Variant();
	Variant(int i);
	Variant(__int64 i);
	Variant(double f);
	Variant(const char *str);
	Variant(const string &s);
	Variant(const Array &a);
	Variant(const Map &m);
	Variant(const std::exception &e);
	Variant(std::size_t len, const Variant &v);
	Variant(const string &name, const Variant &v);
	Variant(IObjectPtr obj);
	Variant(ObjectID id, bool);
	Variant(FutureResultPtr future);
	Variant(std::istream &stream, int size);
	Variant(const Variant &v);
	Variant(Variant &&v);
	~Variant();

	Variant& add(const Variant &v);
	Variant& add(const string &name, const Variant &v);

	Variant item(unsigned int pos) const;
	Variant item(const string &name, const Variant &def = Variant()) const;

	Variant& operator=(const Variant &v);

	__int64 toInt(bool convert = false) const;
	double toReal(bool convert = false) const;
	string toString(bool convert = false) const;
	Array toArray(bool convert = false) const;
	Map toMap(bool convert = false) const;
	remote_error toException() const;
	IObjectPtr toObject() const;
	ObjectID toObjectID() const;
	FutureResultPtr toFuture() const;
	
	Array& getArray();
	Map& getMap();
	string& getString();

	const Array& getArray() const;
	const Map& getMap() const;
	const string& getString() const;

	Type type() const;
	const char* typeName() const;

	bool isNull() const;
	bool isInt() const;
	bool isReal() const;
	bool isString() const;
	bool isArray() const;
	bool isMap() const;
	bool isException() const;
	bool isObject() const;
	bool isObjectID() const;
	bool isFuture() const;
	bool isPacked() const;

	void pack(std::ostream &stream, const Callback &replacer = Callback()) const;
	Variant& unpack(const Callback &replacer = Callback());
	Variant& unpack(std::istream &stream, const Callback &replacer = Callback());
	string repr(unsigned int maxlen = 100) const;

private:
	Type m_type;

	union {
		__int64 m_int;
		double m_real;
		ObjectID m_id;
		string *m_stringPtr;
		Array *m_arrayPtr;
		Map *m_mapPtr;	
	};
	
	IObjectPtr m_objectPtr;	
	FutureResultPtr m_futurePtr;

	void clone(const Variant &v);	
	void swap(Variant &v);
	void free();
};

void packStr(std::ostream &stream, const string &s, int sizeLen = 4);
void unpackStr(std::istream &stream, string &s, int sizeLen = 4);

}

