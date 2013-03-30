#pragma once

#include "objects.h"
#include <boost/enable_shared_from_this.hpp>

class GlobalServerObject : public DualRPC::LocalObject, 
						public boost::enable_shared_from_this<GlobalServerObject>
{
public:
	GlobalServerObject();

	DualRPC::Variant login(const DualRPC::Variant &args);
	/*
	Первый метод, который вызывает каждый клиент при подключении.
	Входной параметр: map
		"login" : int - выданный ранее идентификатор (если не указан или равен 0, то сервер вернет в ответ)
		"type" : int - CT_ACTOR = 1, CT_COORD = 2, CT_MANAGER = 3
		"name" : string - имя компьютера, используется при первоначальной регистрации
					(если с прошлого раза не изменилось, то не указывается)
		"domain" : string - домен компьютера, используется при первоначальной регистрации
		"object: : object - ссылка на клиентский объект для выполнения обратных вызовов
	Выходной параметр: map
		"login" : int - выданный идентификатор клиента (если не был указан)
		"object" : object - серверный объект для взаимодействия с данным клиентом
					(соответствует типу подключившиегося клиента)
	Исключения:
	*/

private:
	enum ClientType
	{
		CT_ACTOR = 1, CT_COORD = 2, CT_MANAGER = 3
	};
	struct ClientInfo
	{
		unsigned int id;
		ClientType type;
		std::string name, domain;
		DualRPC::IObjectPtr objectPtr;
	};
	typedef std::map<unsigned int, ClientInfo> ClientInfoMap;
	friend class ServerObject;

	ClientInfoMap m_clients;
	unsigned int m_nextClientID;

	unsigned int getNextClientID();
};

typedef boost::shared_ptr<GlobalServerObject> GlobalServerObjectPtr;

class ServerObject : public DualRPC::LocalObject
{
public:
	ServerObject(GlobalServerObjectPtr parent);
	~ServerObject();

	DualRPC::Variant enumGroups(const DualRPC::Variant &args);
	/*
	Возвращает список групп, зарегистрированных на сервере.
	Входной параметр: int - указывает к какой группе принадлежат подгруппы (если 0, то к корневой, если -1, то все подряд)
	Выходной параметр: array of map
		"id" : int
		"name" : string
	*/

	DualRPC::Variant enumClients(const DualRPC::Variant &args);
	/*
	Возвращает список клиентов, зарегистрированных на сервере.
	Входной параметр: int - указывает к какой группе принадлежат клиенты (если 0, то к корневой, если -1, то все подряд)
	Выходной параметр: array of map
		"id" : int - номер клиента
		"name" : string - имя
		"type" : int - тип (CT_ACTOR = 1, CT_COORD = 2, CT_MANAGER = 3)
		"domain" : string - рабочая группа\домен
		"group" : int - номер группы
		"groupName" : string - имя группы (если на вход подали -1, иначе поля не будет)
	*/

	DualRPC::Variant clientObject(const DualRPC::Variant &args);
	/*
	Возвращает клиентский объект, с помощью которого можно управлять клиентом.
	Входной параметр: int - номер клиента
	Выходной параметр: object
	*/

private:
	GlobalServerObjectPtr m_parent;
};

typedef boost::shared_ptr<ServerObject> ServerObjectPtr;
