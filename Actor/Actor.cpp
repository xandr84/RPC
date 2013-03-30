// Actor.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include "logger.h"
#include "Actor.h"
#include "ActorObject.h"
#include "FileSystemObject.h"

ActorClient::ActorClient(boost::asio::io_service &iosvc, DualRPC::ObjectsStorage &storage) : 
	Client(iosvc, storage) 
{
}

void ActorClient::onStart() 
{
	ActorObject *pObj = new ActorObject;
	pObj->registerFactory("FileSystem", IFactoryPtr(new Factory<FileSystemObject>()));

	DualRPC::FutureResultPtr f = globalObject()->call("login", 
		DualRPC::Variant("login", int(0)).
		add("type", int(1)).
		add("name", "Actor").
		add("domain", "HOME").
		add("object", DualRPC::IObjectPtr(pObj)),
		true
	).toFuture();
	f->addBoth(boost::bind(&ActorClient::loginResult, this, _1),	
				boost::bind(&ActorClient::loginError, this, _1));
}

DualRPC::Variant ActorClient::loginResult(const DualRPC::Variant &ret)
{
	LOG_DEBUG_FMT(0, "Login success %1%", ret.repr());
	m_id = (unsigned int)ret.item("login", 0).toInt();
	m_serverObjPtr = ret.item("object").toObject();
	return DualRPC::Variant();
}

DualRPC::Variant ActorClient::loginError(const DualRPC::Variant &ret)
{
	LOG_DEBUG_FMT(0, "Login error %1%", ret.repr());
	return DualRPC::Variant();
}
