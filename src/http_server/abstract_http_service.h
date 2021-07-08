
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
    std::string method;
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
    // 业务处理函数，只能异步处理，不能同步回响应
    virtual void DoService(void * handler, HttpRequestPtr request) = 0;

    void SetReplyFunc(ReplyFunc func) { reply_func = func; }

 protected:
    ReplyFunc reply_func;
};