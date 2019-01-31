#include "HttpServer.h"

/*
"/" 结果为 [""]
"/zhang" 结果为 ["", "zhang"]
"/zhang/" 结果为 ["", "zhang"]
"" 结果为[]
*/
std::vector<std::string> HttpServer::splitUrl(const std::string &url) const
{
	std::istringstream is(url);
	std::string word;
	std::vector<std::string> result;
	while (std::getline(is, word, '/'))
	{
		result.push_back(word);
	}
	return result;
}