#include "stdafx.h"
#include "future_result.h"
#include "logger.h"

namespace DualRPC
{

FutureResult::FutureResult() : 
	m_activated(false)
{
}

void FutureResult::execFirstItem()
{
	CallbackList::iterator it = m_callbackList.begin();
	if(it == m_callbackList.end())
		return;

	try
	{
		if(m_lastResult.isException())
			m_lastResult = it->second(m_lastResult);
		else
			m_lastResult = it->first(m_lastResult);
	}
	catch(std::exception &e)
	{
		m_lastResult = e;
	}
	m_callbackList.erase(it);
}

void FutureResult::addCallback(const Callback &cb)
{
	m_callbackList.push_back(CallbackList::value_type(cb, Callback()));
	if(m_activated)
		execFirstItem();
}

void FutureResult::addErrback(const Callback &eb)
{
	m_callbackList.push_back(CallbackList::value_type(Callback(), eb));
	if(m_activated)
		execFirstItem();
}

void FutureResult::addBoth(const Callback &cb, const Callback &eb)
{
	m_callbackList.push_back(CallbackList::value_type(cb, eb));
	if(m_activated)
		execFirstItem();
}

void FutureResult::addBoth(const Callback &cb)
{
	m_callbackList.push_back(CallbackList::value_type(cb, cb));
	if(m_activated)
		execFirstItem();
}

Variant FutureResult::callback(const Variant &result)
{
	m_activated = true;
	m_lastResult = result;
	while(!m_callbackList.empty())
	{
		execFirstItem();
	}
	return m_lastResult;
}

Variant FutureResult::errback(const Variant &error)
{
	return callback(error);
}

///////////////////////////////////////////////////////////////////////////////////

FutureResultList::FutureResultList() : 
	m_activated(0)
{
}

void FutureResultList::addResult(const FutureResultPtr &result)
{
	m_results.push_back(ResultPair(result, Variant()));
}

void FutureResultList::join(const Callback &callback)
{
	unsigned int i = 0;
	m_callback = callback;
	for(ResultList::iterator it = m_results.begin(); it != m_results.end(); ++it)
	{
		if(i++ >= m_activated)
		{
			it->first->addBoth(boost::bind(&FutureResultList::internalCallback, this, it, _1));
		}
	}
}

Variant FutureResultList::internalCallback(ResultList::iterator it, const Variant &v)
{
	it->second = v;
	if(++m_activated == m_results.size())
	{
		Variant result;
		for(ResultList::iterator j = m_results.begin(); j != m_results.end(); ++j)
		{
			result.add(j->second);
		}
		m_callback(result);
	}
	return v;
}

}