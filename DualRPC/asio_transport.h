#pragma once

#define BOOST_ASIO_HAS_MOVE

#include "transport.h"
#include <boost/asio.hpp>

namespace DualRPC
{

namespace aio = boost::asio;
using aio::ip::tcp;

class AsioClientBase : public ClientBase
{
public:
	AsioClientBase(aio::io_service &iosvc, ObjectsStorage &storage);	

protected:
	aio::io_service &m_iosvc;
	tcp::socket m_socket;
	string m_recvBuffer;
	unsigned int m_recvBufferSize;

	virtual void onStart();
	virtual void onRestart();
	virtual void connectionMade() = 0;

	void close() override;
	void writeData(const string &data) override;
	void writeData(const char *data, unsigned int size);
	Variant startRead(const Variant &v = Variant()) override;

	void handleReadSize(const boost::system::error_code& error, std::size_t bytes_transferred);
	void handleReadData(const boost::system::error_code& error, std::size_t bytes_transferred);
	void handleWrite(const boost::system::error_code& error, std::size_t bytes_transferred);
	virtual void handleError(const boost::system::error_code& error) = 0;

	Variant recvAnswer();
};

//////////////////////////////////////////////////////////////////////////

class AsioClient : public AsioClientBase
{
public:
	AsioClient(aio::io_service &iosvc, ObjectsStorage &storage);

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

class AsioServer;

class AsioClientSession : public AsioClientBase
{
public:
	AsioClientSession(AsioServer &server, SessionID sessionID);
	~AsioClientSession();	

protected:
	void connectionMade() override;
	void handleTimeout(const boost::system::error_code& error);
	void handleError(const boost::system::error_code& error) override;

private:
	friend class AsioServer;

	AsioServer &m_server;
	aio::deadline_timer m_timer;
	SessionID m_sessionID, m_remoteSessionID;

	void cancelTimer();
	void handleReadSession(const boost::system::error_code& error);
};

//////////////////////////////////////////////////////////////////////////

class AsioServer
{
public:
	AsioServer(aio::io_service &iosvc, ObjectsStorage &storage);

	void setDisconnectTimeout(unsigned int sec);
	unsigned int getDisconnectTimeout() const;

	void setMaxMessageSize(unsigned int size);
	unsigned int getMaxMessageSize() const;	

	void listen(const string &addr, unsigned short port);

private:
	friend class AsioClientSession;
	typedef std::map<SessionID, AsioClientSessionPtr> ClientSessionMap;

	aio::io_service &m_iosvc;	
	aio::ip::tcp::acceptor m_acceptor;
	ObjectsStorage &m_storage;
	ClientSessionMap m_clients;
	unsigned int m_disconnectTimeout;
	unsigned int m_maxMesssageSize;

	SessionID getNextSessionID() const;
	
	void startAsyncAccept();
	void handleAccept(const AsioClientSessionPtr &newSession, const boost::system::error_code& error);
	void moveConnectionToSession(SessionID localSessionID, SessionID remoteSessionID);
	void removeSession(SessionID sessionID);
};

}