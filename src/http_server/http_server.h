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

    int Start(int port, AbstractHttpSevice *service);
    
    void Stop();

    void Reply(void *handler, HttpResponsePtr response);

    static HttpServer *Instance();

 protected:
    int RunEvent();

 private:
    static AbstractHttpSevice *service_;

    int port_ = -1;
    struct evhttp *httpd_ = nullptr;

    static void LogHandler(int level, const char *msg);

    static void GenCallBack(struct evhttp_request *req, void *arg);
};
