#pragma once

#include <string>
#include <sstream>
#include "Tcp2HttpAdapter.h"

typedef std::string(*urlHandler_t)(void *); // todo 补充函数参数

// HttpServer只考虑HTTP协议的解析，请求和响应的处理，TCP的读写通过TcpServer去处理
// 现在需要思考的是这两层怎么通信比较好
// TcpServer应该运行在一个单独的线程，两层之间可以通过消息队列来通信
// 以一个新请求举例子，TcpServer收到客户端连接，读取客户端的输入，如果长时间没有输入输出，就要关闭连接（可以设置超时时间）
// TcpServer收到输入就发给HttpServer，HttpServer建立一个新连接对象，开始解析HTTP,如果发现HTTP报文有问题，可以发消息给TcpServer去关闭这个连接
// 在收到了完整的HTTP请求，查找路由然后处理，生成HTTP响应结果，发给TcpServer去发送。TcpServer需要保证发送成功，如果发送失败，应该要告诉HttpServer

// 考虑HttpServer和TcpServer之间加一个broker，TcpServer只要专注于读写
#include "HttpConnection.h"
#include "TcpServer/TcpServer/log.h"

struct HttpInfo
{
	Connection connection;
	std::string url;
	std::map<std::string, std::string> headers;
};

typedef std::string(*NormalHandler_t)(HttpInfo);
typedef std::string(*StaticStrHandler_t)();
typedef std::string(*AutoDirSearchHandler_t)();
typedef std::string(*SingleFileHandler_t)();

enum class HttpHandlerType
{
	IllegalHandler,
	NormalHandler,
	StaticStrHandler,
	AutoDirSearchHandler,
	SingleFileHandler,
};


struct HttpHandler
{
	HttpHandlerType type;
	void *handler;
	std::string str;
	HttpHandler() : type(HttpHandlerType::IllegalHandler), handler(nullptr) {}
};
//struct HttpUrlLevel
//{
//	std::map<std::string, HttpHandler> handlers;
//	HttpUrlLevel *nextLevel;
//};
//struct HttpUrlLevel
//{
//	HttpHandler handler;
//	std::map<std::string, HttpUrlLevel> nextLevel;
//};
class HttpServer
{
public:
	HttpServer(const std::string add, int p) : address(add), port(p),
		tcpServer(), adapter(tcpServer, address, p)
	{}
	~HttpServer()
	{}

	bool addUrlHandler(const std::string &url, NormalHandler_t handler) 
	{
		HttpHandler newHandler;
		newHandler.handler = (void *)handler;
		newHandler.type = HttpHandlerType::NormalHandler;
		return addUrl(url, newHandler);
	}
	bool addUrlRetStaticStr(const std::string &url, const std::string &staticResult) 
	{ 
		HttpHandler newHandler;
		newHandler.str = staticResult;
		newHandler.type = HttpHandlerType::StaticStrHandler;
		return addUrl(url, newHandler);
	}
	bool addUrlAutoDirSearch(const std::string &url, const std::string &dir) 
	{ 
		HttpHandler newHandler;
		newHandler.str = dir;
		newHandler.type = HttpHandlerType::AutoDirSearchHandler;
		return addUrl(url, newHandler);
	}
	bool addUrlSingleFile(const std::string &url, const std::string &fileName) 
	{ 
		HttpHandler newHandler;
		newHandler.str = fileName;
		newHandler.type = HttpHandlerType::SingleFileHandler;
		return addUrl(url, newHandler);
	}

