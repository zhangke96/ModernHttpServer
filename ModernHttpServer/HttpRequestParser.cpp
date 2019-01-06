#include "HttpRequestParser.h"

HttpRequestParser::HttpRequestParser() : finished(false), fvpairs()
{
	num = 0;
	parser = (http_parser *)malloc(sizeof(http_parser));
	http_parser_init(parser, HTTP_REQUEST);
	parser->data = this;  // 用来标示对象
						  // 设置http_parser的回调函数
	settings.on_message_begin = call_message_begin_cb;
	settings.on_header_field = call_header_field_cb;
	settings.on_header_value = call_header_value_cb;
	settings.on_url = call_request_url_cb;
	settings.on_status = call_response_status_cb;
	settings.on_body = call_body_cb;
	settings.on_headers_complete = call_headers_complete_cb;
	settings.on_message_complete = call_message_complete_cb;
	settings.on_chunk_header = call_chunk_header_cb;
	settings.on_chunk_complete = call_chunk_complete_cb;
}
ssize_t HttpRequestParser::parser_execute(const char *buf, size_t recved)     /* recved is the length of the content in buf */
{
	++num;
	size_t nparsed = http_parser_execute(parser, &settings, buf, recved);
	if (HTTP_PARSER_ERRNO(parser) != HPE_OK)
	{
		return -1;
	}
	if (num >= 5)
		finished = true;
	return nparsed;
}