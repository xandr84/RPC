#include "stdafx.h"
#include "transport.h"
#include "objects.h"
#include "logger.h"

#include <boost/format.hpp>

namespace DualRPC
{

const char* PROTOCOL_NAME = "ROC1";

ClientBase::ClientBase(ObjectsStorage &storage) : 
	m_storage(storage), 	
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

	if(withResult)
	{
		sendCallRequest(RT_CALL_FUNC, getNextRequestID(), id, name, args);
		//return recvAnswer();
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
			writeData(m_messageQueue.front().data);			
		}
		return rd.writeCompletePtr;
	}
	else
	{
		writeData(rd.data);	
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

	m_storage.packVariant(stream, v, shared_from_this());
	return sendBuffer(type, requestID, stream);
}

void ClientBase::processIncomingRequest(const string &data)
{
	m_requireProcessing = true;
	if(m_enableProcessing)
	{
		if(processInput(Variant(), data))
		{			
			startRead();
		}
		else m_delayedData = data;
	}
}

void ClientBase::processDataWritten()
{
	if(!m_messageQueue.empty())
	{
		RequestData &rd = m_messageQueue.front();
		rd.writeCompletePtr->callback(Variant());
		m_messageQueue.pop();
		if(!m_messageQueue.empty())
		{
			writeData(m_messageQueue.front().data);			
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
		f->addBoth(boost::bind(&ClientBase::startRead, shared_from_this(), Variant()));
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
		processIncomingRequest(m_delayedData);
	return Variant();
}

bool ClientBase::processInput(Variant &result, const string &data)
{
	char type;
	RequestID requestID;
	std::istringstream stream(data);	

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
				written->addCallback(boost::bind(&ClientBase::startRead, 
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
				f->addCallback(boost::bind(&ClientBase::startRead, 
					shared_from_this(), Variant()));
				return false;
			}			
		}
		else
		{
			m_storage.localCall(id, name, args, false, -1, written);
			if(written)
			{
				written->addCallback(boost::bind(&ClientBase::startRead, 
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


} //namespace DaulRPC