#pragma once
#include "TcpServer/TcpServer/TcpServer.h"
enum class HttpEventType : int8_t
{
	NoEvent,	// ��Ч��Event
	NewConnection,
	NewData,
	PeerShutdown,
	CanWrite
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
//std::string generateResponseLine(int responseCode);
//std::string generateTimeStr();
//std::string generateContentType();
//std::string generateLengthInd(size_t length = 0, bool isChunked = false);
//std::string generateConnAlive(bool isKeepAlive = true);
//
//// ����HTTP��Ӧ��ͷ������
//std::string generateHttpResponseHead(int responseCode, bool isKeepAlive = true, size_t length = 0, bool isChunked = false)
//{
//	std::string response = generateResponseLine(responseCode);
//	response += generateTimeStr();
//	response += generateContentType();
//	response += generateLengthInd(length, isChunked);
//	response += generateConnAlive(isKeepAlive);
//	return response + "\r\n";
//}