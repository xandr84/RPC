#include "stdafx.h"
#include "FileSystemObject.h"

FileSystemObject::FileSystemObject()
{
	registerMethod("openFile", boost::bind(&FileSystemObject::openFile, this, _1));
	registerMethod("listDir", boost::bind(&FileSystemObject::listDir, this, _1));
}

DualRPC::Variant FileSystemObject::openFile(const DualRPC::Variant &args)
{
	std::string name = args.item("path").toString();
	int mode = (int)args.item("mode", 0).toInt();
	return DualRPC::IObjectPtr(new FileObject(name.c_str(), mode));
}

DualRPC::Variant FileSystemObject::listDir(const DualRPC::Variant &args)
{
	return DualRPC::Variant();
}
	
///////////////////////////////////////////////////////////////

FileObject::FileObject(const char *name, std::ios_base::openmode mode) :
	m_stream(name, mode)
{
	registerMethod("read", boost::bind(&FileObject::read, this, _1));
	registerMethod("write", boost::bind(&FileObject::write, this, _1));

	m_stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
}

DualRPC::Variant FileObject::read(const DualRPC::Variant &args)
{
	__int64 size = args.item("size", -1).toInt();
	__int64 seek = args.item("seek", -1).toInt();
	m_writer = args.item("writer", DualRPC::IObjectPtr()).toObject();

	if(size < 0)
	{
		m_stream.seekg(0, std::ios_base::end);
		size = m_stream.tellg();
		m_stream.seekg(0, std::ios_base::beg);
	}

	if(seek >= 0)
		m_stream.seekg(seek, std::ios_base::beg);

	if(m_writer)
	{
		m_readResult.reset(new DualRPC::FutureResult);
		iterRead(size);
		return m_readResult;
	}
	else
	{
		return readBlock(size);
	}
}

DualRPC::Variant FileObject::readBlock(__int64 size)
{
	DualRPC::Variant res("");
	std::string &data = res.getString();
	data.resize(int(size));
	m_stream.read(&data[0], size);
	__int64 c = m_stream.gcount();
	data.resize(int(c));
	return res;
}

DualRPC::Variant FileObject::iterRead(__int64 restsize, __int64 lastsize, 
	const steady_clock::time_point &lasttp, const DualRPC::Variant &v)
{
	if(restsize == 0)
	{
		m_readResult->callback(DualRPC::Variant(""));
	}
	else
	{
		steady_clock::time_point tp = steady_clock::now();
		boost::chrono::duration<double> time_span = 
			boost::chrono::duration_cast<boost::chrono::duration<double>>(tp - lasttp);

		__int64 size = __int64(lastsize / time_span.count());
		if(size < 64*1024)
			size = 64*1024;
		if(size > restsize)
			size = restsize;
		
		DualRPC::FutureResultPtr f;
		m_writer->call("", readBlock(size), false, -1, f);
		f->addCallback(boost::bind(&FileObject::iterRead, this, 
			restsize-size, size, tp, _1));
	}
	return DualRPC::Variant();
}
	
DualRPC::Variant FileObject::write(const DualRPC::Variant &args)
{
	return DualRPC::Variant();
}