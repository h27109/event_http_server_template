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
    /// @param wait_sec-等待会话完成时间
    /// @return void
    void Stop(int wait_sec = 0);

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

    int reply_fds[2] = {0, 0};
    int port_ = -1;
    struct evhttp *httpd_ = nullptr;
    struct event_config *cfg_ = nullptr;
    struct event_base *base_ = nullptr;

    static bool stop_flag_;

    /// @brief libevent日志回调函数
    /// @param level 日志级别，msg消息内容
    /// @return void
    static void LogHandler(int level, const char *msg);

    /// @brief 收到完整请求后的回调函数
    /// @param req-请求句柄，arg-参数(未使用)
    /// @return void
    static void HttpRequestHandler(struct evhttp_request *req, void *arg);

    /// @brief http会话完成后的回调函数
    /// @param req-请求句柄，arg-参数(未使用)
    /// @return void
    static void RequestCompleted(struct evhttp_request *req, void *arg);

    /// @brief 从reply管道获取待处理的响应
    /// @param reply_fd-管道，handler-返回句柄，response-返回报文
    /// @return bool是否成功
    static bool FetchResponse(int reply_fd, void *&handler, HttpResponsePtr &response);

    /// @brief 打包响应消息的http头
    /// @param req-请求句柄，response-返回报文
    /// @return void
    static void PackRespHead(struct evhttp_request *req, HttpResponsePtr response);

    /// @brief 发送响应到网络
    /// @param reply_fd-管道，what/arg无使用
    /// @return void
    static void SendTo(int reply_fd, short what, void *arg);
};
