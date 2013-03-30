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
	������ �����, ������� �������� ������ ������ ��� �����������.
	������� ��������: map
		"login" : int - �������� ����� ������������� (���� �� ������ ��� ����� 0, �� ������ ������ � �����)
		"type" : int - CT_ACTOR = 1, CT_COORD = 2, CT_MANAGER = 3
		"name" : string - ��� ����������, ������������ ��� �������������� �����������
					(���� � �������� ���� �� ����������, �� �� �����������)
		"domain" : string - ����� ����������, ������������ ��� �������������� �����������
		"object: : object - ������ �� ���������� ������ ��� ���������� �������� �������
	�������� ��������: map
		"login" : int - �������� ������������� ������� (���� �� ��� ������)
		"object" : object - ��������� ������ ��� �������������� � ������ ��������
					(������������� ���� ���������������� �������)
	����������:
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
	���������� ������ �����, ������������������ �� �������.
	������� ��������: int - ��������� � ����� ������ ����������� ��������� (���� 0, �� � ��������, ���� -1, �� ��� ������)
	�������� ��������: array of map
		"id" : int
		"name" : string
	*/

	DualRPC::Variant enumClients(const DualRPC::Variant &args);
	/*
	���������� ������ ��������, ������������������ �� �������.
	������� ��������: int - ��������� � ����� ������ ����������� ������� (���� 0, �� � ��������, ���� -1, �� ��� ������)
	�������� ��������: array of map
		"id" : int - ����� �������
		"name" : string - ���
		"type" : int - ��� (CT_ACTOR = 1, CT_COORD = 2, CT_MANAGER = 3)
		"domain" : string - ������� ������\�����
		"group" : int - ����� ������
		"groupName" : string - ��� ������ (���� �� ���� ������ -1, ����� ���� �� �����)
	*/

	DualRPC::Variant clientObject(const DualRPC::Variant &args);
	/*
	���������� ���������� ������, � ������� �������� ����� ��������� ��������.
	������� ��������: int - ����� �������
	�������� ��������: object
	*/

private:
	GlobalServerObjectPtr m_parent;
};

typedef boost::shared_ptr<ServerObject> ServerObjectPtr;
