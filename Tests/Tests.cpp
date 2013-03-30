// Tests.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"

#include "variant.h"
#include "objects.h"
#include "transport.h"
#include "logger.h"

#include "Actor.h"
#include "Manager.h"
#include "ServerObject.h"

#include <iostream>
#include <locale>

using namespace DualRPC;
using namespace std;

ofstream testsLog("tests.log");

void initLogger()
{
	std::locale::global(std::locale(std::locale::classic(), "", std::locale::ctype));

	Logger &log = Logger::instance();
	log.registerSender(0, "Main", 
		ILogSinkPtr(new LineFormatter(
			ILogSinkPtr(new LogStreamSinkAdapter<ostream>(cout)),
			LineFormatter::LF_TEXT | LineFormatter::LF_ENDL)
		)
	);
	log.appendSink(0, ILogSinkPtr(new LineFormatter(
		ILogSinkPtr(new LogStreamSinkAdapter<ofstream>(testsLog))))
	);
}

int _tmain(int argc, _TCHAR* argv[])
{
	initLogger();

	boost::asio::io_service io_service;	

	//Server
	ObjectsStorage serverStorage;
	serverStorage.registerObject(IObjectPtr(new ServerObject), nullptr, true);

	Server server(io_service, serverStorage);
	server.setMaxMessageSize(50*1024*1024);
	server.asyncListenTcp("0.0.0.0", 6000);

	//Actor
	ObjectsStorage actorStorage;
	boost::shared_ptr<ActorClient> actor(new ActorClient(io_service, actorStorage));
	actor->setMaxMessageSize(50*1024*1024);
	actor->setEndpoint("localhost", 6000);
	actor->connectTcp();

	//Manager
	ObjectsStorage managerStorage;
	boost::shared_ptr<ManagerClient> manager(new ManagerClient(io_service, managerStorage));
	manager->setMaxMessageSize(50*1024*1024);
	manager->setEndpoint("localhost", 6000);
	manager->connectTcp();

	io_service.run();
	return 0;
}

