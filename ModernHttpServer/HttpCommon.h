#pragma once
#include "TcpServer/TcpServer/TcpServer.h"
enum class HttpEventType : int8_t
{
	NewConnection,
	NewData
};
struct TcpData
{
	Connection connection;
	size_t length;
	const char *data;
	TcpData(Connection conn, size_t size, const char *d) :
		connection(conn), length(size), data(d)
	{}
};