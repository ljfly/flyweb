
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>
#include <utility>
#include <sstream>
#include <ctype.h>
#include <iostream>

using namespace std;

typedef map<string, string> 	fly_header;

//#define string::npos			REQUEST_END
#define make_fly_header(key, value)  make_pair((key), (value))	

//保存从http request解析下来的值
typedef struct Http_header_t
{
	string 		method;
	string 		url;
	string		version;

	fly_header header;

	string 	body;
}http_header_t;

/*
 *函数作用：打印fly_http_header_t里的header
 *函数参数：fly_header 的const 引用
 *函数返回值: 无
 */
 void print_http_header_header(const fly_header& head);
/*
 *函数作用：打印fly_http_header_t
 *函数参数：fly_http_header_t指针
 *函数返回值: 无
 */
 void print_http_header(http_header_t *phttphdr);

/*
 *函数作用：分配内存给fly_http_header_t
 *函数参数：无
 *函数返回值: NULL表示分配失败，其他值表示成功
 */
http_header_t *alloc_http_header();
/*
 *函数作用：回收分配给fly_http_header_t的内存
 *函数参数：fly_http_header_t指针
 *函数返回值: 无
 */
void free_http_header(http_header_t *phttphdr);
/*
 *函数作用：解析http_request
 *函数参数：http_request为待解析的值，phttphdr保存了解析下来的值
 *函数返回值: true表示解析成功，false表示解析失败
 */
bool parse_http_request(const string& http_request, http_header_t *phttphdr);
/*
 *函数作用：根据key的值在phttphdr所指向的fly_http_header_t中查找相对应的值
 *函数参数：key为关键字，header
 *函数返回值: -返回空值表示查找失败，否则返回相应的值
 */
string get_value_from_http_header(const string& key, const fly_header& header);