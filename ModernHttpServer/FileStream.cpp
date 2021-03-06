#include "FileStream.h"

void FileStream::initialize()
{
	dirfd.reset(open(dir.c_str(), O_RDONLY));
	if (dirfd.getfd() == -1)
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
		fd.reset(-1);
		return;
	}

	fd.reset(openat(dirfd.getfd(), filename.c_str(), O_RDONLY));
	if (fd.getfd() == -1)
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
	if (fd.getfd() == -1)
	{
		return -1;
	}
	return fd.read(buffer, size);
}
