#include "http_server.h"

#include <mutex>

#include "log/log.h"

using std::string;

AbstractHttpSevice *HttpServer::service_;

// int HttpServer::reply_fds[2];

static std::map<void *, HttpResponsePtr> ResponseList;
static std::mutex ResponseMutex;

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

void HttpServer::Stop() {
    PLOG_ERROR("server stop");
    event_loopbreak();
    evhttp_free(httpd_);
    event_config_free(cfg_);
}

void HttpServer::Reply(void *handler, HttpResponsePtr response) {
    PLOG_INFO("reply to event, handler=0X%X", handler);

    auto val = std::pair<void *, HttpResponsePtr>(handler, response);

    {
        std::lock_guard<std::mutex> locker(ResponseMutex);
        ResponseList.insert(val);
    }

    write(reply_fds[1], &handler, sizeof(void *));
}

//-------------内部函数-------------
void HttpServer::SendTo(int fd, short what, void *arg) {
    void *handler;
    struct evhttp_request *req;
    HttpResponsePtr response;

    int read_len = read(fd, &handler, sizeof(void *));
    if (read_len != sizeof(void *)) {
        PLOG_ERROR("read from pipe line is wrong, read length=%d", read_len);
        return;
    }

    {
        std::lock_guard<std::mutex> locker(ResponseMutex);
        auto resp = ResponseList.find(handler);
        if (resp == ResponseList.end()) {
            PLOG_ERROR("can not find response, handler=0X%X", handler);
            return;
        }
        response = resp->second;
        ResponseList.erase(resp);
    }

    req = (struct evhttp_request *)handler;

    PLOG_INFO("reply to relay, handler=%x", req);

    // HTTP header
    // evhttp_add_header(req->output_headers, "Connection", "close");  //http长连接由libevent自行管理

    //输出的内容
    struct evbuffer *buf = evbuffer_new();
    evbuffer_add_printf(buf, "%s", response->body.c_str());
    evhttp_send_reply(req, response->http_code, NULL, buf);
    evbuffer_free(buf);
}

int HttpServer::RunEvent() {
    string httpd_option_listen = "0.0.0.0";  // 监听所有ip
    struct evhttp_bound_socket *handle = NULL;
    int ret = 0;

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

    if (0 > pipe(reply_fds)) {
        PLOG_ERROR("create reply pipe failed");
        return -1;
    }
    struct event fd_event;
    event_assign(&fd_event, base_, reply_fds[0], EV_PERSIST | EV_READ, HttpServer::SendTo, NULL);
    event_add(&fd_event, NULL);

    //日志回调函数
    event_set_log_callback(LogHandler);

    // 通用http request回函数
    evhttp_set_gencb(httpd_, GenCallBack, NULL);

    handle = evhttp_bind_socket_with_handle(httpd_, httpd_option_listen.c_str(), port_);
    if (!handle) {
        PLOG_ERROR("libevent bind port failed");
        return -1;
    }

    ret = event_base_dispatch(base_);

    PLOG_ERROR("event exit,code=%d", ret);
}

void HttpServer::GenCallBack(struct evhttp_request *req, void *arg) {
    const char *uri = evhttp_request_uri(req);

    // evhttp_request_set_on_complete_cb(req, RequestCompleted, NULL);

    static int64_t recv_count = 0;

    PLOG_INFO("recv http request, count=%ld", ++recv_count);

    HttpRequestPtr request = make_shared<HttpRequest>();

    // decoded uri
    char *decoded_uri;
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

// http 会话释放时调用，供调试时用
void HttpServer::RequestCompleted(struct evhttp_request *req, void *arg) { PLOG_INFO("request finished, req=%x", req); }
