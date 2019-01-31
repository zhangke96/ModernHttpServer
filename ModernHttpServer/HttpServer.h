#pragma once

#include <string>
#include <sstream>
#include "Tcp2HttpAdapter.h"
#include "FileStream.h"

typedef std::string(*urlHandler_t)(void *); // todo ���亯������

// HttpServerֻ����HTTPЭ��Ľ������������Ӧ�Ĵ���TCP�Ķ�дͨ��TcpServerȥ����
// ������Ҫ˼��������������ôͨ�űȽϺ�
// TcpServerӦ��������һ���������̣߳�����֮�����ͨ����Ϣ������ͨ��
// ��һ������������ӣ�TcpServer�յ��ͻ������ӣ���ȡ�ͻ��˵����룬�����ʱ��û�������������Ҫ�ر����ӣ��������ó�ʱʱ�䣩
// TcpServer�յ�����ͷ���HttpServer��HttpServer����һ�������Ӷ��󣬿�ʼ����HTTP,�������HTTP���������⣬���Է���Ϣ��TcpServerȥ�ر��������
// ���յ���������HTTP���󣬲���·��Ȼ��������HTTP��Ӧ���������TcpServerȥ���͡�TcpServer��Ҫ��֤���ͳɹ����������ʧ�ܣ�Ӧ��Ҫ����HttpServer

