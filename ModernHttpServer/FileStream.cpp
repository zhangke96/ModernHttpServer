#include "FileStream.h"

FileStream::~FileStream()
{
}

void FileStream::initialize()
{
	int dirfd = open(dir.c_str(), O_RDONLY);
	if (dirfd == -1)
	{
		if (errno == ENONET)
		{
			specificErrorCode = FileStreamError::NotExist;
		}
		else if (errno = EACCES)
		{
			specificErrorCode = FileStreamError::Forbidden;
		}
		else
		{
			specificErrorCode = FileStreamError::SomeError;
		}
		fd = -1;
		return;
	}
	fd = openat(dirfd, filename.c_str(), O_RDONLY);
	if (fd == -1)
	{
		if (errno == ENONET)
		{
			specificErrorCode = FileStreamError::NotExist;
		}
		else if (errno = EACCES)
		{
			specificErrorCode = FileStreamError::Forbidden;
		}
		else
		{
			specificErrorCode = FileStreamError::SomeError;
		}
	}
}

ssize_t FileStream::read(char *buffer, size_t size)
{
	if (fd == -1)
	{
		return -1;
	}
	return ::read(fd, buffer, size);
}
