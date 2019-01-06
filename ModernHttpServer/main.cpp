#ifdef ZK_MAIN


#include "HttpServer.h"
std::string test(HttpInfo);

int main()
{
	HttpServer httpServer("0.0.0.0", 80);

/*	httpServer.setAddress("0.0.0.0");
	httpServer.setPort(80)*/;

	httpServer.addUrlHandler("/", test);
	httpServer.addUrlRetStaticStr("/hello/", "hello world");
	httpServer.addUrlAutoDirSearch("/source/", "/var/www/html/");
	httpServer.addUrlSingleFile("/about/", "about.txt");

	httpServer.set404Page("404 Not found");
	httpServer.start();
}
std::string test(HttpInfo info)
{
	return info.url;
}
#endif // ZK_MAIN