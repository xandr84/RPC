#pragma once

#include "transport.h"

class ActorClient : public DualRPC::Client
{
public:
	ActorClient(boost::asio::io_service &iosvc, DualRPC::ObjectsStorage &storage);

	void onStart() override;

	DualRPC::Variant loginResult(const DualRPC::Variant &ret);
	DualRPC::Variant loginError(const DualRPC::Variant &ret);

private:
	unsigned int m_id;
	DualRPC::IObjectPtr m_serverObjPtr;
};

typedef boost::shared_ptr<ActorClient> ActorPtr;