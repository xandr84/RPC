#pragma once

#include <boost/bimap.hpp>
#include <map>

#include "defs.h"
#include "variant.h"

namespace DualRPC
{

class FutureResult
{
public:
	FutureResult();

	void addCallback(const Callback &callback);
	void addErrback(const Callback &errback);
	void addBoth(const Callback &callback, const Callback &errback);
	void addBoth(const Callback &callback);

	Variant callback(const Variant &result);
	Variant errback(const Variant &error);

private:
	typedef std::list< std::pair<Callback, Callback> > CallbackList;

	bool m_activated;
	Variant m_lastResult;
	CallbackList m_callbackList;

	void execFirstItem();
};

class FutureResultList
{
public:
	FutureResultList();

	void addResult(const FutureResultPtr &result);
	void join(const Callback &callback);

private:
	typedef std::pair<FutureResultPtr, Variant> ResultPair;
	typedef std::list<ResultPair> ResultList;

	ResultList m_results;
	unsigned int m_activated;
	Callback m_callback;

	Variant internalCallback(ResultList::iterator it, const Variant &v);
};

struct IObject
{
	virtual Variant call(const string &name, const Variant &args, bool withResult = true, 
		float timeout = -1, FutureResultPtr &written = FutureResultPtr()) = 0;	
};

class LocalObject : public IObject
{
public:
	LocalObject() {}
	virtual ~LocalObject();

	void registerMethod(const string &name, const Callback &method);
	Variant call(const string &name, const Variant &args = Variant(), bool withResult = true, 
		float timeout = -1, FutureResultPtr &written = FutureResultPtr()) override;	

protected:
	void returnWritten(const FutureResultPtr &written);

private:
	typedef std::map<string, Callback> RemoteMethodMap;

	RemoteMethodMap m_methods;
	FutureResultPtr m_written;
};

class RemoteObject : public IObject
{
public:
	static int count;

	RemoteObject(const RemoteObject &obj);
	RemoteObject(const ClientBasePtr &client, ObjectID id);
	virtual ~RemoteObject();

	Variant call(const string &name, const Variant &args = Variant(), bool withResult = true, 
		float timeout = -1, FutureResultPtr &written = FutureResultPtr()) override;	

private:
	ClientBasePtr m_clientPtr;
	ObjectID m_id;
};

class ObjectsStorage
{
public:	
	ObjectsStorage();

	Variant localCall(ObjectID id, const string &name, const Variant &args, 
		bool withResult = true, float timeout = -1, FutureResultPtr &written = FutureResultPtr());

	ObjectID registerObject(const IObjectPtr &obj, const ClientBasePtr &owner = ClientBasePtr(), bool global = false);
	void deleteObject(ObjectID id);

	//bool hasObjects(const Variant &v) const;
	//bool hasObjectIDs(const Variant &v) const;

	//void replaceObjectsToIDs(Variant &v, const ClientBasePtr &client);
	//void replaceIDsToObjects(Variant &v, const ClientBasePtr &client);

	Variant IDtoObjectReplacer(const Variant &v, const ClientBasePtr &client);
	Variant objectToIDReplacer(const Variant &v, const ClientBasePtr &client);

	void packVariant(std::ostream &stream, const Variant &v, const ClientBasePtr &client);
	void unpackVariant(std::istream &stream, Variant &v, const ClientBasePtr &client);

	void freeClientObjects(const ClientBasePtr &client);

private:
	typedef std::map<ObjectID, IObjectPtr> ObjectMap;	
	typedef std::map<ClientBasePtr, ObjectIDList> ClientOwnedObjectMap;

	ObjectMap m_objects;
	ObjectID m_nextObjectID;
	ClientOwnedObjectMap m_clientLocalObjects;

	ObjectID getNextObjectID();	
};




}