#ifdef ZK_MAIN


#include "HttpServer.h"
std::string test(HttpInfo);
std::string retIP(HttpInfo);

int main()
{
	HttpServer httpServer("0.0.0.0", 80);

/*	httpServer.setAddress("0.0.0.0");
	httpServer.setPort(80)*/;

	httpServer.addUrlHandler("/", test);
	httpServer.addUrlHandler("/IP/", retIP);
	httpServer.addUrlRetStaticStr("/hello/", "hello world");
	httpServer.addUrlAutoDirSearch("/source/", "/home/zhangke/");
	httpServer.addUrlSingleFile("/about/", "about.txt");

	httpServer.set404Page("404 Not found");
	httpServer.start();
}
std::string test(HttpInfo info)
{
	return info.url;
}

std::string retIP(HttpInfo info)
{
	return inet_ntoa(info.connection.address.sin_addr);
}
#endif // ZK_MAIN