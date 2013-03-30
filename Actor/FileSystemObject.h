#pragma once

#include "objects.h"
#include <fstream>
#include <boost\chrono.hpp>

using boost::chrono::steady_clock;

class FileSystemObject : public DualRPC::LocalObject
{
public:
	FileSystemObject();

	DualRPC::Variant openFile(const DualRPC::Variant &args);
	/*
	������� �������� ������.
	������� ��������: map
		"path" : string - ���� � �����
		"mode" : int - ����� (��� ios_base::openmode 
					in = 0x01; out = 0x02; ate = 0x04; app = 0x08; trunc = 0x10;
					binary = 0x20; )
	�������� ��������: object
	*/

	DualRPC::Variant listDir(const DualRPC::Variant &args);
	/*
	����������� ���������� �������� �����.
	������� ��������: map
		"path" : string - ���� � �����
		"depth" : int - ������������ ������� �����������
	�������� ��������: array of map
		"type" : int - ��� ������� (1 - ����, 2 - �����, 3 - ����)
		"name" : string - ��� �������
		"size" : int - ������ ����� (� ����� � ������ ����� ���� ���)
		"children" : array of map - �������� ��������� �������� (� ������ ����� ���� ���)
	*/
};

class FileObject : public DualRPC::LocalObject
{
public:
	FileObject(const char *name, std::ios_base::openmode mode);

	DualRPC::Variant read(const DualRPC::Variant &args);
	/*
	������ ������ �� �����.
	������� ��������: map
		size:int=-1 - ������ �������� ������ (�� ��������� ���� ����)
		seek:int=-1 - �������, ������ ������������ ������ (�� ��������� � �������� �����)
		writer:object=null - ������ � ���������� �������, ������� ����� ���������� 
			� ���������� string, ���������� ���� ������ ������������� �������, ���������
			���-�� ���, ���� �� ����� �������� size ���� �����; 
			����� ����� ������� ������ ������ ������ 
			(�� ��������� ������� ��� � ������� ���������� ����������� ������ ������)
	�������� ��������: string - ����������� ������
	*/

	DualRPC::Variant write(const DualRPC::Variant &args);
	/*
	����� ������ � ����. � ������ ������ �������� �������� setWriteNotifier �����
	� ��������� ����������.
	������� ��������: map
		seek:int=-1 - �������, � ������� ���������� ������ (�� ��������� � �������)
		flush:int=0 - ����, ����������� ����� �� ��������� ����� ������ � ����
			(�� ��������� - ���)
		data:string - ������������ ������
	�������� ��������: int - ���-�� ���������� ����
	*/

	DualRPC::Variant setWriteNotifier(const DualRPC::Variant &args);
	/*
	������ ������ ��� ����������� ��� ������������� ������ ������, ����� ����� write
	���������� � ����������� �����.
	������� ��������: object - ������ � ���������� �������
	�������� ��������: ���
	*/

private:
	std::fstream m_stream;
	DualRPC::IObjectPtr m_writer, m_writeNotifier;
	DualRPC::FutureResultPtr m_readResult;

	DualRPC::Variant readBlock(__int64 size);
	DualRPC::Variant iterRead(__int64 restsize, __int64 lastsize = 0, 
		const steady_clock::time_point &lasttp = steady_clock::time_point(), 
		const DualRPC::Variant &v = DualRPC::Variant());
};