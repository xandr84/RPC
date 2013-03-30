#include "stdafx.h"
#include "transport.h"
#include "objects.h"
#include "logger.h"

#include <boost/format.hpp>
#include <boost/random/random_number_generator.hpp>
#include <boost/random/mersenne_twister.hpp>

#include <ctime>  
#include <iostream>

namespace DualRPC
{

const char* PROTOCOL_NAME = "ROC1";

boost::mt19937 random_gen(static_cast<unsigned int>(std::time(0)));
boost::random_number_generator<boost::mt19937, SessionID> random_adapter(random_gen);


ClientBase::ClientBase(aio::io_service &iosvc, ObjectsStorage &storage) : 
	m_iosvc(iosvc), 
	m_storage(storage), 	
	m_socket(iosvc),
	m_async(true),
	m_requireProcessing(false), 
	m_enableProcessing(true),
	m_nextRequestID(1),
	m_maxMessageSize(1024*1024)
{
}

ClientBase::~ClientBase()
{
	close();	
}

void ClientBase::setMaxMessageSize(unsigned int size)
{
	m_maxMessageSize = size;
}

unsigned int ClientBase::getMaxMessageSize() const
{
	return m_maxMessageSize;
}

void ClientBase::setAsyncMode(bool value)
{
	m_async = value;
}

bool ClientBase::asyncMode() const
{
	return m_async;
}

void ClientBase::cancelRequestQueue(const std::exception &error)
{
	int count = 0;
	while(!m_messageQueue.empty())
	{
		RequestData &rd = m_messageQueue.front();
		if(rd.type == RT_CALL_PROC || rd.type == RT_CALL_FUNC)
		{
			FutureResultMap::iterator it = m_callbacks.find(rd.id);
			if(it != m_callbacks.end())
			{
				++count;
				it->second->errback(error);
				m_callbacks.erase(it);
			}
		}
		m_messageQueue.pop();
	}
	LOG_DEBUG_FMT(0, "Canceling %d request queue items", count);
}

Variant ClientBase::call(ObjectID id, const string &name, const Variant &args, 
	bool withResult, float timeout, FutureResultPtr &written)
{
	if(asyncMode())
		return asyncCall(id, name, args, withResult, timeout, written);
	return syncCall(id, name, args, withResult);
}

Variant ClientBase::syncCall(ObjectID id, const string &name, const Variant &args, bool withResult)
{
	LOG_DEBUG_FMT(0, "Call <object id %d>.%s(%s)", id % name % args.repr());

	//Variant newargs = args;
	//m_storage.replaceObjectsToIDs(newargs, shared_from_this());

	if(withResult)
	{
		sendCallRequest(RT_CALL_FUNC, getNextRequestID(), id, name, args);
		return recvAnswer();
	}
	else
	{
		sendCallRequest(RT_CALL_PROC, getNextRequestID(), id, name, args);
	}
	return Variant();
}

Variant ClientBase::asyncCall(ObjectID id, const string &name, const Variant &args, 
	bool withResult, float timeout, FutureResultPtr &written)
{
	LOG_DEBUG_FMT(0, "Async call <object id %d>.%s(%s)", id % name % args.repr());

	/*Variant copyargs;
	const Variant *newargs = &args;	
	if(m_storage.hasObjects(args))
	{
		copyargs = args;
		m_storage.replaceObjectsToIDs(copyargs, shared_from_this());
		newargs = &copyargs;
	}*/

	unsigned int requestID = getNextRequestID();

	if(withResult)
	{
		FutureResultPtr future(new FutureResult);			
		m_callbacks[requestID] = future;		
		written = sendCallRequest(RT_CALL_FUNC, requestID, id, name, args).toFuture();
		return future;
	}
	else
	{
		written = sendCallRequest(RT_CALL_PROC, requestID, id, name, args).toFuture();
	}
	return Variant();
}

Variant ClientBase::destroyObject(ObjectID id)
{
	if(id == 0) return Variant();

	std::ostringstream stream;
	unsigned int size = 0;
	char type = RT_DELOBJ;
	unsigned int requestID = getNextRequestID();

	stream.write((const char*)&size, sizeof(size));
	stream.write(&type, sizeof(type));
	stream.write((const char*)&requestID, sizeof(requestID));
	stream.write((const char*)&id, sizeof(id));

	return sendBuffer(type, requestID, stream);
}

void ClientBase::close()
{
	m_socket.close();
	m_storage.freeClientObjects(shared_from_this());
}

RequestID ClientBase::getNextRequestID()
{
	return m_nextRequestID++;
}

Variant ClientBase::sendBuffer(char type, RequestID requestID, std::ostringstream &stream)
{	
	unsigned int size = (unsigned int)stream.tellp();
	size -= sizeof(size);
	stream.seekp(0);
	stream.write((const char*)&size, sizeof(size));

	RequestData rd;
	rd.type = MessageType(type);
	rd.id = requestID;
	rd.data = stream.str();

	if(asyncMode())
	{
		rd.writeCompletePtr.reset(new FutureResult);
		bool empty = m_messageQueue.empty();
		m_messageQueue.push(rd);
		if(empty)
		{
			aio::async_write(m_socket, aio::buffer(m_messageQueue.front().data),
				boost::bind(&ClientBase::handleWrite, shared_from_this(), 
					aio::placeholders::error,
					aio::placeholders::bytes_transferred)
			);
		}
		return rd.writeCompletePtr;
	}
	else
	{
		aio::write(m_socket, aio::buffer(rd.data));		
	}
	return Variant();
}

Variant ClientBase::sendCallRequest(char type, RequestID requestID, 
		ObjectID id, const string &name, const Variant &args)
{
	std::ostringstream stream;
	unsigned int size = 0;

	stream.write((const char*)&size, sizeof(size));
	stream.write(&type, sizeof(type));
	stream.write((const char*)&requestID, sizeof(requestID));
	stream.write((const char*)&id, sizeof(id));
	packStr(stream, name, 1);
	//args.pack(stream);
	m_storage.packVariant(stream, args, shared_from_this());

	if(asyncMode())
		return sendBuffer(type, requestID, stream);
	else
		m_syncRequestStack.push(requestID);
	return Variant();
}

Variant ClientBase::sendReturnResponse(RequestID requestID, const Variant &v)
{
	std::ostringstream stream;
	unsigned int size = 0;
	char type = RT_RETURN;
	LOG_DEBUG_FMT(0, "Send return response on request %1% value %2%", requestID % v.repr());

	stream.write((const char*)&size, sizeof(size));
	stream.write(&type, sizeof(type));
	stream.write((const char*)&requestID, sizeof(requestID));

	/*Variant t;
	const Variant *p = &v;
	if(m_storage.hasObjects(v))
	{
		t = v;
		m_storage.replaceObjectsToIDs(t, shared_from_this());
		p = &t;
	}
	p->pack(stream);*/
	m_storage.packVariant(stream, v, shared_from_this());
	return sendBuffer(type, requestID, stream);
}

void ClientBase::onStart()
{
}

void ClientBase::onRestart()
{
}

Variant ClientBase::startReadSize(const Variant&)
{
	aio::async_read(m_socket, aio::buffer(&m_recvBufferSize, sizeof(m_recvBufferSize)),
		boost::bind(&ClientBase::handleReadSize, shared_from_this(), 
			aio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);
	return Variant();
}

void ClientBase::handleReadSize(const boost::system::error_code& error, std::size_t bytes_transferred)
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
			boost::bind(&ClientBase::handleReadData, shared_from_this(), 
				aio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
		);
	}
}

