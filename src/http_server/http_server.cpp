#include "http_server.h"

#include "log/log.h"

using std::string;

AbstractHttpSevice *HttpServer::service_;

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
        PLOG_ERROR("http port is invalid, start server failed");
        return -1;
    }
    return RunEvent();
}

void HttpServer::Stop() {
    event_loopbreak();
    evhttp_free(httpd_);
}

void HttpServer::Reply(void *handler, HttpResponsePtr response) {
    struct evhttp_request *req = (struct evhttp_request *)handler;
    int code = response->http_code;

    for (auto it : response->head) {
        evhttp_add_header(req->output_headers, it.first.c_str(), it.second.c_str());
    }
    struct evhttp_connection *evcon = req->evcon;

    // HTTP header
    evhttp_add_header(req->output_headers, "Connection", "close");  //??http长连接怎么处理？

    //输出的内容
    struct evbuffer *buf;
    buf = evbuffer_new();
    evbuffer_add_printf(buf, "%s", response->body.c_str());
    evhttp_send_reply(req, response->http_code, NULL, buf);
    evbuffer_free(buf);
}

int HttpServer::RunEvent() {
    //默认参数
    string httpd_option_listen = "0.0.0.0";
    int httpd_option_timeout = 60;  // in seconds

    //初始化event API
    struct event_base *pbase = event_init();

    //创建一个http server
    httpd_ = evhttp_start(httpd_option_listen.c_str(), port_);
    if (httpd_ == NULL) {
        PLOG_ERROR("event init failed");
        return -1;
    }
    evhttp_set_timeout(httpd_, httpd_option_timeout);

    //指定generic callback
    evhttp_set_gencb(httpd_, GenCallBack, NULL);

    event_set_log_callback(LogHandler);

    //循环处理events，不阻塞调用线程
    thread t(event_dispatch);
    t.detach();

    return 0;
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

void HttpServer::GenCallBack(struct evhttp_request *req, void *arg) {
    const char *uri;
    uri = evhttp_request_uri(req);

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
