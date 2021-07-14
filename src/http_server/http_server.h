#pragma once

// for struct evkeyvalq
#include <event.h>
#include <sys/queue.h>
// for http
//#include <evhttp.h>
#include <event2/http.h>
#include <event2/http_compat.h>
#include <event2/http_struct.h>
#include <event2/util.h>

#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "abstract_http_service.h"
#include "log/log.h"

using std::string;


class HttpServer {
 public:
    HttpServer();
    virtual ~HttpServer();

    /// @brief 在指定端口启动服务
    /// @param port-端口号，service-业务处理类
    /// @return 0-成功，其他失败
    int Start(int port, AbstractHttpSevice *service);

    /// @brief 停止http服务
    /// @param void
    /// @return void
    void Stop();

    /// @brief 发送Http response到客户端
    /// @param handler-连接句柄，response-响应消息
    /// @return void
    void Reply(void *handler, HttpResponsePtr response);

    static HttpServer *Instance();

 protected:
    /// @brief 启动libevent主loop
    /// @param void
    /// @return 0-成功，其他失败
    int RunEvent();

 private:
    static AbstractHttpSevice *service_;

    int port_ = -1;
    struct evhttp *httpd_ = nullptr;

    /// @brief libevent日志回调函数
    /// @param level 日志级别，msg消息内容
    /// @return void
    static void LogHandler(int level, const char *msg);

    /// @brief 收到完整请求后的回调函数
    /// @param req-请求句柄，arg-参数(未使用)
    /// @return void
    static void GenCallBack(struct evhttp_request *req, void *arg);

    static void RequestCompleted(struct evhttp_request *req, void * arg);
};
