#include "http_server.h"

#include <mutex>

#include "log/log.h"

using std::string;

AbstractHttpSevice *HttpServer::service_;

static std::map<void *, HttpResponsePtr> ResponseList;
static std::mutex ResponseMutex;

bool HttpServer::stop_flag_ = false;

//----------外部接口-------------------
HttpServer::HttpServer() {}
HttpServer::~HttpServer() {}

HttpServer *HttpServer::Instance() {
    static HttpServer instance_;
    return &instance_;
}

int HttpServer::Start(int port, AbstractHttpSevice *service) {
    port_ = port;
    service_ = service;
    if (port_ <= 0) {
        PLOG_ERROR("http port is invalid, start server failed, port =%d", port_);
        return -1;
    }
    return RunEvent();
}

void HttpServer::Stop(int wait_sec) {
    PLOG_INFO("server start to stop");
    stop_flag_ = true;
    if (wait_sec > 0) {
        sleep(wait_sec);
    }

    event_loopbreak();
    evhttp_free(httpd_);
    event_config_free(cfg_);

    PLOG_INFO("server stoped");
}

// 发送响应
void HttpServer::Reply(void *handler, HttpResponsePtr response) {
    // 保存response到待发送的map
    auto val = std::pair<void *, HttpResponsePtr>(handler, response);

    {
        std::lock_guard<std::mutex> locker(ResponseMutex);
        ResponseList.insert(val);
        PLOG_INFO("response list count=%d", ResponseList.size());
    }

    // 通知event主线程
    write(reply_fds[1], &handler, sizeof(void *));
}

//-------------内部函数-------------
// 获取可以返回的连接
bool HttpServer::FetchResponse(int reply_fd, void *&handler, HttpResponsePtr &response) {
    // 接收待回应的handler
    int read_len = read(reply_fd, &handler, sizeof(void *));
    if (read_len != sizeof(void *)) {
        PLOG_ERROR("read from pipe line is wrong, read length=%d", read_len);
        return false;
    }

    // 取得响应的内容
    {
        std::lock_guard<std::mutex> locker(ResponseMutex);
        auto iter = ResponseList.find(handler);
        if (iter == ResponseList.end()) {
            PLOG_ERROR("can not find response, handler=0X%X", handler);
            return false;
        }
        response = iter->second;
        ResponseList.erase(iter);
    }
    return true;
}

void HttpServer::PackRespHead(struct evhttp_request *req, HttpResponsePtr response) {
    for (auto head_pair : response->head) {
        evhttp_add_header(req->output_headers, head_pair.first.c_str(), head_pair.second.c_str());
    }

    // 如果在关闭server过程中，则直接关闭连接
    if (stop_flag_) {
        evhttp_add_header(req->output_headers, "Connection", "close");
        return;
    }

    //一个连接最多过100个请求
    const int max_request_per_conn = 100;
    struct evhttp_connection *evcon = req->evcon;
    static map<evhttp_connection *, int> req_in_connection;

    // connection头域为空或者为keep alive，需要保持长连接
    // 否则libevent会自动通知关闭连接，此时需要删除掉连接信息，否则会有内存泄漏
    string keep_alive = evhttp_find_header(req->input_headers, "Connection");
    if (!keep_alive.empty() && keep_alive != "keep-alive") {
        req_in_connection.erase(evcon);
        return;
    }

    auto it = req_in_connection.find(evcon);
    if (it == req_in_connection.end()) {
        auto val = std::pair<evhttp_connection *, int>(evcon, 1);
        req_in_connection.insert(val);
    } else {
        ++it->second;
        if (it->second >= max_request_per_conn) {
            evhttp_add_header(req->output_headers, "Connection", "close");
            req_in_connection.erase(it);
        }
    }
}

