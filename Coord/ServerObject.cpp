#include "StdAfx.h"
#include "ServerObject.h"
#include "logger.h"

GlobalServerObject::GlobalServerObject() : 
	m_nextClientID(1000)
{
	registerMethod("login", boost::bind(&GlobalServerObject::login, this, _1));
}

unsigned int GlobalServerObject::getNextClientID() 
{
	return m_nextClientID++;
}

DualRPC::Variant GlobalServerObject::login(const DualRPC::Variant &args)
{
	ClientInfo info;

	info.name = args.item("name").toString();
	info.domain = args.item("domain").toString();
	info.id = (unsigned int)args.item("login").toInt();
	info.type = ClientType(args.item("type").toInt());
	info.objectPtr = args.item("object").toObject();

	if(info.id == 0)
		info.id = getNextClientID();
	m_clients[info.id] = info;

	return DualRPC::Variant("login", (int)info.id).
		add("object", DualRPC::IObjectPtr(new ServerObject(shared_from_this())));
}

///////////////////////////////////////////////////////////////////////////
ServerObject::ServerObject(GlobalServerObjectPtr parent) : 
	m_parent(parent)
{	
	registerMethod("enumGroups", boost::bind(&ServerObject::enumGroups, this, _1));
	registerMethod("enumClients", boost::bind(&ServerObject::enumClients, this, _1));
	registerMethod("clientObject", boost::bind(&ServerObject::clientObject, this, _1));
}

ServerObject::~ServerObject()
{
	LOG_DEBUG(0, "ServerObject deleted");
}

DualRPC::Variant ServerObject::enumGroups(const DualRPC::Variant &args)
{
	return DualRPC::Variant();
}

DualRPC::Variant ServerObject::enumClients(const DualRPC::Variant &args)
{
	DualRPC::Variant res;
	unsigned int group = (unsigned int)args.toInt();
	for(auto it = m_parent->m_clients.begin(); it != m_parent->m_clients.end(); ++it)
	{
		res.add(DualRPC::Variant("id", (int)it->second.id).
			add("name", it->second.name).
			add("type", int(it->second.type)).
			add("domain", it->second.domain).
			add("group", 0)
		);		
	}
	return res;
}

DualRPC::Variant ServerObject::clientObject(const DualRPC::Variant &args)
{
	unsigned int id = (unsigned int)args.toInt();
	GlobalServerObject::ClientInfoMap::iterator it = m_parent->m_clients.find(id);
	return (it == m_parent->m_clients.end()) ? DualRPC::Variant() : it->second.objectPtr;
}
