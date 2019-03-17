#pragma once

#include <string>
#include <sstream>
#include "Tcp2HttpAdapter.h"
#include "FileStream.h"

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
#include "HttpResponseGenerator.h"
#include <memory>

struct HttpInfo
{
	TcpConnection connection;
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
				TcpConnection newConnection = *((TcpConnection *)(event.data));
				delete event.data;
				if (HttpConnections.find(newConnection) != HttpConnections.end())
				{
					Log(logger, Logger::LOG_ERROR, "Dup TcpConnection");
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
					Log(logger, Logger::LOG_ERROR, "Not exist TcpConnection");
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
						bool ifCloseThisLoopEnd = true;	// 标记这个连接在这次处理之后是否关闭
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
								ssize_t length = stream.read(buffer, 4095);
								buffer[length] = '\0';
								WriteMeta *toWrite2 = nullptr;
								if (stream.end())
								{
									toWrite2 = new WriteMeta(string2WriteMeta(responseGenerator.getResponseBody(buffer, true)));
								}
								else
								{
									toWrite2 = new WriteMeta(string2WriteMeta(responseGenerator.getResponseBody(buffer, false)));
									// 还要添加到未完成队列中
									if (workDeque.find(newTcpData.connection) == workDeque.cend())
									{
										workDeque[newTcpData.connection] = {std::shared_ptr<HttpResponseGenerator>(new HttpResponseGenerator(responseGenerator)), std::shared_ptr<CharStream>(new FileStream(stream))};
									}
									ifCloseThisLoopEnd = false;
								}
								adapter.addConnectionWrite(&newTcpData.connection, toWrite2);
								delete toWrite2;
							}
						}
						adapter.addConnectionWriteEvent(&newTcpData.connection);
						if (ifCloseThisLoopEnd)
						{
							adapter.addConnectionCloseEvent(&newTcpData.connection);
							HttpConnections.erase(newTcpData.connection);
						}
					}
				}
				delete[] newTcpData.data;
				delete event.data;
			}
			else if (event.event == HttpEventType::PeerShutdown)
			{
				HttpConnections.erase(*(TcpConnection*)(event.data));
				// 还有一些数据需要删除
				delete event.data;
			}
			else if (event.event == HttpEventType::CanWrite)
			{
				Connection canWriteConnection = *(Connection*)(event.data);
				if (workDeque.find(canWriteConnection) != workDeque.cend())
				{
					auto &temp = workDeque[canWriteConnection];
					char buffer[4096];
					ssize_t length = temp.second->read(buffer, 4095);
					if (length >= 0)
					{
						buffer[length] = '\0';
					}
					WriteMeta *toWrite = nullptr;
					if (temp.second->end())
					{
						toWrite = new WriteMeta(string2WriteMeta(temp.first->getResponseBody(buffer, true)));
						adapter.addConnectionCloseEvent(&canWriteConnection);
						HttpConnections.erase(canWriteConnection);
						workDeque.erase(canWriteConnection);
					}
					else
					{
						toWrite = new WriteMeta(string2WriteMeta(temp.first->getResponseBody(buffer, false)));
					}
					adapter.addConnectionWrite(&canWriteConnection, toWrite);
					// 这里需要delete
				}
			}
		}
	}

private:
	std::string address;
	int port;
	TcpServer tcpServer;
	Tcp2HttpAdapter adapter;
	std::map<TcpConnection, HttpConnection> HttpConnections;
	std::map<std::string, HttpHandler> HttpHandlers;
	std::string str404;
	HttpUrlLevel urlHandlesTree;
	std::map<Connection, std::pair<std::shared_ptr<HttpResponseGenerator>, std::shared_ptr<CharStream>>> workDeque;
	std::vector<std::string> splitUrl(const std::string &url) const;
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
	}
	// 如果是AutoDirSearchHandler，第二个返回值是文件名
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
			if (nextLevelHandlerMap.find(levelUrlResult[i]) == nextLevelHandlerMap.cend())	// 下一层没有注册了
			{
				if (index->thisHandler.type == HttpHandlerType::AutoDirSearchHandler)	// 文件夹搜索，匹配到前缀
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
				index = nextLevelHandlerMap[levelUrlResult[i]];	// 向下一层
			}
			++i;
		}
		return { index->thisHandler, "" };
	}
	
};