void ClientBase::handleReadData(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		m_requireProcessing = true;
		if(m_enableProcessing)
		{
			if(processAnswer(Variant()))
			{			
				startReadSize();
			}
			m_recvBuffer.clear();
		}
	}
}

void ClientBase::handleWrite(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		if(!m_messageQueue.empty())
		{
			RequestData &rd = m_messageQueue.front();
			rd.writeCompletePtr->callback(Variant());
			m_messageQueue.pop();
			if(!m_messageQueue.empty())
			{
				aio::async_write(m_socket, aio::buffer(m_messageQueue.front().data),
					boost::bind(&ClientBase::handleWrite, shared_from_this(), 
						aio::placeholders::error,
						aio::placeholders::bytes_transferred)
				);
			}
		}
	}
}

bool ClientBase::findAndStartCallback(const Variant &result, RequestID id)
{
	FutureResultMap::iterator it = m_callbacks.find(id);
	if(it == m_callbacks.end()) return true;

	Variant v;
	if(result.isException())
		v = it->second->errback(result);
	else
		v = it->second->callback(result);

	if(v.isFuture())
	{
		FutureResultPtr f = v.toFuture();
		f->addBoth(boost::bind(&ClientBase::startReadSize, shared_from_this(), Variant()));
	}

	m_callbacks.erase(it);
	return !v.isFuture();
}

