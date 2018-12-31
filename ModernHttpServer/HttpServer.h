#pragma once

#include <string>

typedef std::string(*urlHandler_t)(void *); // todo ���亯������

// HttpServerֻ����HTTPЭ��Ľ������������Ӧ�Ĵ���TCP�Ķ�дͨ��TcpServerȥ����
// ������Ҫ˼��������������ôͨ�űȽϺ�
// TcpServerӦ��������һ���������̣߳�����֮�����ͨ����Ϣ������ͨ��
// ��һ������������ӣ�TcpServer�յ��ͻ������ӣ���ȡ�ͻ��˵����룬�����ʱ��û�������������Ҫ�ر����ӣ��������ó�ʱʱ�䣩
// TcpServer�յ�����ͷ���HttpServer��HttpServer����һ�������Ӷ��󣬿�ʼ����HTTP,�������HTTP���������⣬���Է���Ϣ��TcpServerȥ�ر��������
// ���յ���������HTTP���󣬲���·��Ȼ��������HTTP��Ӧ���������TcpServerȥ���͡�TcpServer��Ҫ��֤���ͳɹ����������ʧ�ܣ�Ӧ��Ҫ����HttpServer

// ����HttpServer��TcpServer֮���һ��broker��TcpServerֻҪרע�ڶ�д
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

