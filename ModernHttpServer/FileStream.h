#pragma once
#include "TcpServer/TcpServer/tool.h"
#include <fcntl.h>
// 后期改用mmap，实现零拷贝
class FileStream :
	public CharStream
{
public:
	FileStream(const std::string &dir, std::string filename) : dir(dir), filename(filename), 
		dirfd(-1), fd(-1), specificErrorCode(FileStreamError::NoError)
	{
		initialize();
	}
	~FileStream();
	bool ifError() const
	{
		return fd == -1;
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
private:
	std::string dir;
	std::string filename;
	int dirfd;
	int fd;
	void initialize();
	FileStreamError specificErrorCode;
};