	bool set404Page(const std::string &page) 
	{
		str404 = page;
		return true; 
	}
	bool start() 
	{ 
		auto[result, errorStr] = adapter.start();
		if (!result)
		{
			std::cerr << "error: " << errorStr << std::endl;
			return false;
		}
		tcpServer.runServer();
		for (; ;)
		{
			HttpEvent event = adapter.getEvent();
			if (event.event == HttpEventType::NewConnection)
			{
				Connection newConnection = *((Connection *)(event.data));
				delete event.data;
				if (HttpConnections.find(newConnection) != HttpConnections.end())
				{
					Log(logger, Logger::LOG_ERROR, "Dup Connection");
				}
				else
				{
					HttpConnections.insert(std::make_pair(newConnection, HttpConnection()));
					//HttpConnections[newConnection] = HttpConnection();
				}
			}
			else if (event.event == HttpEventType::NewData)
			{
				TcpData newTcpData = *((TcpData *)(event.data));
				if (HttpConnections.find(newTcpData.connection) == HttpConnections.end())
				{
					Log(logger, Logger::LOG_ERROR, "Not exist Connection");
				}
				else
				{
					HttpConnectionState state = HttpConnections[newTcpData.connection].handleTcpData(&newTcpData);
					if (state == HttpConnectionState::Http_Connection_Error)
					{
						adapter.addConnectionCloseEvent(&newTcpData.connection);
						HttpConnections.erase(newTcpData.connection);
					}
					else if (state == HttpConnectionState::Http_Connection_Receiving)
					{

					}
					else if (state == HttpConnectionState::Http_Connection_ReceiveAll)
					{
						// 可以开始处理了
						std::string url = HttpConnections[newTcpData.connection].getUrl();
						HttpHandler handler = getUrlHandler(url);
						if (handler.type == HttpHandlerType::IllegalHandler)
						{
							// 404
							WriteMeta toWrite = string2WriteMeta(createNotFound());
							adapter.addConnectionWrite(&newTcpData.connection, &toWrite);
						}
						else if (handler.type == HttpHandlerType::NormalHandler)
						{
							HttpInfo info;
							info.connection = newTcpData.connection;
							info.url = url;
							info.headers = HttpConnections[newTcpData.connection].getHeaders();
							std::string result = reinterpret_cast<NormalHandler_t>(handler.handler)(info);
							std::string responseStr = generateResponse(200, result);
							WriteMeta toWrite = string2WriteMeta(responseStr);
							adapter.addConnectionWrite(&newTcpData.connection, &toWrite);
						}
						else if (handler.type == HttpHandlerType::StaticStrHandler)
						{

						}
						adapter.addConnectionWriteEvent(&newTcpData.connection);
						adapter.addConnectionCloseEvent(&newTcpData.connection);
						HttpConnections.erase(newTcpData.connection);
					}
				}
				delete[] newTcpData.data;
				delete event.data;
			}
		}
	}

private:
	std::string address;
	int port;
	TcpServer tcpServer;
	Tcp2HttpAdapter adapter;
	std::map<Connection, HttpConnection> HttpConnections;
	std::map<std::string, HttpHandler> HttpHandlers;
	std::string str404;
	std::vector<std::string> splitUrl(const std::string &url) const
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
	bool addUrl(const std::string &url, HttpHandler handler)
	{
		if (HttpHandlers.find(url) != HttpHandlers.end())
		{
			return false;
		}
		else
		{
			HttpHandlers[url] = handler;
		}
	}
	HttpHandler getUrlHandler(const std::string &url)
	{
		if (HttpHandlers.find(url) == HttpHandlers.end())
		{
			return HttpHandler();
		}
		else
		{
			return HttpHandlers[url];
		}
	}
	std::string generateResponse(int statusCode, const std::string &body)
	{
		time_t nowTime = time(NULL);
		struct tm tmTemp;
		const struct tm *toParse = gmtime_r(&nowTime, &tmTemp);
		char timebuf[40];
		assert(strftime(timebuf, 40, "Date: %a, %d %b %G %T %Z", toParse) != 0);
		std::string response("HTTP/1.1 200 OK\r\n"
			"Server: zhangke/0.1\r\n");
		response.append(timebuf);
		response.append("\r\nContent-Type: text/html\r\nContent-length: ");
		response.append(std::to_string(body.size()));
		response.append("\r\nConnection:close\r\n\r\n");
		response.append(body);
		return response;
	}
	/*bool addUrlHandlerToTree(const std::string &url, HttpHandler handler)
	{
		auto levelUrlResult = splitUrl(url);
		int i = 0;
		HttpUrlLevel *urlTreeNode = &urlHandlesTree;
		while (levelUrlResult[i] != "" && i < levelUrlResult.size())
		{
			if (urlTreeNode->nextLevel.find(levelUrlResult[i]) == urlTreeNode->nextLevel.end())
			{
				urlTreeNode->nextLevel.insert(std::make_pair(levelUrlResult[i], HttpUrlLevel()));
			}
			urlTreeNode = &(urlTreeNode->nextLevel[levelUrlResult[i]]);
			++i;
		}
		if (urlTreeNode->handler.type == HttpHandlerType::IllegalHandler)
		{
			urlTreeNode->handler = handler;
			return true;
		}
		else
		{
			return false;
		}
	}
	HttpHandler findUrlHandler(const std::string &url)
	{
		auto levelUrlResult = splitUrl(url);
		int i = 0;
		HttpUrlLevel *urlTreeNode = &urlHandlesTree;
		while (levelUrlResult[i] != "" && i < levelUrlResult.size())
		{
			if (urlTreeNode->nextLevel.find(levelUrlResult[i]) == urlTreeNode->nextLevel.end())
			{
				break;
			}
			else
			{
				urlTreeNode = &(urlTreeNode->nextLevel[levelUrlResult[i]]);
			}
			++i;
		}
		HttpHandler getHandler;
		if (urlTreeNode->nextLevel.find(levelUrlResult[i]) == urlTreeNode->nextLevel.end())
		{
			return getHandler;
		}
		else
		{
			return 
		}
	}
	*/
};
