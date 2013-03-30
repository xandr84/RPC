// Client.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include <iostream>
#include <locale>

#include "variant.h"
#include "objects.h"
#include "asio_transport.h"
#include "logger.h"


using namespace DualRPC;
using namespace std;

class TestObject : public LocalObject
{
public:
	TestObject() {
		registerMethod("boo", boost::bind(&TestObject::boo, this, _1));
	}

	Variant boo(const Variant &args) {
		cout << "TestObject::boo(" << args.repr() << ")" << endl;
		return Variant(args).add("fam", "Ivanov");
	}
};

class MyClient : public AsioClient
{
public:
	MyClient(aio::io_service &iosvc, ObjectsStorage &storage) : 
		AsioClient(iosvc, storage) 
	{
	}

	void onStart() override
	{
		IObjectPtr obj(new TestObject);
		FutureResultPtr f = globalObject()->call("foo", obj, true).toFuture();
		f->addCallback(boost::bind(&MyClient::fooResult, this, _1));			
	}

	Variant fooResult(const Variant &ret)
	{
		cout << "foo result " << ret.repr() << endl;
		return Variant();
	}
};

void testSyncClient()
{
	ObjectsStorage storage;
	aio::io_service io_service;	
	AsioClientPtr client(new AsioClient(io_service, storage));
	client->setEndpoint("127.0.0.1", 6000);
	client->connectTcp();

	try
	{
		IObjectPtr p = client->globalObject();
		IObjectPtr obj(new TestObject);
		Variant v = p->call("foo", obj, true);
		cout << v.repr() << endl;
		//v = p->call("abc");
	}
	catch(remote_error &e)
	{
		cout << "remote error: " << e.what() << endl;
	}
}

void testAsyncClient()
{
	ObjectsStorage storage;
	aio::io_service io_service;	
	AsioClientPtr client(new MyClient(io_service, storage));
	client->setEndpoint("127.0.0.1", 6000);
	client->connectTcp();	
	io_service.run();
}

ofstream clientLog("client.log");

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

struct Var
{
	int *p;

	Var(int i) { 
		LOG_DEBUG(0, "Var(int)");
		p = new int(i); 
	}

	Var(const Var &v) {
		LOG_DEBUG(0, "Var(const Var&)");
		p = new int(*v.p);
	}

	Var(Var &&v) {
		LOG_DEBUG(0, "Var(Var&&)");
		p = v.p;
		v.p = nullptr;
	}

	~Var() {		
		if(p) {
			LOG_DEBUG(0, "~Var");
			delete p;
		}
		else LOG_DEBUG(0, "~Var null");
	}
};

Var func()
{
	Var v(7);
	return v;
}

void proc(const Var &v)
{
	cout << *v.p << endl;
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

	testAsyncClient();
	//cout << sizeof(Variant) << endl;
	//proc(func());

	#ifdef _DEBUG
 	_CrtMemDumpAllObjectsSince(&_ms); // dump leaks
	#endif

	return 0;
}

