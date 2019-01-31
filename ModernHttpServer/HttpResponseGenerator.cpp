#include "HttpResponseGenerator.h"


const std::map<std::string, std::string> HttpResponseGenerator::mineMap 
{
	{"html", "text/html"},
	{"htm", "text/html"},
	{"shtml", "text/html"},
	{"css", "text/css"},
	{"xml", "text/xml"},
	{"gif", "image/gif"},
	{"jpeg", "image/jpeg"},
	{"jpg", "image/jpeg"},
	{"js", "application/javascript"}
};
const std::map<int, std::string> HttpResponseGenerator::responseCodeStrMap
{
	{200, "OK"},
	{302, "FOUND"},
	{304, "NOT MODIFIED"},
	{400, "BAD REQUEST"},
	{403, "FORBIDDEN"},
	{404, "NOT FOUND"},
	{500, "INTERNAL SERVER ERROR"}
};
std::string HttpResponseGenerator::genreateHttpVerStr() const
{
	if (httpVersion == HttpVersion::V1_1)
	{
		return "HTTP/1.1";
	}
	else if (httpVersion == HttpVersion::V1_0)
	{
		return "HTTP/1.0";
	}
	else
	{
		Log(logger, Logger::LOG_WARN, "not support HTTP VERSION");
		return "HTTP/1.1";
	}
}
std::string HttpResponseGenerator::getMineStr(const std::string fileSuffix)
{
	if (auto index(mineMap.cend()); (index = mineMap.find(fileSuffix)) == mineMap.cend())
	{
		Log(logger, Logger::LOG_WARN, "not found mine type: " + fileSuffix);
		return "text/html";
	}
	else
	{
		return index->second;
	}
}
std::string HttpResponseGenerator::getCodeStr(int code)
{
	if(auto index(responseCodeStrMap.cend()); (index = responseCodeStrMap.find(code)) == responseCodeStrMap.cend())
	{
		Log(logger, Logger::LOG_WARN, "not found response code: " + code);
		return "INTERNAL SERVER ERROR";
	}
	else
	{
		return index->second;
	}
}
std::string HttpResponseGenerator::generateResponseLine() const
{
	return genreateHttpVerStr() + " " + std::to_string(responseCode) + " " + getCodeStr(responseCode) + "\r\n";
}
std::string HttpResponseGenerator::generateServerName() const
{
	return "Server: zhangke/0.1\r\n";
}
std::string HttpResponseGenerator::generateTimeStr() const
{
	time_t nowTime = time(NULL);
	struct tm tmTemp;
	const struct tm *toParse = gmtime_r(&nowTime, &tmTemp);
	char timebuf[40];
	assert(strftime(timebuf, 40, "Date: %a, %d %b %G %T %Z\r\n", toParse) != 0);
	return timebuf;
}
std::string HttpResponseGenerator::generateLengthInd() const
{
	if (isChunked)
	{
		return "Transfer-Encoding: chunked\r\n";
	}
	else
	{
		return "Content-Length: " + std::to_string(length) + "\r\n";
	}
}
std::string HttpResponseGenerator::generateConnAlive() const
{
	if (isKeepAlive)
	{
		return "Connection:keep-alive\r\n";
	}
	else
	{
		return "Connection:close\r\n";
	}
}
std::string HttpResponseGenerator::generateContentType() const
{
	return "Content-Type: " + getMineStr(mime) + "\r\n";
}
std::string HttpResponseGenerator::getResponseBody(const std::string &frame, bool ifEnd)
{
	if (!isChunked)
	{
		return frame;
	}
	else
	{
		char sizebuf[15];
		snprintf(sizebuf, 14, "%x\r\n", frame.size());
		if (!ifEnd)
		{
			return sizebuf + frame + "\r\n";
		}
		else
		{
			std::string temp(sizebuf + frame + "\r\n0\r\n\r\n");
			std::cout << temp << std::endl;
			std::cout << temp.size() << std::endl;
			return temp;
		}
	}
}