Variant ClientBase::disableProcessing(RequestID requestID, const Variant &v)
{
	m_enableProcessing = false;
	FutureResultPtr f = sendReturnResponse(requestID, v).toFuture();
	f->addCallback(boost::bind(&ClientBase::enableProcessing, shared_from_this(), Variant()));
	return Variant();
}

Variant ClientBase::enableProcessing(const Variant &v)
{
	m_enableProcessing = true;
	if(m_requireProcessing)
		handleReadData(boost::system::error_code(), 0);
	return Variant();
}

bool ClientBase::processAnswer(Variant &result)
{
	char type;
	RequestID requestID;
	std::istringstream stream(m_recvBuffer);	

	m_requireProcessing = false;

	stream.read(&type, sizeof(type));

	if(type == RT_RETURN || type == RT_CALL_PROC || type == RT_CALL_FUNC || type == RT_DELOBJ)
	{
		stream.read((char*)&requestID, sizeof(requestID));
	}

	if(type == RT_RETURN)
	{
		if(!asyncMode())		
		{
			if(m_syncRequestStack.empty())
			{
				throw std::runtime_error((boost::format("Unexpected return answer %1%") % requestID).str());
			}

			if(m_syncRequestStack.top() != requestID)
			{
				throw std::runtime_error(
					(boost::format("Invalid return request id (expected request %1%, but received %2%)") %
						m_syncRequestStack.top() % requestID).str());
			}
		}

		//result.unpack(stream);		
		m_storage.unpackVariant(stream, result, shared_from_this());
		LOG_DEBUG_FMT(0, "Receive answer on request %d value %s", requestID % result.repr());
		//m_storage.replaceIDsToObjects(result, shared_from_this());
		if(asyncMode())
		{
			return findAndStartCallback(result, requestID);
		}
		else
		{
			if(result.isException())
				throw result.toException();
		}
		return true;  //Результат вызова обработан
	}
	else
	if(type == RT_CALL_PROC || type == RT_CALL_FUNC)
	{
		ObjectID id;
		stream.read((char*)&id, sizeof(id));

		string name;
		unpackStr(stream, name, 1);
			
		Variant args;
		//args.unpack(stream);
		//m_storage.replaceIDsToObjects(args, shared_from_this());
		m_storage.unpackVariant(stream, args, shared_from_this());

		LOG_DEBUG_FMT(0, "Receive request %d call <object id %d>.%s(%s)", 
			requestID % id % name % args.repr());	

		FutureResultPtr written;	
		
		if(type == RT_CALL_FUNC)
		{			
			result = m_storage.localCall(id, name, args, true, -1, written);
			if(written)	//Отложенный результат удаленного транзитного вызова
			{
				written->addCallback(boost::bind(&ClientBase::startReadSize, 
					shared_from_this(), Variant()));

				FutureResultPtr f = result.toFuture();
				f->addBoth(boost::bind(&ClientBase::sendReturnResponse, 
					shared_from_this(), requestID, _1));
				return false;
			}

			if(result.isFuture())	//Отложенный результат локального вызова 
			{
				FutureResultPtr f = result.toFuture();
				f->addBoth(boost::bind(&ClientBase::disableProcessing, 
					shared_from_this(), requestID, _1));				
				return true;
			}			
			else //Результат локального вызова
			{		
				FutureResultPtr f = sendReturnResponse(requestID, result).toFuture();
				f->addCallback(boost::bind(&ClientBase::startReadSize, 
					shared_from_this(), Variant()));
				return false;
			}			
		}
		else
		{
			m_storage.localCall(id, name, args, false, -1, written);
			if(written)
			{
				written->addCallback(boost::bind(&ClientBase::startReadSize, 
					shared_from_this(), Variant()));
				return false;
			}			
			return true;
		}
	}
	else
	if(type == RT_DELOBJ)
	{
		ObjectID id;
		stream.read((char*)&id, sizeof(id));
		LOG_DEBUG_FMT(0, "Receive request %d on delete object %d", requestID % id);
		m_storage.deleteObject(id);
		return true;
	}

	return false;
}

