// Actor.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include "logger.h"
#include "Actor.h"
#include "ActorObject.h"

#include <fstream>
#include <iostream>

using namespace DualRPC;
using namespace std;

ofstream actorLog("actor.log");

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
		ILogSinkPtr(new LogStreamSinkAdapter<ofstream>(actorLog))))
	);
}

int _tmain(int argc, _TCHAR* argv[])
{
	initLogger();

	boost::asio::io_service io_service;	

	ObjectsStorage storage;

	ActorPtr actor(new ActorClient(io_service, storage));
	actor->setMaxMessageSize(50*1024*1024);
	actor->setEndpoint("192.168.1.21", 6000);
	actor->connectTcp();

	io_service.run();
	return 0;
}

