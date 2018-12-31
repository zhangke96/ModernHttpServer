#pragma once
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <cassert>
#include "log.h"
#include <map>
#include <deque>
#include <sys/uio.h>
#include <vector>
#include <set>

#define BUFFER_SIZE (80 * 1024)		// ÿ�ζ�д������С

struct Connection
{
	int fd;
	struct sockaddr_in address;
};
struct WriteMeta // ��¼Ҫwrite����Ϣ����ʱ��ĳ�����ָ�� todo
{
	const char *buffer;
	int totalLen;
	int toWriteLen;
	WriteMeta() : buffer(nullptr), totalLen(0), toWriteLen(0) {}
	WriteMeta(const char *buf, int len) : buffer(buf), totalLen(len), toWriteLen(len) {}
};
enum class OnConnectOperation
{
	CLOSE_IT,
	ADD_READ,
	ADD_WRITE
};
enum class EpollChangeOperation
{
	ADD_WRITE,
	CLOSE_IT,
	CLOSE_IF_NO_WRITE,
};
class TcpServer;

typedef OnConnectOperation (*OnConnectHandle_t)(const Connection*);
typedef void (*OnReadHandle_t)(const Connection*, const char *, ssize_t);
typedef std::deque<WriteMeta>(*OnCanWriteHandle_t)(const Connection*);
typedef std::vector<std::pair<int, EpollChangeOperation>>(*OnChangeEpoll_t)();

class TcpServer
{
public:
	TcpServer() : serverFd(-1), epollFd(-1), 
		readyForStart(false), onConnectHandler(nullptr),
		onReadHandler(nullptr), onCanWriteHandler(nullptr), 
		onChangeEpollHandler(nullptr), notifyFd(-1) {}

	~TcpServer();

	TcpServer(const TcpServer&) = delete;
	TcpServer & operator=(const TcpServer&) = delete;

	bool setAddressPort(const std::string &addr, int port)
	{
		struct in_addr tempAddr;
		if (inet_pton(AF_INET, addr.c_str(), &tempAddr) != 1)
		{
			return false;
		}
		if (port >= 65535 || port <= 0)
		{
			return false;
		}
		// ���浽����ĵ�ַ�ṹ��
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(tempAddr.s_addr);
		address.sin_port = htons(port);
		readyForStart = true;
		return true;
	}

	std::string getAddress() const
	{
		char buffer[INET_ADDRSTRLEN];
		in_addr_t addr = ntohl(address.sin_addr.s_addr);
		if (inet_ntop(AF_INET, &addr, buffer, INET_ADDRSTRLEN) == NULL)
		{
			return "";
		}
		else
		{
			return buffer;
		}
	}
	int getPort() const
	{
		return ntohs(address.sin_port);
	}
	bool onConnect(OnConnectHandle_t handler) //
	{
		if (onConnectHandler)
		{
			return false;
		}
		else
		{
			onConnectHandler = handler;
		}
		return true;
	}
	bool onNewData(OnReadHandle_t handler)
	{
		if (onReadHandler)
		{
			return false;
		}
		else
		{
			onReadHandler = handler;
		}
		return true;
	}
	/*bool onWriteFail();
	bool onReadFail(); */
	bool onCanSendData(OnCanWriteHandle_t handler)	// ����׽�������write��TcpServer����û�ȡָ��fd������
	{
		if (onCanWriteHandler)
		{
			return false;
		}
		else
		{
			onCanWriteHandler = handler;
		}
		return true;
	}
	bool onNeedChangeEpoll(OnChangeEpoll_t handler)
	{
		if (onChangeEpollHandler)
		{
			return false;
		}
		else
		{
			onChangeEpollHandler = handler;
		}
		return true;
	}
	bool setNotifyFd(int fd)
	{
		if (notifyFd != -1)
		{
			return false;
		}
		else
		{
			notifyFd = fd;
			if (!setNoBlock(notifyFd))
			{
				return false;
			}
		}
		return true;
	}

	//bool closeConnection(); // �ر�ĳ������
	
