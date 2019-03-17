#pragma once
#include "TcpServer/TcpServer/TcpServer.h"
enum class HttpEventType : int8_t
{
	NoEvent,	// ÎÞÐ§µÄEvent
	NewConnection,
	NewData,
	PeerShutdown
};
struct TcpData
{
	TcpConnection connection;
	size_t length;
	const char *data;
	TcpData(TcpConnection conn, size_t size, const char *d) :
		connection(conn), length(size), data(d)
	{}
};
