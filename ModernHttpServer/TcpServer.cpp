#include "TcpServer.h"

TcpServer::~TcpServer()
{
}

bool operator<(const Connection &lhs, const Connection &rhs)
{
	return lhs.fd < rhs.fd;
}

WriteMeta string2WriteMeta(const std::string &str)
{
	char *buf = new char[str.length()];
	memcpy(buf, str.c_str(), str.length());
	return WriteMeta(buf, str.length());
}