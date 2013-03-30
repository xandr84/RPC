#pragma once

#include "transport.h"

class ActorClient : public DualRPC::Client
{
public:
	ActorClient(boost::asio::io_service &iosvc, DualRPC::ObjectsStorage &storage)  : 
		DualRPC::Client(iosvc, storage) {
	}
};