	bool start()
	{
		if (!readyForStart)
		{
			return false;
		}
		if (!ignoreSignal())	// ����SIG_PIPE
		{
			return false;
		}
		int reuse = 1;
		if ((serverFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if (!setNoBlock(serverFd))
		{
			return false;
		}
		if (bind(serverFd, (sockaddr *)&address, sizeof(address)) < 0)  /* bind socket fd to address addr */
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if (listen(serverFd, 1024) < 0)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if ((epollFd = epoll_create1(0)) == -1)
		{
			recordError(__FILE__, __LINE__);
			close(serverFd);
			return false;
		}
		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = serverFd;
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &event) == -1)
		{
			recordError(__FILE__, __LINE__);
			close(serverFd);
			close(epollFd);
			return false;
		}
		event.data.fd = notifyFd;
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, notifyFd, &event) == -1)
		{
			recordError(__FILE__, __LINE__);
			close(serverFd);
			close(epollFd);
			return false;
		}
		return true;
	}
	// bool stop(); // ���Ǻܺ�ʵ�֣��Ȳ�ʵ��

	void runServer()
	{
		struct epoll_event *toHandleEvents;
		toHandleEvents = new struct epoll_event[1024];
		assert(toHandleEvents);
		for (; ;)
		{
			if (onChangeEpollHandler)
			{
				auto toHandle = onChangeEpollHandler();
				for (auto c : toHandle)
				{
					if (c.second == EpollChangeOperation::ADD_WRITE)
					{
						addToWrite(c.first);
					}
					else if (c.second == EpollChangeOperation::CLOSE_IF_NO_WRITE)
					{
						closeWhenWriteFinish.insert(c.first);
					}
				}
			}
			int epollRet = epoll_wait(epollFd, toHandleEvents, 1024, -1);
			if (epollRet < 0)
			{
				recordError(__FILE__, __LINE__);
				Log(logger, Logger::LOG_ERROR, errorString);
			}
			else
			{
				for (int i = 0; i < epollRet; ++i)
				{
					printEvent(toHandleEvents[i].data.fd, toHandleEvents[i].events);
					if (toHandleEvents[i].data.fd == serverFd)	// ������serverFd
					{
						if (toHandleEvents[i].events & EPOLLIN)
						{
							if (!acceptNewConnection())
							{
								recordError(__FILE__, __LINE__);
								Log(logger, Logger::LOG_ERROR, errorString);
							}
						}
					}
					else if (toHandleEvents[i].data.fd == notifyFd)	// ������֪ͨfd
					{
						if (toHandleEvents[i].events & EPOLLIN)
						{

						}
					}
					else
					{
						int fd = toHandleEvents[i].data.fd;
						if (connections.find(fd) == connections.end())
						{
							Log(logger, Logger::LOG_ERROR, "fd not seen before");
							continue;
						}
						if (toHandleEvents[i].events & EPOLLIN || toHandleEvents[i].events & EPOLLHUP)
						{
							// test
							char *buffer = nullptr;
							buffer = new char[80 * 1024];
							ssize_t number = read(fd, buffer, BUFFER_SIZE);
							if (number < 0)
							{
								if (errno == EAGAIN)
								{
									
								}
								else
								{
									recordError(__FILE__, __LINE__);
									Log(logger, Logger::LOG_ERROR, errorString);
								}
								continue;
							}
							else if (number == 0) // �Զ˹ر�������
							{
								closeConnection(fd);
							}
							else
							{
								if (onReadHandler)
								{
									auto c = connections[fd];
									onReadHandler(&(connections[fd]), buffer, number);
								}
							}
							delete[] buffer;
						}
						else if (toHandleEvents[i].events & EPOLLOUT)
						{
							// ���Ȼص��û��ṩ�Ľӿڻ�ȡ���ݣ�Ȼ���ڷ���
							if (onCanWriteHandler)
							{
								std::deque<WriteMeta> toWrite = onCanWriteHandler(&connections[fd]);
								if (toWrite.size() > 0)
								{
									toWriteContents[fd].insert(toWriteContents[fd].end(), toWrite.begin(), toWrite.end());
								}
								if (!writeContent(fd))
								{
									recordError(__FILE__, __LINE__);
									Log(logger, Logger::LOG_ERROR, errorString);
								}
							}
						}
					}
				}
			}
		}
	}
	std::string getError() const
	{
		return errorString;
	}

