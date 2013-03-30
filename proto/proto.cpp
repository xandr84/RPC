// proto.cpp: определяет точку входа для консольного приложения.
//
#include "stdafx.h"
#include "variant.h"
#include "objects.h"
#include "asio_transport.h"
#include "logger.h"

#include <iostream>
#include <locale>

using namespace DualRPC;
using namespace std;

class TestObject : public LocalObject
{
public:
	TestObject(aio::io_service &iosvc) : 
	  m_timer(iosvc)
	{
		registerMethod("foo", boost::bind(&TestObject::foo, this, _1)); 
	}

	Variant foo(const Variant &args) {
		cout << "TestObject::foo(" << args.repr() << ")" << endl;
		remobj = args.toObject();
		FutureResultPtr f = remobj->call("boo", Variant("name", "Sergey"), true).toFuture();
		f->addCallback(boost::bind(&TestObject::booResult, this, _1));
		m_fooResult = FutureResultPtr(new FutureResult);
		//booResult2(Variant());
		return Variant(m_fooResult);
	}

	Variant booResult(const Variant &ret)
	{
		cout << "boo result " << ret.repr() << endl;
		m_fooResult->callback(Variant(ret).add("otch", "Petrovich"));
		remobj.reset();
		return Variant();
	}

	Variant booResult2(const Variant &ret)
	{
		m_timer.expires_from_now(boost::posix_time::seconds(3));
		m_timer.async_wait(
			boost::bind(&TestObject::handleTimer, this));
		return Variant();
	}

	void handleTimer()
	{
		FutureResultPtr f = remobj->call("boo", Variant("name", "John"), true).toFuture();
		f->addCallback(boost::bind(&TestObject::booResult2, this, _1));	
	}

private:
	FutureResultPtr m_fooResult;
	aio::deadline_timer m_timer;
	IObjectPtr remobj;
};

void testSyncServer()
{
	aio::io_service io_service;	
	ObjectsStorage storage;
	IObjectPtr obj(new TestObject(io_service));
	storage.registerObject(obj, ClientBasePtr(), true);
	
	AsioServer server(io_service, storage);
	server.listen("127.0.0.1", 6000);
}

void testAsyncServer()
{
	aio::io_service io_service;	
	ObjectsStorage storage;
	IObjectPtr obj(new TestObject(io_service));
	storage.registerObject(obj, ClientBasePtr(), true);
	
	AsioServer server(io_service, storage);
	server.listen("127.0.0.1", 6000);
	io_service.run();
}

ofstream clientLog("proto.log");

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
		ILogSinkPtr(new LogStreamSinkAdapter<ofstream>(clientLog))))
	);
}

int _tmain(int argc, _TCHAR* argv[])
{
	initLogger();

	#ifdef _DEBUG
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE ); // enable file output
	_CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT ); // set file to stdout
	_CrtMemState _ms; 
	_CrtMemCheckpoint(&_ms); // now forget about objects created before
	#endif
	//DualRPC::Variant v = DualRPC::Variant(3, 10).add(5.0).add("str");
	//DualRPC::Variant b = DualRPC::Variant("a", 12).ins("name", "John").ins("array", v);
	//std::cout << b.repr() << std::endl;
	
	testAsyncServer();

	#ifdef _DEBUG
 	//_CrtMemDumpAllObjectsSince(&_ms); // dump leaks
	#endif

	return 0;
}

