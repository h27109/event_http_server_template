
#pragma once 

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "common/blocking_queue.h"
#include "common/string_utility.h"
#include "common/time_utility.h"
#include "transport/session_cache.h"

class HttpRequest {
 public:
    std::string body;
    std::vector<std::pair<std::string, std::string>> head;
    std::string url;
    //std::string method;
};

class HttpResponse {
 public:
    std::string body;
    std::vector<std::pair<std::string, std::string>> head;
    int http_code = 200;
};

typedef shared_ptr<HttpRequest> HttpRequestPtr;
typedef shared_ptr<HttpResponse> HttpResponsePtr;

class HttpContext {
 public:
    std::string id;
    HttpRequestPtr request;
    void *handler = nullptr;
};

typedef shared_ptr<HttpContext> HttpContextPtr;

typedef function<void(void *handler, HttpResponsePtr reponse)> ReplyFunc;

class AbstractHttpSevice {
 public:
    /// @brief 回调的业务处理函数，此函数必须为异步处理，处理出错时，由Response来处理
    /// @param handler-http请求的句柄,request-请求消息体
    /// @return void
    virtual void DoService(void * handler, HttpRequestPtr request) = 0;

    /// @brief 设置service往http server层回响应的函数
    /// @param func-函数指针
    /// @return void
    void SetReplyFunc(ReplyFunc func) { reply_func = func; }

 protected:
    ReplyFunc reply_func;
};