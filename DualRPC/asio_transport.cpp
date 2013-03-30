#include "stdafx.h"
#include "asio_transport.h"
#include "objects.h"
#include "logger.h"

#include <boost/format.hpp>
#include <iostream>
#include <ctime>  

#include <boost/random/random_number_generator.hpp>
#include <boost/random/mersenne_twister.hpp>

namespace DualRPC
{

boost::mt19937 random_gen(static_cast<unsigned int>(std::time(0)));
boost::random_number_generator<boost::mt19937, SessionID> random_adapter(random_gen);

AsioClientBase::AsioClientBase(aio::io_service &iosvc, ObjectsStorage &storage) :
	ClientBase(storage),
	m_iosvc(iosvc),
	m_socket(iosvc)
{
}

void AsioClientBase::close()
{
	ClientBase::close();
	m_socket.close();
}

void AsioClientBase::onStart()
{
}

void AsioClientBase::onRestart()
{
}

void AsioClientBase::writeData(const string &data) 
{
	writeData(data.c_str(), data.size());
}

void AsioClientBase::writeData(const char *data, unsigned int size)
{
	aio::async_write(m_socket, aio::buffer(data, size),
		boost::bind(&AsioClientBase::handleWrite, 
			boost::dynamic_pointer_cast<AsioClientBase, ClientBase>(shared_from_this()), 
			aio::placeholders::error,
			aio::placeholders::bytes_transferred)
	);
}

Variant AsioClientBase::startRead(const Variant&)
{
	aio::async_read(m_socket, aio::buffer(&m_recvBufferSize, sizeof(m_recvBufferSize)),
		boost::bind(&AsioClientBase::handleReadSize, 
			boost::dynamic_pointer_cast<AsioClientBase, ClientBase>(shared_from_this()), 
			aio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);
	return Variant();
}

void AsioClientBase::handleReadSize(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		if(m_recvBufferSize > getMaxMessageSize())
		{
			LOG_ALARM_FMT(0, "Max message size %1% bytes exceeded by read size in %2% bytes",
				getMaxMessageSize() % m_recvBufferSize);
			close();
		}
		m_recvBuffer.resize(m_recvBufferSize);
		aio::async_read(m_socket, aio::buffer(&m_recvBuffer[0], m_recvBufferSize),
			boost::bind(&AsioClientBase::handleReadData, 
				boost::dynamic_pointer_cast<AsioClientBase, ClientBase>(shared_from_this()), 
				aio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
		);
	}
}

void AsioClientBase::handleReadData(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		processIncomingRequest(m_recvBuffer);
		m_recvBuffer.clear();
	}
}

void AsioClientBase::handleWrite(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		processDataWritten();
	}
}

Variant AsioClientBase::recvAnswer()
{
	while(true)
	{
		aio::read(m_socket, aio::buffer((char*)&m_recvBufferSize, sizeof(m_recvBufferSize)));
		if(m_recvBufferSize > getMaxMessageSize())
		{
			throw std::runtime_error(
				(boost::format("Max message size %1% bytes exceeded by read size in %2% bytes") %
					getMaxMessageSize() % m_recvBufferSize).str());
		}

		m_recvBuffer.resize(m_recvBufferSize);
		aio::read(m_socket, aio::buffer(&m_recvBuffer[0], m_recvBufferSize), 
			aio::transfer_exactly(m_recvBufferSize));

		Variant result;
		processIncomingRequest(m_recvBuffer);
		return result;
	}

	return Variant();
}

//////////////////////////////////////////////////////////////////////////
AsioClient::AsioClient(aio::io_service &iosvc, ObjectsStorage &storage) :
	AsioClientBase(iosvc, storage),
	m_sessionID(0),
	m_newSessionID(0),
	m_port(0),
	m_resolver(iosvc),
	m_reconTimer(iosvc),
	m_reconnectTimeout(5)
{
}

void AsioClient::setReconnectTimeout(unsigned int sec)
{
	m_reconnectTimeout = sec;
}

unsigned int AsioClient::getReconnectTimeout() const
{
	return m_reconnectTimeout;
}

void AsioClient::setEndpoint(const string &host, unsigned short port)
{
	m_host = host;
	m_port = port;
}

IObjectPtr AsioClient::globalObject()
{
	return IObjectPtr(new RemoteObject(shared_from_this(), 0));
}

bool AsioClient::connectTcp()
{
	if(asyncMode())
	{
		asyncConnectTcp();
		return true;
	}
	return syncConnectTcp();
}

bool AsioClient::syncConnectTcp()
{
	tcp::resolver resolver(m_iosvc);
    tcp::resolver::query query(tcp::v4(), m_host, std::to_string((long long)m_port));
    tcp::resolver::iterator iterator = resolver.resolve(query);

	boost::system::error_code ec;
	std::cout << "Connecting to " << m_host << ":" << m_port << " - ";
    aio::connect(m_socket, iterator, ec);
	if(ec != 0)
	{
		std::cout << "error " << ec.message() << std::endl;
		return false;
	}

	std::cout << "success" << std::endl;
	aio::read(m_socket, aio::buffer(m_proto, 4));
	m_proto[4] = 0;
	if(strcmp(m_proto, PROTOCOL_NAME) != 0)
	{
		std::cout << "Invalid protocol name" << m_proto << std::endl;
		close();
		return false;
	}
	onStart();
	return true;
}

