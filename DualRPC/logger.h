#pragma once

#include <string>
#include <map>
#include <list>
#include <fstream>

#include <boost/function.hpp>
#include "boost/shared_ptr.hpp"
#include <boost/format.hpp>

enum ENUM_LOG_LEVEL
{
	DEBUG_LEVEL,
	INFO_LEVEL,
	WARN_LEVEL,
	ERROR_LEVEL,
	ALARM_LEVEL,
	FATAL_LEVEL,
};

struct LogMessage
{
	ENUM_LOG_LEVEL level;
	int senderID;
	std::string text;
	const char *file;
	const char *func;
	int line;

	LogMessage();
	LogMessage(ENUM_LOG_LEVEL level, int senderID, const std::string &text,
		const char *file, const char *func, int line);
};

struct ILogSink
{
	virtual void write(const LogMessage &message) = 0;
};

typedef boost::shared_ptr<ILogSink> ILogSinkPtr;

class Logger : public ILogSink
{
public:
	Logger();

	static Logger& instance();

	void registerSender(int id, const std::string &name, ILogSinkPtr sink);
	void removeSender(int id);

	void appendSink(int senderID, ILogSinkPtr sink);
	void removeSink(int senderID, ILogSinkPtr sink);

	void setLevel(ENUM_LOG_LEVEL level);

	void write(const LogMessage &message) override;

private:
	typedef std::list<ILogSinkPtr> SinkList;

	struct SenderInfo
	{
		std::string name;
		SinkList sinks;
	};
	
	typedef std::map<int, SenderInfo> SenderMap;	

	SenderMap m_senders;
	ENUM_LOG_LEVEL m_level;
};

#define LOG_STREAM(lvl, id, txt) Logger::instance().write(LogMessage(lvl, id, txt, __FILE__, __FUNCTION__, __LINE__))
#define LOG_STREAM_FMT(lvl, id, fmt, val) Logger::instance().write(LogMessage(lvl, id, (boost::format(fmt) % val).str(), __FILE__, __FUNCTION__, __LINE__))

#ifdef _DEBUG
#define LOG_DEBUG(sender, txt) LOG_STREAM(DEBUG_LEVEL, sender, txt)
#define LOG_DEBUG_FMT(sender, format, values) LOG_STREAM_FMT(DEBUG_LEVEL, sender, format, values)
#else
#define LOG_DEBUG(sender, format, values) 
#define LOG_DEBUG_FMT(sender, format, values) 
#endif

#define LOG_INFO(sender, txt) LOG_STREAM(INFO_LEVEL, sender, txt)
#define LOG_WARN(sender, txt) LOG_STREAM(WARN_LEVEL, sender, txt)
#define LOG_ERROR(sender, txt) LOG_STREAM(ERROR_LEVEL, sender, txt)
#define LOG_ALARM(sender, txt) LOG_STREAM(ALARM_LEVEL, sender, txt)
#define LOG_FATAL(sender, txt) LOG_STREAM(FATAL_LEVEL, sender, txt)

#define LOG_INFO_FMT(sender, format, values) LOG_STREAM_FMT(INFO_LEVEL, sender, format, values)
#define LOG_WARN_FMT(sender, format, values) LOG_STREAM_FMT(WARN_LEVEL, sender, format, values)
#define LOG_ERROR_FMT(sender, format, values) LOG_STREAM_FMT(ERROR_LEVEL, sender, format, values)
#define LOG_ALARM_FMT(sender, format, values) LOG_STREAM_FMT(ALARM_LEVEL, sender, format, values)
#define LOG_FATAL_FMT(sender, format, values) LOG_STREAM_FMT(FATAL_LEVEL, sender, format, values)


class LineFormatter : public ILogSink
{
public:
	enum {
		LF_TIME = 1, 
		LF_LEVEL = 2, 
		LF_POS = 4, 
		LF_TEXT = 8,
		LF_ENDL = 16
	};

	LineFormatter(ILogSinkPtr sink, 
		int param = LF_TIME | LF_LEVEL | LF_POS | LF_TEXT | LF_ENDL, 
		const char *time_format = "%Y-%m-%d %H:%M:%S");

	void write(const LogMessage &message) override;

private:
	ILogSinkPtr m_sink;
	const char *m_timeFormat;
	int m_param;
};

template<class Stream> class LogStreamSinkAdapter : public ILogSink
{
public:
	LogStreamSinkAdapter(Stream &stream) : m_stream(stream) {}

	void write(const LogMessage &message) override
	{
		m_stream << message.text;
		m_stream.flush();
	}

private:
	Stream &m_stream;
};

