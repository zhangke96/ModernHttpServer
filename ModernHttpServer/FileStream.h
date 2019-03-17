#pragma once
#include "TcpServer/TcpServer/tool.h"
#include "TcpServer/TcpServer/log.h"
#include "TcpServer/TcpServer/aux_class.h"
#include <fcntl.h>
#include <sys/stat.h>
// 后期改用mmap，实现零拷贝
class FileStream :
	public CharStream
{
public:
	FileStream(const std::string &dir, std::string filename) : dir(dir), filename(filename), 
		specificErrorCode(FileStreamError::NoError)
	{
		initialize();
	}
	bool ifError() const
	{
		return fd.getfd() == -1;
	}
	enum class FileStreamError
	{
		NoError,
		NotExist,
		Forbidden,
		SomeError
	};
	FileStreamError getErrorCode() const
	{
		return specificErrorCode;
	}
	ssize_t read(char *, size_t size) override;
	bool end() const override
	{
		return fd.endOfFile();
	}
private:
	std::string dir;
	std::string filename;
	fdwrap dirfd;
	fdwrap fd;
	void initialize();
	FileStreamError specificErrorCode;
};