void AsioClient::asyncConnectTcp()
{
	LOG_INFO_FMT(0, "Connecting to %1%:%2%", m_host % m_port);

    tcp::resolver::query query(tcp::v4(), m_host, std::to_string((long long)m_port));

	m_resolver.async_resolve(query,
		boost::bind(&AsioClient::handleResolve, 
			boost::dynamic_pointer_cast<AsioClient, ClientBase>(shared_from_this()), 
			boost::asio::placeholders::error, 
            boost::asio::placeholders::iterator)
	);	
}

void AsioClient::doReconnect()
{
	m_reconTimer.expires_from_now(boost::posix_time::seconds(getReconnectTimeout()));
	m_reconTimer.async_wait(
		boost::bind(&AsioClient::asyncConnectTcp, 
			boost::dynamic_pointer_cast<AsioClient, ClientBase>(shared_from_this()))			
	);
}

void AsioClient::handleResolve(const boost::system::error_code& error, 
	tcp::resolver::iterator iterator)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		aio::async_connect(m_socket, iterator,
			boost::bind(&AsioClient::handleConnect, 
				boost::dynamic_pointer_cast<AsioClient, ClientBase>(shared_from_this()), 
				aio::placeholders::error)
		);
	}
}

void AsioClient::handleConnect(const boost::system::error_code& error)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		LOG_INFO(0, "Successfull connected");
		connectionMade();
	}
}

void AsioClient::connectionMade()
{
	aio::async_read(m_socket, aio::buffer(m_proto, 4),
		boost::bind(&AsioClient::handleReadProto, 
			boost::dynamic_pointer_cast<AsioClient, ClientBase>(shared_from_this()), 
			aio::placeholders::error)
	);
}

void AsioClient::handleReadProto(const boost::system::error_code& error)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		m_proto[4] = 0;
		if(strcmp(m_proto, PROTOCOL_NAME) == 0)
		{
			aio::async_write(m_socket, aio::buffer(&m_sessionID, sizeof(m_sessionID)),
				boost::bind(&AsioClient::handleWriteSession, 
					boost::dynamic_pointer_cast<AsioClient, ClientBase>(shared_from_this()),
					aio::placeholders::error)
			);
		}
		else 
		{
			LOG_FATAL_FMT(0, "Invalid protocol name '%s'", m_proto);
			close();
		}
	}
}

void AsioClient::handleWriteSession(const boost::system::error_code& error)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		aio::async_read(m_socket, aio::buffer(&m_newSessionID, sizeof(m_newSessionID)),
			boost::bind(&AsioClient::handleReadSession, 
				boost::dynamic_pointer_cast<AsioClient, ClientBase>(shared_from_this()),
				aio::placeholders::error)
		);
	}
}

void AsioClient::handleReadSession(const boost::system::error_code& error)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		LOG_DEBUG_FMT(0, "Receive session %1%", m_newSessionID);

		/*aio::async_read(m_socket, aio::buffer(&m_recvBufferSize, sizeof(m_recvBufferSize)),
			boost::bind(&ClientBase::handleReadSize, shared_from_this(), 
				aio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
		);*/
		startRead();

		if(m_sessionID == m_newSessionID)
		{
			handleWrite(boost::system::error_code(), 0);
			onRestart();
		}
		else 
		{
			m_sessionID = m_newSessionID;
			onStart();
		}
	}
}

void AsioClient::handleError(const boost::system::error_code& error)
{
	LOG_ERROR_FMT(0, "%1%", error.message());
	close();
	doReconnect();
}


//////////////////////////////////////////////////////////////////////////
AsioClientSession::AsioClientSession(AsioServer &server, SessionID sessionID) :
	AsioClientBase(server.m_iosvc, server.m_storage),
	m_server(server),
	m_timer(server.m_iosvc),
	m_sessionID(sessionID),
	m_remoteSessionID(0)
{
	LOG_DEBUG_FMT(0, "ClientSession create %1%", m_sessionID);
}

AsioClientSession::~AsioClientSession()
{
	LOG_DEBUG_FMT(0, "ClientSession delete %1%", m_sessionID);
}

void AsioClientSession::cancelTimer()
{
	m_timer.cancel();
}

void AsioClientSession::connectionMade()
{
	writeData(PROTOCOL_NAME, 4);
	/*aio::async_write(m_socket, aio::buffer(m_protoName),
		boost::bind(&AsioClientBase::handleWrite, 
			boost::dynamic_pointer_cast<AsioClientSession, ClientBase>(shared_from_this()), 
			aio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);*/
	aio::async_read(m_socket, aio::buffer(&m_remoteSessionID, sizeof(m_remoteSessionID)),
		boost::bind(&AsioClientSession::handleReadSession, 
			boost::dynamic_pointer_cast<AsioClientSession, ClientBase>(shared_from_this()), 
			aio::placeholders::error)
	);
}

