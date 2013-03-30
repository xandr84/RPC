#pragma once

#include <string>
#include <list>
#include "boost/shared_ptr.hpp"
#include <boost/function.hpp>

namespace DualRPC
{

class Variant;
class ClientBase;
class ClientSession;
class Client;
class FutureResult;
class ObjectsStorage;
struct IObject;

typedef unsigned int ObjectID;
typedef unsigned int RequestID;
typedef unsigned __int64 SessionID;

typedef boost::shared_ptr<ClientBase> ClientBasePtr;
typedef boost::shared_ptr<ClientSession> ClientSessionPtr;
typedef boost::shared_ptr<Client> ClientPtr;
typedef boost::shared_ptr<IObject> IObjectPtr;
typedef boost::shared_ptr<FutureResult> FutureResultPtr;

typedef std::list<ObjectID> ObjectIDList;

typedef boost::function<Variant (const Variant&)> Callback;

using std::string;

}