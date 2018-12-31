#ifdef ZK_MAIN


#include "HttpServer.h"

int main()
{
	HttpServer httpServer;

	httpServer.setAddress("0.0.0.0");
	httpServer.setPort(80);

	httpServer.addUrlHandler("/", nullptr);
	httpServer.addUrlRetStaticStr("/hello/", "hello world");
	httpServer.addUrlAutoDirSearch("/source/", "/var/www/html/");
	httpServer.addUrlSingleFile("/about/", "about.txt");

	httpServer.set404Page("404 Not found");
	httpServer.start();
}

#endif // ZK_MAIN