void AsioClientSession::handleReadSession(const boost::system::error_code& error)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		m_server.moveConnectionToSession(m_sessionID, m_remoteSessionID);
	}
}

void AsioClientSession::handleError(const boost::system::error_code& error)
{
	LOG_ERROR_FMT(0, "Client session message '%1%'", error.message());

	m_timer.expires_from_now(boost::posix_time::seconds(m_server.getDisconnectTimeout()));
	m_timer.async_wait(
		boost::bind(&AsioClientSession::handleTimeout, 
			boost::dynamic_pointer_cast<AsioClientSession, ClientBase>(shared_from_this()),
			aio::placeholders::error)
	);
}

void AsioClientSession::handleTimeout(const boost::system::error_code& error)
{
	if(aio::error::operation_aborted != error)
	{
		cancelRequestQueue(remote_error(error.message()));
		close();
		m_server.removeSession(m_sessionID);
	}
}

//////////////////////////////////////////////////////////////////////////
AsioServer::AsioServer(aio::io_service &iosvc, ObjectsStorage &storage) : 
	m_iosvc(iosvc),
	m_storage(storage),
	m_acceptor(iosvc),
	m_disconnectTimeout(30), //sec
	m_maxMesssageSize(1024*1024)
{
}

void AsioServer::setDisconnectTimeout(unsigned int sec)
{
	m_disconnectTimeout = sec;
}

unsigned int AsioServer::getDisconnectTimeout() const
{
	return m_disconnectTimeout;
}

void AsioServer::setMaxMessageSize(unsigned int size)
{
	m_maxMesssageSize = size;
}

unsigned int AsioServer::getMaxMessageSize() const
{
	return m_maxMesssageSize;
}

SessionID AsioServer::getNextSessionID() const
{
	SessionID id = 0;
	do 
	{
		id = random_adapter(std::numeric_limits<SessionID>::max());
	} 
	while(id == 0 || m_clients.find(id) != m_clients.end());
	return id;
}

/*
void AsioServer::listenTcp(const string &addr, unsigned short port)
{
	LOG_INFO_FMT(0, "Start async listening on %1%:%2%", addr % port);

	while(true)
	{
		SessionID id = getNextSessionID();
		ClientSession session(*this, id);
		boost::system::error_code ec;
		m_acceptor.accept(session.m_socket, 
			tcp::endpoint(aio::ip::address::from_string(addr), port), ec);
		if(ec) return;

		while(session.m_socket.is_open())
			session.recvAnswer();
	}
}
*/

void AsioServer::listen(const string &addr, unsigned short port)
{
	LOG_INFO_FMT(0, "Start async listening on %1%:%2%", addr % port);

	tcp::endpoint endpoint(aio::ip::address::from_string(addr), port);
	m_acceptor.open(endpoint.protocol());
	m_acceptor.set_option(tcp::acceptor::reuse_address(true));
	m_acceptor.bind(endpoint);
	m_acceptor.listen();
	startAsyncAccept();
}

void AsioServer::startAsyncAccept()
{
	SessionID id = getNextSessionID();
	AsioClientSessionPtr newSession(new AsioClientSession(*this, id));
	newSession->setMaxMessageSize(getMaxMessageSize());
	m_clients[id] = newSession;
    m_acceptor.async_accept(newSession->m_socket,
        boost::bind(&AsioServer::handleAccept, this, newSession, aio::placeholders::error)
	);
}

void AsioServer::handleAccept(const AsioClientSessionPtr &newSession, const boost::system::error_code& error)
{
	if(error)
	{
		LOG_ERROR_FMT(0, "Accept client error: '%1%'", error.message());
	}
	else 
	{
		LOG_INFO_FMT(0, "New client accepted %1%", 
			newSession->m_socket.remote_endpoint().address().to_string());

		newSession->connectionMade();
	}

	startAsyncAccept();
}

void AsioServer::moveConnectionToSession(SessionID localSessionID, SessionID remoteSessionID)
{
	ClientSessionMap::iterator local = m_clients.find(localSessionID);
	ClientSessionMap::iterator remote = m_clients.find(remoteSessionID);
	if(remote == m_clients.end())
	{
		remote = local;
	}
	else
	{
		LOG_DEBUG_FMT(0, "Move socket from session %1% to %2%", localSessionID % remoteSessionID);
		remote->second->cancelTimer();
		remote->second->m_socket = std::move(local->second->m_socket);
		m_clients.erase(local);
	}

	LOG_DEBUG_FMT(0, "Send session %1% to client", remote->second->m_sessionID);

	remote->second->writeData((char*)&remote->second->m_sessionID, sizeof(SessionID));
	remote->second->startRead();
	/*
	aio::async_read(remote->second->m_socket, 
		aio::buffer(&remote->second->m_recvBufferSize, 
			sizeof(remote->second->m_recvBufferSize)),
		boost::bind(&ClientBase::handleReadSize, remote->second, 
			aio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);*/
}

void AsioServer::removeSession(SessionID sessionID)
{
	LOG_DEBUG_FMT(0, "Remove session %1% from server", sessionID);
	m_clients.erase(sessionID);
}

}