// ����HttpServer��TcpServer֮���һ��broker��TcpServerֻҪרע�ڶ�д
#include "HttpConnection.h"
#include "TcpServer/TcpServer/log.h"
#include "HttpResponseGenerator.h"

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
struct HttpUrlLevel
{
	HttpHandler thisHandler;
	std::map<std::string, HttpUrlLevel*> nextLevelHandler;
};
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
		return addUrlHandlerToTree(url, newHandler);
	}
	bool addUrlRetStaticStr(const std::string &url, const std::string &staticResult) 
	{ 
		HttpHandler newHandler;
		newHandler.str = staticResult;
		newHandler.type = HttpHandlerType::StaticStrHandler;
		return addUrlHandlerToTree(url, newHandler);
	}
	bool addUrlAutoDirSearch(const std::string &url, const std::string &dir) 
	{ 
		HttpHandler newHandler;
		newHandler.str = dir;
		newHandler.type = HttpHandlerType::AutoDirSearchHandler;
		return addUrlHandlerToTree(url, newHandler);
	}
	bool addUrlSingleFile(const std::string &url, const std::string &fileName) 
	{ 
		HttpHandler newHandler;
		newHandler.str = fileName;
		newHandler.type = HttpHandlerType::SingleFileHandler;
		return addUrlHandlerToTree(url, newHandler);
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
						// ���Կ�ʼ������
						std::string url = HttpConnections[newTcpData.connection].getUrl();
						auto handlerFilename = findUrlHandler(url);
						HttpHandler handler = handlerFilename.first;
						if (handler.type == HttpHandlerType::IllegalHandler)
						{
							// 404
							HttpResponseGenerator responseGenerator;
							responseGenerator.setResponseCode(404);
							responseGenerator.setKeepAlive(false);
							responseGenerator.setLengthInd(str404.length());
							WriteMeta toWrite = string2WriteMeta(responseGenerator.getResponseHead());
							adapter.addConnectionWrite(&newTcpData.connection, &toWrite);
							WriteMeta toWrite2 = string2WriteMeta(responseGenerator.getResponseBody(str404));
							adapter.addConnectionWrite(&newTcpData.connection, &toWrite2);
						}
						else if (handler.type == HttpHandlerType::NormalHandler)
						{
							HttpInfo info;
							info.connection = newTcpData.connection;
							info.url = url;
							info.headers = HttpConnections[newTcpData.connection].getHeaders();
							std::string result = reinterpret_cast<NormalHandler_t>(handler.handler)(info);
							//std::string responseStr = generateResponse(200, result);
							HttpResponseGenerator responseGenerator;
							responseGenerator.setResponseCode(200);
							responseGenerator.setKeepAlive(true);
							responseGenerator.setLengthInd(0, true);
							WriteMeta toWrite = string2WriteMeta(responseGenerator.getResponseHead());
							adapter.addConnectionWrite(&newTcpData.connection, &toWrite);
							WriteMeta toWrite2 = string2WriteMeta(responseGenerator.getResponseBody(result, true));
							adapter.addConnectionWrite(&newTcpData.connection, &toWrite2);
						}
						else if (handler.type == HttpHandlerType::AutoDirSearchHandler)
						{
							FileStream stream(handler.str, handlerFilename.second);
							if (stream.ifError())
							{
								if (stream.getErrorCode() == FileStream::FileStreamError::NotExist)
								{
									HttpResponseGenerator responseGenerator;
									responseGenerator.setResponseCode(404);
									responseGenerator.setKeepAlive(false);
									responseGenerator.setLengthInd(str404.length());
									WriteMeta toWrite = string2WriteMeta(responseGenerator.getResponseHead());
									adapter.addConnectionWrite(&newTcpData.connection, &toWrite);
									WriteMeta toWrite2 = string2WriteMeta(str404);
									adapter.addConnectionWrite(&newTcpData.connection, &toWrite2);
								}
								else if (stream.getErrorCode() == FileStream::FileStreamError::Forbidden)
								{
									HttpResponseGenerator responseGenerator;
									responseGenerator.setResponseCode(403);
									responseGenerator.setKeepAlive(false);
									std::string str403("403 Forbidden");
									responseGenerator.setLengthInd(str403.length());
									WriteMeta toWrite = string2WriteMeta(responseGenerator.getResponseHead());
									adapter.addConnectionWrite(&newTcpData.connection, &toWrite);
									WriteMeta toWrite2 = string2WriteMeta(str403);
									adapter.addConnectionWrite(&newTcpData.connection, &toWrite2);
								}
							}
							else
							{
								HttpResponseGenerator responseGenerator;
								responseGenerator.setLengthInd(0, true);
								WriteMeta toWrite = string2WriteMeta(responseGenerator.getResponseHead());
								adapter.addConnectionWrite(&newTcpData.connection, &toWrite);
								char buffer[4096];
								ssize_t length = stream.read(buffer, 4096);
								buffer[length] = '\0';
								WriteMeta toWrite2 = string2WriteMeta(responseGenerator.getResponseBody(buffer, true));
								adapter.addConnectionWrite(&newTcpData.connection, &toWrite2);
							}
						}
						adapter.addConnectionWriteEvent(&newTcpData.connection);
						adapter.addConnectionCloseEvent(&newTcpData.connection);
						HttpConnections.erase(newTcpData.connection);
					}
				}
				delete[] newTcpData.data;
				delete event.data;
			}
			else if (event.event == HttpEventType::PeerShutdown)
			{
				HttpConnections.erase(*(Connection*)(event.data));
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
	HttpUrlLevel urlHandlesTree;
	//std::deque<std::pair<Connection, CharStream>> workDeque;
	std::vector<std::string> splitUrl(const std::string &url) const;
	/*bool addUrl(const std::string &url, HttpHandler handler)
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
	}*/
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
	bool addUrlHandlerToTree(const std::string &url, HttpHandler handler)
	{
		auto levelUrlResult = splitUrl(url);
		if (levelUrlResult.size() == 0 || levelUrlResult[0] != "")
		{
			return false;
		}
		HttpUrlLevel *index = &urlHandlesTree;
		int i = 1;
		while (i < levelUrlResult.size())
		{
			auto &nextLevelHandlerMap = index->nextLevelHandler;
			if (nextLevelHandlerMap.find(levelUrlResult[i]) == nextLevelHandlerMap.cend())
			{
				nextLevelHandlerMap.insert({ levelUrlResult[i], new HttpUrlLevel() });
			}
			index = nextLevelHandlerMap[levelUrlResult[i]];
			++i;
		}
		if (index->thisHandler.type == HttpHandlerType::IllegalHandler)
		{
			index->thisHandler = handler;
			return true;
		}
		else
		{
			return false;
		}
		/*while (levelUrlResult[i] != "" && i < levelUrlResult.size())
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
		}*/
	}
	// �����AutoDirSearchHandler���ڶ�������ֵ���ļ���
	std::pair<HttpHandler, std::string> findUrlHandler(const std::string &url)
	{
		auto levelUrlResult = splitUrl(url);
		if (levelUrlResult.size() == 0 || levelUrlResult[0] != "")
		{
			return { HttpHandler(), "" };
		}
		HttpUrlLevel *index = &urlHandlesTree;
		int i = 1;
		while (i < levelUrlResult.size())
		{
			auto &nextLevelHandlerMap = index->nextLevelHandler;
			if (nextLevelHandlerMap.find(levelUrlResult[i]) == nextLevelHandlerMap.cend())	// ��һ��û��ע����
			{
				if (index->thisHandler.type == HttpHandlerType::AutoDirSearchHandler)	// �ļ���������ƥ�䵽ǰ׺
				{
					std::string filename = "";
					for (; i < levelUrlResult.size(); ++i)
					{
						filename.append(levelUrlResult[i]);
						if (i < levelUrlResult.size() - 1)
						{
							filename.append("/");
						}
					}
					return { index->thisHandler, filename };
				}
				else
				{
					return { HttpHandler(), "" };
				}
			}
			else
			{
				index = nextLevelHandlerMap[levelUrlResult[i]];	// ����һ��
			}
			++i;
		}
		return { index->thisHandler, "" };
	}
	
};
