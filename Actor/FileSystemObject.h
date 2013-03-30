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
	Создает файловый объект.
	Входной параметр: map
		"path" : string - путь к файлу
		"mode" : int - режим (как ios_base::openmode 
					in = 0x01; out = 0x02; ate = 0x04; app = 0x08; trunc = 0x10;
					binary = 0x20; )
	Выходной параметр: object
	*/

	DualRPC::Variant listDir(const DualRPC::Variant &args);
	/*
	Перечисляет содержимое заданной папки.
	Входной параметр: map
		"path" : string - путь к папке
		"depth" : int - максимальная глубина вложенности
	Выходной параметр: array of map
		"type" : int - тип объекта (1 - файл, 2 - папка, 3 - диск)
		"name" : string - имя объекта
		"size" : int - размер файла (у папок и дисков этого поля нет)
		"children" : array of map - перечень вложенных объектов (у файлов этого поля нет)
	*/
};

class FileObject : public DualRPC::LocalObject
{
public:
	FileObject(const char *name, std::ios_base::openmode mode);

	DualRPC::Variant read(const DualRPC::Variant &args);
	/*
	Читает данные из файла.
	Входной параметр: map
		size:int=-1 - размер читаемых данных (по умолчанию весь файл)
		seek:int=-1 - позиция, откуда производится чтение (по умолчанию с текущего места)
		writer:object=null - объект с безымянным методом, который будет вызываться 
			с параметром string, содержащим блок данных произвольного размера, некоторое
			кол-во раз, пока не будет передано size байт файла; 
			после этого функция вернет пустую строку 
			(по умолчанию объекта нет и функция возвращает запрошенную порцию данных)
	Выходной параметр: string - прочитанные данные
	*/

	DualRPC::Variant write(const DualRPC::Variant &args);
	/*
	Пишет данные в файл. В случае ошибок вызывает заданный setWriteNotifier метод
	и возващает исключение.
	Входной параметр: map
		seek:int=-1 - позиция, с которой происходит запись (по умолчанию с текущей)
		flush:int=0 - флаг, указывающий нужно ли выполнить сброс данных в файл
			(по умолчанию - нет)
		data:string - записываемые данные
	Выходной параметр: int - кол-во записанных байт
	*/

	DualRPC::Variant setWriteNotifier(const DualRPC::Variant &args);
	/*
	Задает объект для уведомления при возникновении ошибок записи, когда метод write
	вызывается в процедурном стиле.
	Входной параметр: object - объект с безымянным методом
	Выходной параметр: нет
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