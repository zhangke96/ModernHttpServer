#pragma once

#include <string>

typedef std::string(*urlHandler_t)(void *); // todo 补充函数参数

// HttpServer只考虑HTTP协议的解析，请求和响应的处理，TCP的读写通过TcpServer去处理
// 现在需要思考的是这两层怎么通信比较好
// TcpServer应该运行在一个单独的线程，两层之间可以通过消息队列来通信
// 以一个新请求举例子，TcpServer收到客户端连接，读取客户端的输入，如果长时间没有输入输出，就要关闭连接（可以设置超时时间）
// TcpServer收到输入就发给HttpServer，HttpServer建立一个新连接对象，开始解析HTTP,如果发现HTTP报文有问题，可以发消息给TcpServer去关闭这个连接
// 在收到了完整的HTTP请求，查找路由然后处理，生成HTTP响应结果，发给TcpServer去发送。TcpServer需要保证发送成功，如果发送失败，应该要告诉HttpServer

// 考虑HttpServer和TcpServer之间加一个broker，TcpServer只要专注于读写
class HttpServer
{
public:
	HttpServer();
	~HttpServer();
	bool setAddress(const std::string &addr) { return true; }
	bool setPort(int port) { return true; }

	bool addUrlHandler(const std::string &url, urlHandler_t handler) { return true; }
	bool addUrlRetStaticStr(const std::string &url, const std::string &staticResult) { return true; }
	bool addUrlAutoDirSearch(const std::string &url, const std::string &dir) { return true; }
	bool addUrlSingleFile(const std::string &url, const std::string &fileName) { return true; }

	bool set404Page(const std::string &page) { return true; }
	bool start() { return true; }
};

