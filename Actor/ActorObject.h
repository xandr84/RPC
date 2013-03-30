#pragma once

#include "objects.h"

struct IFactory
{
	virtual DualRPC::IObjectPtr createObject() = 0;
};

typedef boost::shared_ptr<IFactory> IFactoryPtr;

template <typename T> class Factory : public IFactory
{
public:
	DualRPC::IObjectPtr createObject() override { 
		return DualRPC::IObjectPtr(new T);
	}
};

class ActorObject : public DualRPC::LocalObject
{
public:
	ActorObject();

	void registerFactory(const std::string &name, IFactoryPtr ptr);

	DualRPC::Variant enumFactories(const DualRPC::Variant &args);
	/*
	����������� ��������� �������.
	������� ��������: null
	�������� ��������: array of string		
	*/

	DualRPC::Variant createObject(const DualRPC::Variant &args);
	/*
	������� ������ �������� �������.
	������� ��������: string
	�������� ��������: object	
	*/

private:
	typedef std::map<std::string, IFactoryPtr> FactoryPtrMap;

	FactoryPtrMap m_factories;
};