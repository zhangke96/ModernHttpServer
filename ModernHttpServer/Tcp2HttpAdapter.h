#pragma once
#include "HttpCommon.h"


struct HttpEvent
{
	HttpEventType event;
	void *data;
};
class Tcp2HttpAdapter
{
public:
	Tcp2HttpAdapter(TcpServer &server, const std::string &address, int port) : tcpServer(server)
	{
		pthread_mutex_init(&protectEventMutex, nullptr);
		pthread_cond_init(&eventsConditon, nullptr);
		tcpServer.setAddressPort(address, port);
		tcpServer.setData(this);
		tcpServer.onConnect(onConnectHandler);
		tcpServer.onNewData(onReadHandler);
	}
	~Tcp2HttpAdapter()
	{
		pthread_mutex_destroy(&protectEventMutex);
		pthread_cond_destroy(&eventsConditon);
	}
	std::pair<bool, std::string> start()
	{
		bool startResult = tcpServer.start();
		if (startResult)
		{
			return { true, "" };
		}
		else
		{
			return { false, tcpServer.getError() };
		}
	}
	HttpEvent getEvent(bool isBlock = true) // HttpServer ��ȡ�µ����ݣ��������ӣ����յ�������
	{
		pthread_mutex_lock(&protectEventMutex);
		while (events.empty())
		{
			pthread_cond_wait(&eventsConditon, &protectEventMutex);
		}
		HttpEvent event = events.front();
		events.pop_front();
		pthread_mutex_unlock(&protectEventMutex);
		return event;
	}

	void addConnectionWriteEvent(const Connection *conn)
	{
		tcpServer.notifyChangeEpoll({ {conn->fd, EpollChangeOperation::ADD_WRITE} });
	}

	void addConnectionCloseEvent(const Connection *conn)
	{
		tcpServer.notifyChangeEpoll({ {conn->fd, EpollChangeOperation::CLOSE_IF_NO_WRITE} });
	}

	void addConnectionWrite(const Connection *conn, const WriteMeta *toWrite)
	{
		tcpServer.notifyCanWrite(conn->fd, *toWrite);
	}
	static OnConnectOperation onConnectHandler(const Connection *, void *);
	static void onReadHandler(const Connection *, const char *, size_t, void *);
private:
	TcpServer &tcpServer;
	std::deque<HttpEvent> events;
	pthread_mutex_t protectEventMutex;
	pthread_cond_t eventsConditon;
	void postEvent(HttpEvent newEvent)
	{
		pthread_mutex_lock(&protectEventMutex);
		events.push_back(newEvent);
		pthread_mutex_unlock(&protectEventMutex);
		pthread_cond_signal(&eventsConditon);
	}
};

