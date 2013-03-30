#pragma once

#include <boost/bimap.hpp>
#include <map>

#include "defs.h"
#include "variant.h"

namespace DualRPC
{

class FutureResult
{
public:
	FutureResult();

	void addCallback(const Callback &callback);
	void addErrback(const Callback &errback);
	void addBoth(const Callback &callback, const Callback &errback);
	void addBoth(const Callback &callback);

	Variant callback(const Variant &result);
	Variant errback(const Variant &error);

private:
	typedef std::list< std::pair<Callback, Callback> > CallbackList;

	bool m_activated;
	Variant m_lastResult;
	CallbackList m_callbackList;

	void execFirstItem();
};

class FutureResultList
{
public:
	FutureResultList();

	void addResult(const FutureResultPtr &result);
	void join(const Callback &callback);

private:
	typedef std::pair<FutureResultPtr, Variant> ResultPair;
	typedef std::list<ResultPair> ResultList;

	ResultList m_results;
	unsigned int m_activated;
	Callback m_callback;

	Variant internalCallback(ResultList::iterator it, const Variant &v);
};

}