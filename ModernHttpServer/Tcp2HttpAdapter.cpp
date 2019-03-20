#include "Tcp2HttpAdapter.h"

OnConnectOperation Tcp2HttpAdapter::onConnectHandler(const TcpConnection *connection, void *arg)
{
	Tcp2HttpAdapter *adapter = static_cast<Tcp2HttpAdapter *>(arg);
	HttpEvent newEvent;
	newEvent.event = HttpEventType::NewConnection;
	TcpConnection *conn = new TcpConnection(*connection);
	newEvent.data = (void *)conn;
	adapter->postEvent(newEvent);
	return OnConnectOperation::ADD_READ;
}

void Tcp2HttpAdapter::onReadHandler(const TcpConnection *connection, const char *data, size_t size, void *arg)
{
	Tcp2HttpAdapter *adapter = static_cast<Tcp2HttpAdapter *>(arg);
	HttpEvent newEvent;
	newEvent.event = HttpEventType::NewData;
	char *temp_data = new char[size];
	memcpy(temp_data, data, size);
	TcpData *newData = new TcpData(*connection, size, temp_data);
	newEvent.data = newData;
	adapter->postEvent(newEvent);
}

void Tcp2HttpAdapter::onPeerShutdownHandler(const TcpConnection *connection, void *arg)
{
	Tcp2HttpAdapter *adapter = static_cast<Tcp2HttpAdapter *>(arg);
	// 现在直接静默回复关闭，并且告诉上层
	HttpEvent newEvent;
	newEvent.event = HttpEventType::PeerShutdown;
	newEvent.data = new TcpConnection(*connection);
	adapter->postEvent(newEvent);
	adapter->addConnectionShutdownEvent(connection);
}

void Tcp2HttpAdapter::onCanWriteHandler(const TcpConnection *connection, void *arg)
{
	Tcp2HttpAdapter *adapter = static_cast<Tcp2HttpAdapter *>(arg);
	HttpEvent newEvent;
	newEvent.event = HttpEventType::CanWrite;
	newEvent.data = new TcpConnection(*connection);
	adapter->postEvent(newEvent);
}