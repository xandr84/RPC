#include "stdafx.h"
#include "logger.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#define BOOST_CHRONO_VERSION 2
#include <boost/chrono/chrono_io.hpp>

LogMessage::LogMessage() :
	level(DEBUG_LEVEL), senderID(0), file(nullptr), func(nullptr), line(0)
{
}

LogMessage::LogMessage(ENUM_LOG_LEVEL lvl, int sen, const std::string &txt,
		const char *f, const char *fun, int ln) :
	level(lvl), senderID(sen), text(txt), file(f), func(fun), line(ln)
{
}

//////////////////////////////////////////////////////////////////////
Logger gLogger;

Logger::Logger() : 
	m_level(DEBUG_LEVEL)
{
}

Logger& Logger::instance()
{
	return gLogger;
}

void Logger::registerSender(int id, const std::string &name, ILogSinkPtr sink)
{
	SenderInfo info;
	info.name = name;
	info.sinks.push_back(sink);
	m_senders[id] = info;
}

void Logger::removeSender(int id)
{
	m_senders.erase(id);
}

void Logger::appendSink(int senderID, ILogSinkPtr sink)
{
	m_senders[senderID].sinks.push_back(sink);
}

void Logger::removeSink(int senderID, ILogSinkPtr sink)
{
	auto it = m_senders.find(senderID);
	if(it != m_senders.end())
	{
		auto s = it->second.sinks;
		std::remove(s.begin(), s.end(), sink);
	}
}

void Logger::setLevel(ENUM_LOG_LEVEL level)
{
	m_level = level;
}

void Logger::write(const LogMessage &message)
{
	if(message.level < m_level)
		return;

	auto it = m_senders.find(message.senderID);
	if(it == m_senders.end())
		return;

	for(auto s = it->second.sinks.begin(); s != it->second.sinks.end(); ++s)
	{
		(*s)->write(message);
	}
}

//////////////////////////////////////////////////////////////////////
LineFormatter::LineFormatter(ILogSinkPtr sink, int param, const char *time_format) : 
	m_sink(sink), m_param(param), m_timeFormat(time_format)
{
}

void LineFormatter::write(const LogMessage &message)
{
	using namespace boost::chrono;
	const char *levelNames[] = {"DEBUG", "INFO", "WARN", "ERROR", "ALARM", "FATAL"};
	std::ostringstream ss;
	LogMessage msg = message;

	if(m_param & LF_TIME)
		ss << time_fmt(timezone::local, m_timeFormat) << system_clock::now() << " ";

	if(m_param & LF_LEVEL)
		ss << levelNames[msg.level] << " ";

	if(m_param & LF_TEXT)
		ss << msg.text << " ";

	if(m_param & LF_POS)
		ss << "| " << msg.file << " (" << msg.func << "): line " << msg.line;

	if(m_param & LF_ENDL)
		ss << std::endl;
	
	msg.text = ss.str();
	m_sink->write(msg);
}

