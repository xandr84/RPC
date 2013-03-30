#pragma once

#define BOOST_ASIO_HAS_MOVE

#include <stack>
#include <queue>
#include <sstream>
#include <boost/enable_shared_from_this.hpp>

#include "defs.h"
#include "variant.h"

namespace DualRPC
{

extern const char* PROTOCOL_NAME;

class ClientBase : public boost::enable_shared_from_this<ClientBase>
{
public:
	ClientBase(ObjectsStorage &storage);
	virtual ~ClientBase();

	void setMaxMessageSize(unsigned int size);
	unsigned int getMaxMessageSize() const;	

	void setAsyncMode(bool value);
	bool asyncMode() const;
	
	virtual void close();

	Variant call(ObjectID id, const string &name, const Variant &args, bool withResult, 
		float timeout, FutureResultPtr &written);

	Variant destroyObject(ObjectID id);		

protected:
	virtual Variant startRead(const Variant &v = Variant()) = 0;
	void processIncomingRequest(const string &data);
	virtual void writeData(const string &data) = 0;
	void processDataWritten();

	void sendRequestQueue();
	void cancelRequestQueue(const std::exception &error);

private:
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

	ObjectsStorage &m_storage;	
	bool m_async, m_requireProcessing, m_enableProcessing;
	RequestID m_nextRequestID;
	unsigned int m_maxMessageSize;	
	string m_delayedData;
		
	//sync mode
	std::stack<RequestID> m_syncRequestStack;
	//async mode
	FutureResultMap m_callbacks;
	MessageQueue m_messageQueue;

	RequestID getNextRequestID();

	Variant syncCall(ObjectID id, const string &name, const Variant &args, bool withResult = true);
	Variant asyncCall(ObjectID id, const string &name, const Variant &args, bool withResult = true, 
		float timeout = -1, FutureResultPtr &written = FutureResultPtr());
	
	Variant disableProcessing(RequestID requestID, const Variant &v);
	Variant enableProcessing(const Variant &v);
	Variant sendBuffer(char type, RequestID requestID, std::ostringstream &stream);
	Variant sendReturnResponse(RequestID requestID, const Variant &result);
	Variant sendCallRequest(char type, RequestID requestID, ObjectID id, 
		const string &name, const Variant &args);

	bool processInput(Variant &result, const string &data);
	bool findAndStartCallback(const Variant &result, RequestID id);	
};


}