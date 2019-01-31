#pragma once
#include <map>
#include <string>
#include "TcpServer/TcpServer/log.h"

class HttpResponseGenerator
{
public:
	enum class HttpVersion
	{
		V1_0,
		V1_1,
		V2_0
	};
public:
	HttpResponseGenerator() : httpVersion(HttpVersion::V1_1),
		responseCode(200), mime("html"), length(0), isChunked(false), isKeepAlive(true)
	{}

	void setHttpVersion(HttpVersion version)
	{
		httpVersion = version;
	}
	void setResponseCode(int responseCode)
	{
		this->responseCode = responseCode;
	}
	void setMime(std::string fileSuffix)
	{
		mime = fileSuffix;
	}
	void setLengthInd(size_t length = 0, bool isChunked = false)
	{
		this->length = length;
		this->isChunked = isChunked;
	}
	void setKeepAlive(bool alive)
	{
		isKeepAlive = alive;
	}
	std::string getResponseHead()
	{
		return generateResponseLine() +
			generateServerName() +
			generateTimeStr() +
			generateContentType() +
			generateLengthInd() +
			generateConnAlive() +
			"\r\n";
	}
	std::string getResponseBody(const std::string &frame, bool ifEnd = false);	// ifEnd 用于chunked 方式

private:
	static const std::map<std::string, std::string> mineMap;
	static const std::map<int, std::string> responseCodeStrMap;
	static std::string getMineStr(const std::string fileSuffix);
	static std::string getCodeStr(int code);
	HttpVersion httpVersion;
	int responseCode;
	std::string mime;
	size_t length;
	bool isChunked;
	bool isKeepAlive;
	std::string generateResponseLine() const;
	std::string generateServerName() const;
	std::string generateTimeStr() const;
	std::string generateLengthInd() const;
	std::string generateConnAlive() const;
	std::string genreateHttpVerStr() const;
	std::string generateContentType() const;
};