private:
	struct sockaddr_in address;

	int serverFd;
	int epollFd;
	int notifyFd;	// �������ṩpipefd[0]��TcpServer�������������ߵ��������������epoll_wait����ʱ���ٷ��ش���
	bool readyForStart;
	std::string errorString;
	std::map<int, Connection> connections;
	std::map<int, std::deque<WriteMeta>> toWriteContents;
	std::set<int> closeWhenWriteFinish;

	OnConnectHandle_t onConnectHandler;	// TcpServer accept�����Ӻ��ص�
	OnReadHandle_t onReadHandler;		// TcpServer read�����ݻ�ص�
	OnCanWriteHandle_t onCanWriteHandler;// TcpServer �����׽��ֿ���write��ص���ȡ����
	OnChangeEpoll_t onChangeEpollHandler;

	void recordError(const char * filename, int lineNumber)
	{
		char errorStr[256];
		errorString.assign(filename);
		errorString.append(" : ");
		errorString.append(std::to_string(lineNumber));
		errorString.append(" : ");
		errorString.append(strerror_r(errno, errorStr, 256));	// �̶߳��е�errno������
	}
	bool acceptNewConnection()
	{
		int clfd;
		struct sockaddr_in clientAddr;
		socklen_t length;
		while ((clfd = accept(serverFd, (sockaddr *)&clientAddr, &length)) >= 0)
		{
			if (!onConnectHandler)
			{
				close(clfd);
				continue;
			}
			else
			{
				if (!setNoBlock(clfd))
				{
					recordError(__FILE__, __LINE__);
					Log(logger, Logger::LOG_ERROR, errorString);
				}
				connections[clfd] = { clfd, clientAddr };
				switch (onConnectHandler(&connections[clfd]))
				{
					case OnConnectOperation::CLOSE_IT:
					{
						closeConnection(clfd);
					}
					break;
					case OnConnectOperation::ADD_READ:
					{
						addToRead(clfd);
					}
					break;
					case OnConnectOperation::ADD_WRITE:
					{
						addToWrite(clfd);
					}
				}
			}
		}
		if (errno = EAGAIN)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	// closeҲ��Ҫ�����ϲ㣬������������close��tcpserverҪclose
	int closeConnection(int fd)	
	{
		// ��epoll���Ƴ���Ȼ��رգ�Ȼ��ɾ��
		if (epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL) == -1)
		{
			if (errno != ENOENT)
			{
				return -1;
			}
		}
		if (close(fd) < 0)
		{
			return -1;
		}
		connections.erase(fd);
		auto index = toWriteContents.find(fd);
		if (index != toWriteContents.end())
		{
			for (auto c : index->second)
			{
				if (c.buffer)
				{
					delete[] c.buffer;
				}
			}
			toWriteContents.erase(fd);
		}
		return 0;
	}
	int addToRead(int fd)
	{
		struct epoll_event event;
		bzero(&event, sizeof(event));
		if (epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, &event) == -1)
		{
			if (errno != ENOENT)
			{
				return -1;
			}
		}
		event.events |= EPOLLIN;
		event.data.fd = fd;
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) == -1)
		{
			return -1;
		}
		return 0;
	}
	int addToWrite(int fd)
	{
		struct epoll_event event;
		bzero(&event, sizeof(event));
		if (epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, &event) == -1)
		{
			if (errno != ENOENT)
			{
				return -1;
			}
		}
		event.events |= EPOLLOUT;
		event.data.fd = fd;
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) == -1)
		{
			return -1;
		}
		return 0;
	}
	/*
* ����SIGPIPE�ź�
* �����һ���Ѿ��رյ�socketд��������ź�
* ������һ���յ���RST��socket����д���յ�����ź�
* write�᷵��EPIPE
*/
	bool ignoreSignal()
	{
		sigset_t signals;
		if (sigemptyset(&signals) != 0)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if (sigaddset(&signals, SIGPIPE) != 0)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if (sigprocmask(SIG_BLOCK, &signals, nullptr) != 0)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		return true;
	}

	bool setNoBlock(int fd)
	{
		int val;
		if ((val = fcntl(fd, F_GETFL, 0)) == -1)
		{
			recordError(__FILE__, __LINE__);
			return false;
		}
		if ((val = fcntl(fd, F_SETFL, val | O_NONBLOCK)) == -1)
		{
			recordError(__FILE__, __LINE__);
			return false;;
		}
		return true;
	}

	void printEvent(int fd, uint32_t events)
	{
		std::cout << "fd: " << fd << std::endl;
		if (events & EPOLLIN)
			std::cout << "EPOLLIN" << std::endl;
		if (events & EPOLLPRI)
			std::cout << "EPOLLPRI" << std::endl;
		if (events & EPOLLOUT)
			std::cout << "EPOLLOUT" << std::endl;
		if (events & EPOLLRDNORM)
			std::cout << "EPOLLRDNORM" << std::endl;
		if (events & EPOLLRDBAND)
			std::cout << "EPOLLRDBAND" << std::endl;
		if (events & EPOLLWRNORM)
			std::cout << "EPOLLWRNORM" << std::endl;
		if (events & EPOLLWRBAND)
			std::cout << "EPOLLWRBAND" << std::endl;
		if (events & EPOLLMSG)
			std::cout << "EPOLLMSG" << std::endl;
		if (events & EPOLLERR)
			std::cout << "EPOLLERR" << std::endl;
		if (events & EPOLLHUP)
			std::cout << "EPOLLHUP" << std::endl;
		if (events & EPOLLRDHUP)
			std::cout << "EPOLLRDHUP" << std::endl;
		if (events & EPOLLWAKEUP)
			std::cout << "EPOLLWAKEUP" << std::endl;
		if (events & EPOLLONESHOT)
			std::cout << "EPOLLONESHOT" << std::endl;
		if (events & EPOLLET)
			std::cout << "EPOLLET" << std::endl;
	}

	bool writeContent(int fd)
	{
		if (toWriteContents.find(fd) == toWriteContents.end())
		{
			return false;
		}
		else if (toWriteContents[fd].size() == 0)
		{
			return false;
		}
		else
		{
			// ʹ��writev
			auto &toWriteDeque = toWriteContents[fd];
			struct iovec *toWriteIovec = new struct iovec[toWriteDeque.size()];
			int remainSize = BUFFER_SIZE;
			int i = 0;
			while (remainSize > 0 && i < toWriteDeque.size())
			{
				auto const &writeMeta = toWriteDeque[i];
				toWriteIovec[i].iov_base = (void *)(writeMeta.buffer + writeMeta.totalLen - writeMeta.toWriteLen);
				if (remainSize >= writeMeta.toWriteLen)
				{	
					toWriteIovec[i].iov_len = writeMeta.toWriteLen;
					remainSize -= writeMeta.toWriteLen;
				}
				else
				{
					toWriteIovec[i].iov_len = remainSize;
					remainSize = 0;
				}
				++i;
			}
			ssize_t size = writev(fd, toWriteIovec, i);
			delete[] toWriteIovec;
			if (size == -1)
			{
				if (errno == EAGAIN)
				{
					
				}
				else
				{
					return false;
				}
			}
			else // ��ʼ�޸�Meta���ݲ����Ƴ�д���Meta
			{
				int i = 0;
				auto index = toWriteDeque.begin();
				while (size > 0)
				{
					if (index->toWriteLen > 0)
					{
						if (size >= index->toWriteLen)
						{
							size -= index->toWriteLen;
							delete[] index->buffer;
							toWriteDeque.pop_front();
							index = toWriteDeque.begin();
						}
						else
						{
							index->toWriteLen -= size;
						}
					}
					else
					{
						// ��Ӧ���������
						Log(logger, Logger::LOG_ERROR, "should not run here");
						exit(-1);
					}
				}
			}
			if (toWriteDeque.size() == 0)
			{
				if (closeWhenWriteFinish.find(fd) != closeWhenWriteFinish.end())
				{
					closeConnection(fd);
				}
			}
			return true;
		}
	}

	void handleNotify()
	{

	}
};

