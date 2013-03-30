#pragma once

#include "transport.h"

class ManagerClient : public DualRPC::Client
{
public:
	ManagerClient(boost::asio::io_service &iosvc, DualRPC::ObjectsStorage &storage) : 
		DualRPC::Client(iosvc, storage) {
	}
};