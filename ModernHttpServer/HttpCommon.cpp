#include "HttpCommon.h"
const std::map<int, std::string> responseCodeStrMap
{
	{200, "OK"},
	{302, "FOUND"},
	{304, "NOT MODIFIED"},
	{400, "BAD REQUEST"},
	{403, "FORBIDDEN"},
	{404, "NOT FOUND"},
	{500, "INTERNAL SERVER ERROR"}
};
//std::string generateResponseLine(int responseCode)
//{
//	auto index = responseCodeStrMap.find(responseCode);
//	if (index != responseCodeStrMap.cend())
//	{
//		return "HTTP/1.1 " + std::to_string(responseCode) + " " + index->second + "\r\n";
//	}
//	else
//	{
//		return "HTTP/1.1 500 INTERNAL SERVER ERROR\r\n";
//	}
//}
//std::string generateTimeStr()
//{
//	time_t nowTime = time(NULL);
//	struct tm tmTemp;
//	const struct tm *toParse = gmtime_r(&nowTime, &tmTemp);
//	char timebuf[40];
//	assert(strftime(timebuf, 40, "Date: %a, %d %b %G %T %Z\r\n", toParse) != 0);
//	return timebuf;
//}
//std::string generateContentType()
//{
//	return "Content-Type: text/html\r\n";
//}
//std::string generateLengthInd(size_t length = 0, bool isChunked = false)
//{
//	if (isChunked)
//	{
//		return "Transfer-Encoding: chunked\r\n";
//	}
//	else
//	{
//		return "Content-Length: " + std::to_string(length) + "\r\n";
//	}
//}
//std::string generateConnAlive(bool isKeepAlive = true)
//{
//	if (isKeepAlive)
//	{
//		return "Connection:keep-alive\r\n";
//	}
//	else
//	{
//		return "Connection:close\r\n";
//	}
//}