// 发送响应到网络
void HttpServer::SendTo(int reply_fd, short what, void *arg) {
    void *handler;
    HttpResponsePtr response;
    struct evhttp_request *req;

    if (!FetchResponse(reply_fd, handler, response)) {
        return;
    }

    req = (struct evhttp_request *)handler;

    PackRespHead(req, response);

    //返回body
    struct evbuffer *buf = evbuffer_new();
    evbuffer_add(buf, response->body.c_str(), response->body.size());
    evhttp_send_reply(req, response->http_code, NULL, buf);
    evbuffer_free(buf);
}

// 启动event
int HttpServer::RunEvent() {
    string httpd_option_listen = "0.0.0.0";  // 监听所有ip
    struct evhttp_bound_socket *handle = NULL;
    int ret = 0;

    // event base
    cfg_ = event_config_new();
    base_ = event_base_new_with_config(cfg_);
    if (!base_) {
        PLOG_ERROR("create event base failed");
        return -1;
    }
    /* Create a new evhttp object to handle requests. */
    httpd_ = evhttp_new(base_);
    if (!httpd_) {
        PLOG_ERROR("create evhttp failed");
        return -1;
    }

    // 回响应的pipe
    if (pipe(reply_fds) < 0) {
        PLOG_ERROR("create reply pipe failed");
        return -1;
    }
    struct event fd_event;
    event_assign(&fd_event, base_, reply_fds[0], EV_PERSIST | EV_READ, HttpServer::SendTo, NULL);
    event_add(&fd_event, NULL);

    //日志回调函数
    event_set_log_callback(LogHandler);

    // 通用http request回函数
    evhttp_set_gencb(httpd_, HttpRequestHandler, NULL);

    handle = evhttp_bind_socket_with_handle(httpd_, httpd_option_listen.c_str(), port_);
    if (!handle) {
        PLOG_ERROR("libevent bind port failed");
        return -1;
    }

    ret = event_base_dispatch(base_);

    PLOG_ERROR("event exit,code=%d", ret);

    return 0;
}

// libevent主线程回调，单线程
void HttpServer::HttpRequestHandler(struct evhttp_request *req, void *arg) {
    const char *uri;
    char *decoded_uri;

    // http 会话释放时调用，供调试时用
    // evhttp_request_set_on_complete_cb(req, RequestCompleted, NULL);

    static int64_t recv_count = 0;
    PLOG_INFO("recv http request, remote host=%s:%d, count=%ld", req->remote_host, req->remote_port, ++recv_count);

    HttpRequestPtr request = make_shared<HttpRequest>();

    // decoded uri
    uri = evhttp_request_uri(req);
    decoded_uri = evhttp_decode_uri(uri);
    request->url = string(decoded_uri);
    free(decoded_uri);

    // decode head
    struct evkeyval *header;
    TAILQ_FOREACH(header, req->input_headers, next) {
        // parse head
        request->head.push_back(pair<string, string>(string(header->key), string(header->value)));
    }

    // decode body
    char *post_data = (char *)EVBUFFER_DATA(req->input_buffer);
    int data_length = EVBUFFER_LENGTH(req->input_buffer);
    request->body = string(post_data, data_length);

    if (service_) {
        service_->DoService((void *)req, request);
    }
}

/*
#define EVENT_LOG_DEBUG 0--->debug
#define EVENT_LOG_MSG   1--->info
#define EVENT_LOG_WARN  2--->warning
#define EVENT_LOG_ERR   3--->error
*/
void HttpServer::LogHandler(int level, const char *msg) {
    switch (level) {
        case 0:
            PLOG_DEBUG("log from event=%s", msg);
            break;
        case 1:
            PLOG_INFO("log from event=%s", msg);
            break;
        case 2:
            PLOG_WARN("log from event=%s", msg);
            break;
        default:
            PLOG_ERROR("log from event=%s", msg);
            break;
    }
}

void HttpServer::RequestCompleted(struct evhttp_request *req, void *arg) { PLOG_INFO("request finished, req=%x", req); }