Variant ClientBase::recvAnswer()
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
		if(processAnswer(result))
			return result;
	}

	return Variant();
}

//////////////////////////////////////////////////////////////////////////
Client::Client(aio::io_service &iosvc, ObjectsStorage &storage) :
	ClientBase(iosvc, storage),
	m_sessionID(0),
	m_newSessionID(0),
	m_port(0),
	m_resolver(iosvc),
	m_reconTimer(iosvc),
	m_reconnectTimeout(5)
{
}

void Client::setReconnectTimeout(unsigned int sec)
{
	m_reconnectTimeout = sec;
}

unsigned int Client::getReconnectTimeout() const
{
	return m_reconnectTimeout;
}

void Client::setEndpoint(const string &host, unsigned short port)
{
	m_host = host;
	m_port = port;
}

IObjectPtr Client::globalObject()
{
	return IObjectPtr(new RemoteObject(shared_from_this(), 0));
}

bool Client::connectTcp()
{
	if(asyncMode())
	{
		asyncConnectTcp();
		return true;
	}
	return syncConnectTcp();
}

bool Client::syncConnectTcp()
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

void Client::asyncConnectTcp()
{
	LOG_INFO_FMT(0, "Connecting to %1%:%2%", m_host % m_port);

    tcp::resolver::query query(tcp::v4(), m_host, std::to_string((long long)m_port));

	m_resolver.async_resolve(query,
		boost::bind(&Client::handleResolve, 
			boost::dynamic_pointer_cast<Client, ClientBase>(shared_from_this()), 
			boost::asio::placeholders::error, 
            boost::asio::placeholders::iterator)
	);	
}

void Client::doReconnect()
{
	m_reconTimer.expires_from_now(boost::posix_time::seconds(getReconnectTimeout()));
	m_reconTimer.async_wait(
		boost::bind(&Client::asyncConnectTcp, 
			boost::dynamic_pointer_cast<Client, ClientBase>(shared_from_this()))			
	);
}

void Client::handleResolve(const boost::system::error_code& error, 
	tcp::resolver::iterator iterator)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		aio::async_connect(m_socket, iterator,
			boost::bind(&Client::handleConnect, 
				boost::dynamic_pointer_cast<Client, ClientBase>(shared_from_this()), 
				aio::placeholders::error)
		);
	}
}

void Client::handleConnect(const boost::system::error_code& error)
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

void Client::connectionMade()
{
	aio::async_read(m_socket, aio::buffer(m_proto, 4),
		boost::bind(&Client::handleReadProto, 
			boost::dynamic_pointer_cast<Client, ClientBase>(shared_from_this()), 
			aio::placeholders::error)
	);
}

void Client::handleReadProto(const boost::system::error_code& error)
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
				boost::bind(&Client::handleWriteSession, 
					boost::dynamic_pointer_cast<Client, ClientBase>(shared_from_this()),
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

void Client::handleWriteSession(const boost::system::error_code& error)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		aio::async_read(m_socket, aio::buffer(&m_newSessionID, sizeof(m_newSessionID)),
			boost::bind(&Client::handleReadSession, 
				boost::dynamic_pointer_cast<Client, ClientBase>(shared_from_this()),
				aio::placeholders::error)
		);
	}
}

