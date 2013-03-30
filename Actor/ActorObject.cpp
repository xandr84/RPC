#include "stdafx.h"
#include "logger.h"
#include "ActorObject.h"

ActorObject::ActorObject()
{
	registerMethod("enumFactories", boost::bind(&ActorObject::enumFactories, this, _1));
	registerMethod("createObject", boost::bind(&ActorObject::createObject, this, _1));
}

void ActorObject::registerFactory(const std::string &name, IFactoryPtr ptr)
{
	m_factories[name] = ptr;
}

DualRPC::Variant ActorObject::enumFactories(const DualRPC::Variant &args)
{
	DualRPC::Variant res;
	for(auto it = m_factories.cbegin(); it != m_factories.cend(); ++it)
		res.add(it->first);
	return res;
}

DualRPC::Variant ActorObject::createObject(const DualRPC::Variant &args)
{
	std::string name = args.toString();
	auto f = m_factories.find(name);
	if(f == m_factories.end())
	{
		throw std::runtime_error(
			(boost::format("Factory '%1%' not registered") % name).str());
	}
	return f->second->createObject();
}