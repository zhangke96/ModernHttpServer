#ifdef TCP_SERVER_TEST

#include "TcpServer.h"
#include "tool.h"
#include <iostream>
#include <list>
using namespace std;

OnConnectOperation connectHandler(Connection*);
void onReadHandler(Connection *, const char *, ssize_t);
std::deque<WriteMeta> onCanWriteHandler(Connection *);
std::list<std::pair<int, WriteMeta>> toWrite;
std::vector<std::pair<int, EpollChangeOperation>> onChangeEpollHandler();
std::vector < std::pair<int, EpollChangeOperation>> changes;

int main()
{
	TcpServer tcpServer;
	cout << "setAddressPort result: " << boolalpha
		<< tcpServer.setAddressPort("0.0.0.0", 80) << endl;
	cout << "getAddress: " << tcpServer.getAddress() << endl;
	cout << "getPort: " << tcpServer.getPort() << endl;
	bool startResult = false;
	tcpServer.onConnect(connectHandler);
	tcpServer.onNewData(onReadHandler);
	tcpServer.onCanSendData(onCanWriteHandler);
	tcpServer.onNeedChangeEpoll(onChangeEpollHandler);
	startResult = tcpServer.start();
	if (!startResult)
	{
		cout << "getError: " << tcpServer.getError() << endl;
	}
	else
	{
		cout << "Start ok" << endl;
	}
	tcpServer.runServer();
}

OnConnectOperation connectHandler(Connection*)
{
	return OnConnectOperation::ADD_READ;
}
void onReadHandler(Connection *connect, const char *buffer, ssize_t size)
{
	cout << std::string(buffer, size) << endl;
	std::string temp = createOk("hello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\nhello world\n");
	char *buf = new char[temp.length()];
	memcpy(buf, temp.c_str(), temp.length());
	WriteMeta toWriteMeta(buf, temp.length());
	toWrite.emplace_back(connect->fd, toWriteMeta);
	changes.emplace_back(connect->fd, EpollChangeOperation::ADD_WRITE);
	changes.emplace_back(connect->fd, EpollChangeOperation::CLOSE_IF_NO_WRITE);
}
std::deque<WriteMeta> onCanWriteHandler(Connection *connect) // 传递数据一定要new出来的
{
	std::deque<WriteMeta> result;
	for (auto index = toWrite.begin(); index != toWrite.end(); ++index)
	{
		if (index->first == connect->fd)
		{
			result.push_back(index->second);
			index = toWrite.erase(index);
		}
	}
	return result;
}
std::vector<std::pair<int, EpollChangeOperation>> onChangeEpollHandler()
{
	std::vector<std::pair<int, EpollChangeOperation>> emptyChanges;
	std::swap(emptyChanges, changes);
	return emptyChanges;
}
#endif // TCP_SERVER_TEST