void Client::handleReadSession(const boost::system::error_code& error)
{
	if(error)
	{
		handleError(error);
	}
	else
	{
		LOG_DEBUG_FMT(0, "Receive session %1%", m_newSessionID);

		aio::async_read(m_socket, aio::buffer(&m_recvBufferSize, sizeof(m_recvBufferSize)),
			boost::bind(&ClientBase::handleReadSize, shared_from_this(), 
				aio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
		);

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

void Client::handleError(const boost::system::error_code& error)
{
	LOG_ERROR_FMT(0, "%1%", error.message());
	close();
	doReconnect();
}


//////////////////////////////////////////////////////////////////////////
ClientSession::ClientSession(Server &server, SessionID sessionID) :
	ClientBase(server.m_iosvc, server.m_storage),
	m_server(server),
	m_timer(server.m_iosvc),
	m_sessionID(sessionID),
	m_remoteSessionID(0)
{
	LOG_DEBUG_FMT(0, "ClientSession create %1%", m_sessionID);
}

ClientSession::~ClientSession()
{
	LOG_DEBUG_FMT(0, "ClientSession delete %1%", m_sessionID);
}

void ClientSession::cancelTimer()
{
	m_timer.cancel();
}

void ClientSession::connectionMade()
{
	aio::async_write(m_socket, aio::buffer(PROTOCOL_NAME, 4),
		boost::bind(&ClientBase::handleWrite, shared_from_this(), 
			aio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);
	aio::async_read(m_socket, aio::buffer(&m_remoteSessionID, sizeof(m_remoteSessionID)),
		boost::bind(&ClientSession::handleReadSession, 
			boost::dynamic_pointer_cast<ClientSession, ClientBase>(shared_from_this()), 
			aio::placeholders::error)
	);
}

void ClientSession::handleReadSession(const boost::system::error_code& error)
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

void ClientSession::handleError(const boost::system::error_code& error)
{
	LOG_ERROR_FMT(0, "Client session message '%1%'", error.message());

	m_timer.expires_from_now(boost::posix_time::seconds(m_server.getDisconnectTimeout()));
	m_timer.async_wait(
		boost::bind(&ClientSession::handleTimeout, 
			boost::dynamic_pointer_cast<ClientSession, ClientBase>(shared_from_this()),
			aio::placeholders::error)
	);
}

void ClientSession::handleTimeout(const boost::system::error_code& error)
{
	if(aio::error::operation_aborted != error)
	{
		cancelRequestQueue(remote_error(error.message()));
		close();
		m_server.removeSession(m_sessionID);
	}
}

//////////////////////////////////////////////////////////////////////////
Server::Server(aio::io_service &iosvc, ObjectsStorage &storage) : 
	m_iosvc(iosvc),
	m_storage(storage),
	m_acceptor(iosvc),
	m_disconnectTimeout(30), //sec
	m_maxMesssageSize(1024*1024)
{
}

void Server::setDisconnectTimeout(unsigned int sec)
{
	m_disconnectTimeout = sec;
}

unsigned int Server::getDisconnectTimeout() const
{
	return m_disconnectTimeout;
}

void Server::setMaxMessageSize(unsigned int size)
{
	m_maxMesssageSize = size;
}

unsigned int Server::getMaxMessageSize() const
{
	return m_maxMesssageSize;
}

SessionID Server::getNextSessionID() const
{
	SessionID id = 0;
	do 
	{
		id = random_adapter(std::numeric_limits<SessionID>::max());
	} 
	while(id == 0 || m_clients.find(id) != m_clients.end());
	return id;
}

void Server::listenTcp(const string &addr, unsigned short port)
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

void Server::asyncListenTcp(const string &addr, unsigned short port)
{
	LOG_INFO_FMT(0, "Start async listening on %1%:%2%", addr % port);

	tcp::endpoint endpoint(aio::ip::address::from_string(addr), port);
	m_acceptor.open(endpoint.protocol());
	m_acceptor.set_option(tcp::acceptor::reuse_address(true));
	m_acceptor.bind(endpoint);
	m_acceptor.listen();
	startAsyncAccept();
}

void Server::startAsyncAccept()
{
	SessionID id = getNextSessionID();
	ClientSessionPtr newSession(new ClientSession(*this, id));
	newSession->setMaxMessageSize(getMaxMessageSize());
	m_clients.insert(ClientSessionMap::value_type(id, newSession));
    m_acceptor.async_accept(newSession->m_socket,
        boost::bind(&Server::handleAccept, this, newSession, aio::placeholders::error)
	);
}

void Server::handleAccept(const ClientSessionPtr &newSession, const boost::system::error_code& error)
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

void Server::moveConnectionToSession(SessionID localSessionID, SessionID remoteSessionID)
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
	aio::async_write(remote->second->m_socket, 
		aio::buffer(&remote->second->m_sessionID, sizeof(SessionID)),
		boost::bind(&ClientBase::handleWrite, remote->second, 
			aio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);

	remote->second->startReadSize();
	/*
	aio::async_read(remote->second->m_socket, 
		aio::buffer(&remote->second->m_recvBufferSize, 
			sizeof(remote->second->m_recvBufferSize)),
		boost::bind(&ClientBase::handleReadSize, remote->second, 
			aio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);*/
}

void Server::removeSession(SessionID sessionID)
{
	LOG_DEBUG_FMT(0, "Remove session %1% from server", sessionID);
	m_clients.erase(sessionID);
}

} //namespace DaulRPC