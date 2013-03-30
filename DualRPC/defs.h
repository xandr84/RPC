#pragma once

#include <string>
#include <list>
#include "boost/shared_ptr.hpp"
#include <boost/function.hpp>

namespace DualRPC
{

class Variant;
class ClientBase;
class AsioClientBase;
class AsioClientSession;
class AsioClient;
class FutureResult;
class ObjectsStorage;
struct IObject;

typedef unsigned int ObjectID;
typedef unsigned int RequestID;
typedef unsigned __int64 SessionID;

typedef boost::shared_ptr<ClientBase> ClientBasePtr;
typedef boost::shared_ptr<AsioClientSession> AsioClientSessionPtr;
typedef boost::shared_ptr<AsioClient> AsioClientPtr;
typedef boost::shared_ptr<IObject> IObjectPtr;
typedef boost::shared_ptr<FutureResult> FutureResultPtr;

typedef std::list<ObjectID> ObjectIDList;

typedef boost::function<Variant (const Variant&)> Callback;

using std::string;

}