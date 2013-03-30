#include "stdafx.h"
#include "objects.h"
#include "transport.h"
#include "logger.h"

#include <boost/format.hpp>

namespace DualRPC
{

FutureResult::FutureResult() : 
	m_activated(false)
{
}

void FutureResult::execFirstItem()
{
	CallbackList::iterator it = m_callbackList.begin();
	if(it == m_callbackList.end())
		return;

	try
	{
		if(m_lastResult.isException())
			m_lastResult = it->second(m_lastResult);
		else
			m_lastResult = it->first(m_lastResult);
	}
	catch(std::exception &e)
	{
		m_lastResult = e;
	}
	m_callbackList.erase(it);
}

void FutureResult::addCallback(const Callback &cb)
{
	m_callbackList.push_back(CallbackList::value_type(cb, Callback()));
	if(m_activated)
		execFirstItem();
}

void FutureResult::addErrback(const Callback &eb)
{
	m_callbackList.push_back(CallbackList::value_type(Callback(), eb));
	if(m_activated)
		execFirstItem();
}

void FutureResult::addBoth(const Callback &cb, const Callback &eb)
{
	m_callbackList.push_back(CallbackList::value_type(cb, eb));
	if(m_activated)
		execFirstItem();
}

void FutureResult::addBoth(const Callback &cb)
{
	m_callbackList.push_back(CallbackList::value_type(cb, cb));
	if(m_activated)
		execFirstItem();
}

Variant FutureResult::callback(const Variant &result)
{
	m_activated = true;
	m_lastResult = result;
	while(!m_callbackList.empty())
	{
		execFirstItem();
	}
	return m_lastResult;
}

Variant FutureResult::errback(const Variant &error)
{
	return callback(error);
}

///////////////////////////////////////////////////////////////////////////////////

FutureResultList::FutureResultList() : 
	m_activated(0)
{
}

void FutureResultList::addResult(const FutureResultPtr &result)
{
	m_results.push_back(ResultPair(result, Variant()));
}

void FutureResultList::join(const Callback &callback)
{
	unsigned int i = 0;
	m_callback = callback;
	for(ResultList::iterator it = m_results.begin(); it != m_results.end(); ++it)
	{
		if(i++ >= m_activated)
		{
			it->first->addBoth(boost::bind(&FutureResultList::internalCallback, this, it, _1));
		}
	}
}

Variant FutureResultList::internalCallback(ResultList::iterator it, const Variant &v)
{
	it->second = v;
	if(++m_activated == m_results.size())
	{
		Variant result;
		for(ResultList::iterator j = m_results.begin(); j != m_results.end(); ++j)
		{
			result.add(j->second);
		}
		m_callback(result);
	}
	return v;
}

///////////////////////////////////////////////////////////////////////////////////
LocalObject::~LocalObject() 
{
}

void LocalObject::registerMethod(const string &name, const Callback &method)
{
	m_methods[name] = method;
}

Variant LocalObject::call(const string &name, const Variant &args, bool withResult, 
		float timeout, FutureResultPtr &written)
{
	if(m_methods.count(name) > 0)
	{
		Variant v = m_methods[name](args);
		if(m_written)
		{
			written = m_written;
			m_written.reset();
		}
		return v;
	}

	throw std::runtime_error((boost::format("Method '%1%' not exists") % name).str());
}

void LocalObject::returnWritten(const FutureResultPtr &written)
{
	m_written = written;
}

///////////////////////////////////////////////////////////////////////////////////
int RemoteObject::count = 0;

RemoteObject::RemoteObject(const RemoteObject &obj) :
	m_clientPtr(obj.m_clientPtr), m_id(obj.m_id)
{
	count++;
}

RemoteObject::RemoteObject(const ClientBasePtr &client, ObjectID id) :
	m_clientPtr(client), m_id(id)
{
	count++;
}

RemoteObject::~RemoteObject()
{
	count--;
	if(m_id != 0)
		m_clientPtr->destroyObject(m_id);
}

Variant RemoteObject::call(const string &name, const Variant &args, bool withResult,
	float timeout, FutureResultPtr &written)
{
	return m_clientPtr->call(m_id, name, args, withResult, timeout, written);
}

///////////////////////////////////////////////////////////////////////////////////
ObjectsStorage::ObjectsStorage() 
{
	m_nextObjectID = 100;
}

ObjectID ObjectsStorage::getNextObjectID()
{
	return m_nextObjectID++;
}

ObjectID ObjectsStorage::registerObject(const IObjectPtr &obj, const ClientBasePtr &client, bool global)
{
	ObjectID id = global ? 0 : getNextObjectID();
	m_objects[id] = obj;
	m_clientLocalObjects[client].push_back(id);
	return id;
}

Variant ObjectsStorage::localCall(ObjectID id, const string &name,
	const Variant &args, bool withResult, float timeout, FutureResultPtr &written)
{
	auto it = m_objects.find(id);
	if(it != m_objects.end())
	{
		try
		{
			return it->second->call(name, args, withResult, timeout, written);
		}
		catch(std::exception &e)
		{
			return Variant(e);
		}
		catch(...)
		{
			return Variant(std::runtime_error("Unknown exception")); 
		}
	}
	else 
	{
		return Variant(std::runtime_error(
			(boost::format("Object #%1% not registered") % id).str()));
	}
}

/*
bool ObjectsStorage::hasObjects(const Variant &v) const
{
	if(v.isObject())
		return true;
	else if(v.isArray())
	{
		for(auto it = v.getArray().cbegin(); it != v.getArray().cend(); ++it)
			if(hasObjects(*it)) 
				return true;
	}
	else if(v.isMap())
	{
		for(auto it = v.getMap().begin(); it != v.getMap().end(); ++it)
			if(hasObjects(it->second))
				return true;
	}
	return false;
}

bool ObjectsStorage::hasObjectIDs(const Variant &v) const
{
	if(v.isObjectID())
		return true;
	else if(v.isArray())
	{
		for(auto it = v.getArray().cbegin(); it != v.getArray().cend(); ++it)
			if(hasObjectIDs(*it)) 
				return true;
	}
	else if(v.isMap())
	{
		for(auto it = v.getMap().begin(); it != v.getMap().end(); ++it)
			if(hasObjectIDs(it->second))
				return true;
	}
	return false;
}

void ObjectsStorage::replaceObjectsToIDs(Variant &v, const ClientBasePtr &client)
{
	if(v.isObject())
	{
		v = Variant(registerObject(v.toObject(), client), true);
	}
	else if(v.isArray())
	{
		for(auto it = v.getArray().begin(); it != v.getArray().end(); ++it)
			replaceObjectsToIDs(*it, client);
	}
	else if(v.isMap())
	{
		for(auto it = v.getMap().begin(); it != v.getMap().end(); ++it)
			replaceObjectsToIDs(it->second, client);
	}
}

void ObjectsStorage::replaceIDsToObjects(Variant &v, const ClientBasePtr &client)
{
	if(v.isObjectID())
	{
		v = Variant(IObjectPtr(new RemoteObject(client, v.toObjectID())));
	}
	else if(v.isArray())
	{
		for(auto it = v.getArray().begin(); it != v.getArray().end(); ++it)
			replaceIDsToObjects(*it, client);
	}
	else if(v.isMap())
	{
		for(auto it = v.getMap().begin(); it != v.getMap().end(); ++it)
			replaceIDsToObjects(it->second, client);
	}
}
*/

Variant ObjectsStorage::IDtoObjectReplacer(const Variant &v, const ClientBasePtr &client)
{
	return IObjectPtr(new RemoteObject(client, v.toObjectID()));
}

Variant ObjectsStorage::objectToIDReplacer(const Variant &v, const ClientBasePtr &client)
{
	return Variant(registerObject(v.toObject(), client), true);
}

void ObjectsStorage::packVariant(std::ostream &stream, const Variant &v, const ClientBasePtr &client)
{
	v.pack(stream, boost::bind(&ObjectsStorage::objectToIDReplacer, this, _1, client));
}

void ObjectsStorage::unpackVariant(std::istream &stream, Variant &v, const ClientBasePtr &client)
{
	v.unpack(stream, boost::bind(&ObjectsStorage::IDtoObjectReplacer, this, _1, client));
}

void ObjectsStorage::freeClientObjects(const ClientBasePtr &client)
{
	ClientOwnedObjectMap::iterator f = m_clientLocalObjects.find(client);
	if(f != m_clientLocalObjects.end())
	{
		for(auto i = f->second.begin(); i != f->second.end(); ++i)
		{
			m_objects.erase(*i);
		}
		m_clientLocalObjects.erase(f);
	}	
}

void ObjectsStorage::deleteObject(ObjectID id)
{
	if(id == 0) return;

	auto f = m_objects.find(id);
	if(f == m_objects.end()) return;

	auto c = m_clientLocalObjects.begin();
	while(c != m_clientLocalObjects.end())
	{
		auto i = c->second.begin();
		while(i != c->second.end())
		{
			if(id == *i)
				i = c->second.erase(i);
			else ++i;
		}
		if(c->second.empty())
			c = m_clientLocalObjects.erase(c);
		else ++c;
	}
	m_objects.erase(f);
}


} // namespace