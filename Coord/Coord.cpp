// Coord.cpp: ���������� ����� ����� ��� ����������� ����������.
//

#include "stdafx.h"
#include "transport.h"
#include "logger.h"
#include "ServerObject.h"
#include "sqlite3.h"

#include <fstream>
#include <iostream>

using namespace DualRPC;
using namespace std;

ofstream coordLog("coord.log");

void initLogger()
{
	locale::global(locale(locale::classic(), "", locale::ctype));

	Logger &log = Logger::instance();
	log.registerSender(0, "Main", 
		ILogSinkPtr(new LineFormatter(
			ILogSinkPtr(new LogStreamSinkAdapter<ostream>(cout)),
			LineFormatter::LF_TEXT | LineFormatter::LF_ENDL)
		)
	);
	log.appendSink(0, ILogSinkPtr(new LineFormatter(
		ILogSinkPtr(new LogStreamSinkAdapter<ofstream>(coordLog))))
	);
}

int _tmain(int argc, _TCHAR* argv[])
{
	initLogger();

	sqlite3 *pDb;
	sqlite3_open("", &pDb);

	boost::asio::io_service io_service;	

	DualRPC::ObjectsStorage storage;
	GlobalServerObjectPtr so(new GlobalServerObject());
	storage.registerObject(so, nullptr, true);

	DualRPC::Server server(io_service, storage);
	server.setMaxMessageSize(50*1024*1024);
	server.asyncListenTcp("0.0.0.0", 6000);

	io_service.run();
	return 0;
}

