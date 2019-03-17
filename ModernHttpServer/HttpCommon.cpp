#include "HttpCommon.h"
const std::map<int, std::string> responseCodeStrMap
{
	{200, "OK"},
	{302, "FOUND"},
	{304, "NOT MODIFIED"},
	{400, "BAD REQUEST"},
	{403, "FORBIDDEN"},
	{404, "NOT FOUND"},
	{500, "INTERNAL SERVER ERROR"}
};