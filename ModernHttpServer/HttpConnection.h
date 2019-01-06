#pragma once
#include "HttpRequestParser.h"
#include "HttpCommon.h"
// HttpConnection负责承载HTTP的解析
// 后期可以把用户提供的处理函数绑定到这个上面

enum class HttpConnectionState
{
	Http_Connection_Initial,
	Http_Connection_Error,	// 用户的请求解析有问题
	Http_Connection_Receiving,
	Http_Connection_ReceiveAll,
	Http_Connection_Handleing,
	Http_Connection_SendingHead,
	Http_Connection_SendingBody,
	Http_Connection_SendAll
};
class HttpConnection
{
public:
	HttpConnection() : state(HttpConnectionState::Http_Connection_Initial), tempData(nullptr), length(0) {}
	HttpConnection(const HttpConnection &rhs) : state(rhs.state), tempData(nullptr), length(rhs.length)
	{
		if (rhs.tempData)
		{
			tempData = new char[length];
			memcpy(tempData, rhs.tempData, length);
		}
	}
	HttpConnection & operator=(const HttpConnection& rhs)
	{
		if (this == &rhs)
		{
			return *this;
		}
		state = rhs.state;
		char *temp = new char[rhs.length];
		if (temp)
		{
			delete[] tempData;
			tempData = temp;
			length = rhs.length;
		}
		return *this;
	}
	~HttpConnection()
	{
		if (tempData)
		{
			delete[] tempData;
		}
	}

	HttpConnectionState handleTcpData(TcpData *data)
	{
		char *temp = nullptr;
		size_t toParseLength = 0;
		if (tempData)
		{
			toParseLength = length + data->length;
			temp = new char[toParseLength];
			memcpy(temp, tempData, length);
			delete[] tempData;
			tempData = nullptr;
			length = 0;
			memcpy(temp + length, data->data, data->length);
		}
		else
		{
			temp = const_cast<char *>(data->data);
			toParseLength = data->length;
		}
		ssize_t ret = parser.parser_execute(temp, toParseLength);
		if (ret == -1)
		{
			state = HttpConnectionState::Http_Connection_Error;
			return state;
		}
		else
		{
			if (ret != toParseLength)
			{
				if (parser.isFinished())
				{
					state = HttpConnectionState::Http_Connection_Error;
					return state;
				}
				else
				{
					// 保存一下还没有解析的数据
					assert(tempData == nullptr);
					tempData = new char[toParseLength - ret];
					memcpy(tempData, temp + ret, toParseLength - ret);
					length = toParseLength - ret;
					state = HttpConnectionState::Http_Connection_Receiving;
					return state;
				}
			}
			else
			{
				if (parser.isFinished())
				{
					state = HttpConnectionState::Http_Connection_ReceiveAll;
				}
				else
				{
					state = HttpConnectionState::Http_Connection_Receiving;
				}
				return state;
			}
		}
	}
	std::string getUrl() const
	{
		return parser.getUrl();
	}
	auto getHeaders() const
	{
		return parser.fvpairs;
	}
private:
	HttpRequestParser parser;
	HttpConnectionState state;
	char *tempData;
	ssize_t length;
};

