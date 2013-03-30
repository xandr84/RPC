#pragma once

#define BOOST_ASIO_HAS_MOVE

#include <stack>
#include <queue>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "defs.h"
#include "variant.h"

namespace DualRPC
{

namespace aio = boost::asio;
using aio::ip::tcp;

class ClientBase : public boost::enable_shared_from_this<ClientBase>
{
public:
	ClientBase(aio::io_service &iosvc, ObjectsStorage &storage);
	virtual ~ClientBase();

	void setMaxMessageSize(unsigned int size);
	unsigned int getMaxMessageSize() const;	

	void setAsyncMode(bool value);
	bool asyncMode() const;

	virtual void onStart();
	virtual void onRestart();
	void close();

	Variant call(ObjectID id, const string &name, const Variant &args, bool withResult, 
		float timeout, FutureResultPtr &written);
	Variant destroyObject(ObjectID id);

	void handleReadSize(const boost::system::error_code& error, std::size_t bytes_transferred);
	void handleReadData(const boost::system::error_code& error, std::size_t bytes_transferred);
	void handleWrite(const boost::system::error_code& error, std::size_t bytes_transferred);
	virtual void handleError(const boost::system::error_code& error) = 0;

	virtual void connectionMade() = 0;

protected:
	enum MessageType
	{
		RT_PING = 0,
		RT_PONG = 1,
		RT_CALL_PROC = 10,
		RT_CALL_FUNC = 11,
		RT_RETURN = 20,
		RT_DELOBJ = 30
	};	
	struct RequestData
	{
		MessageType type;
		RequestID id;
		FutureResultPtr writeCompletePtr;
		string data;
	};
	typedef std::map<RequestID, FutureResultPtr> FutureResultMap;
	typedef std::queue<RequestData> MessageQueue;

	aio::io_service &m_iosvc;
	ObjectsStorage &m_storage;	
	tcp::socket m_socket;
	bool m_async, m_requireProcessing, m_enableProcessing;
	RequestID m_nextRequestID;
	unsigned int m_maxMessageSize;	
	unsigned int m_recvBufferSize;
	string m_recvBuffer;
	//sync mode
	std::stack<RequestID> m_syncRequestStack;
	//async mode
	FutureResultMap m_callbacks;
	MessageQueue m_messageQueue;

	RequestID getNextRequestID();

	Variant syncCall(ObjectID id, const string &name, const Variant &args, bool withResult = true);
	Variant asyncCall(ObjectID id, const string &name, const Variant &args, bool withResult = true, 
		float timeout = -1, FutureResultPtr &written = FutureResultPtr());

	Variant startReadSize(const Variant &v = Variant());
	Variant disableProcessing(RequestID requestID, const Variant &v);
	Variant enableProcessing(const Variant &v);
	Variant sendBuffer(char type, RequestID requestID, std::ostringstream &stream);
	Variant sendReturnResponse(RequestID requestID, const Variant &result);
	Variant sendCallRequest(char type, RequestID requestID, ObjectID id, 
		const string &name, const Variant &args);

	bool processAnswer(Variant &result);
	bool findAndStartCallback(const Variant &result, RequestID id);
	Variant recvAnswer();
	
	void sendRequestQueue();
	void cancelRequestQueue(const std::exception &error);
};

//////////////////////////////////////////////////////////////////////////

class Client : public ClientBase
{
public:
	Client(aio::io_service &iosvc, ObjectsStorage &storage);

	void setReconnectTimeout(unsigned int sec);
	unsigned int getReconnectTimeout() const;

	void setEndpoint(const string &host, unsigned short port);
	bool connectTcp();

	IObjectPtr globalObject();

protected:
	void connectionMade() override;
	void handleError(const boost::system::error_code& error) override;

private:
	char m_proto[5];
	SessionID m_sessionID, m_newSessionID;
	string m_host;
	unsigned short m_port;
	tcp::resolver m_resolver;
	aio::deadline_timer m_reconTimer;
	unsigned int m_reconnectTimeout;

	bool syncConnectTcp();
	void asyncConnectTcp();

	void doReconnect();
	void handleResolve(const boost::system::error_code& error, tcp::resolver::iterator iterator);
	void handleConnect(const boost::system::error_code& error);
	void handleReadProto(const boost::system::error_code& error);
	void handleWriteSession(const boost::system::error_code& error);
	void handleReadSession(const boost::system::error_code& error);
};

//////////////////////////////////////////////////////////////////////////

class Server;

class ClientSession : public ClientBase
{
public:
	ClientSession(Server &server, SessionID sessionID);
	~ClientSession();	

protected:
	void connectionMade() override;
	void handleTimeout(const boost::system::error_code& error);
	void handleError(const boost::system::error_code& error) override;

private:
	friend class Server;

	Server &m_server;
	aio::deadline_timer m_timer;
	SessionID m_sessionID, m_remoteSessionID;

	void cancelTimer();
	void handleReadSession(const boost::system::error_code& error);
};

//////////////////////////////////////////////////////////////////////////

class Server
{
public:
	Server(aio::io_service &iosvc, ObjectsStorage &storage);

	void setDisconnectTimeout(unsigned int sec);
	unsigned int getDisconnectTimeout() const;

	void setMaxMessageSize(unsigned int size);
	unsigned int getMaxMessageSize() const;	

	void listenTcp(const string &addr, unsigned short port);
	void asyncListenTcp(const string &addr, unsigned short port);

private:
	friend class ClientSession;
	typedef std::map<SessionID, ClientSessionPtr> ClientSessionMap;

	aio::io_service &m_iosvc;
	ObjectsStorage &m_storage;
	aio::ip::tcp::acceptor m_acceptor;
	ClientSessionMap m_clients;
	unsigned int m_disconnectTimeout;
	unsigned int m_maxMesssageSize;

	SessionID getNextSessionID() const;
	void startAsyncAccept();
	void handleAccept(const ClientSessionPtr &newSession, const boost::system::error_code& error);
	void moveConnectionToSession(SessionID localSessionID, SessionID remoteSessionID);
	void removeSession(SessionID sessionID);
};


}