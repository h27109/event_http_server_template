#pragma once

#include <memory>
#include <string>
#include <vector>

#include "abstract_http_service.h"
#include "boost/thread.hpp"
#include "common/async_http_client.h"
#include "http_service.h"
#include "log/log.h"

AsyncHttpClient* client;

using namespace std;

class RequestHandler {
 public:
    void ThreadFunc() {
        while (1) {
            std::string context_id;
            HttpRequestPtr http_request = make_shared<HttpRequest>();
            if (HttpService::Instance()->TimedWait(context_id, http_request, 2000)) {
                string forward_url = "http://172.20.6.21:1637/request_trace_id_validate";
                string forward_request = http_request->body;
                string log_id = "log_id";
                string session_id = context_id;
                vector<string> headers;
                for (auto it : http_request->head) {
                    string field = it.first + ": " + it.second;
                    headers.push_back(field);
                }
                // 这里处理请求业务
                PLOG_INFO("post to remote, context_id=%s, url=%s, body=%s", context_id.c_str(), forward_url.c_str(),
                          forward_request.c_str());
                client->Post(forward_url, forward_request, session_id, log_id, headers);
            }
        }
    }
};

class ResponseHandler {
 public:
    void ThreadFunc() {
        while (1) {
            string session_id;
            AsyncResponseMsg response;
            if (client->TimedWaitResponse(session_id, response, 2000)) {
                string context_id = session_id;
                HttpResponsePtr new_response = make_shared<HttpResponse>();
                new_response->http_code = response.http_code;
                if (response.curl_code != 0) {
                    PLOG_INFO("curl failed, code=%d", response.curl_code);
                    new_response->http_code = 500;
                }
                new_response->body = response.rsp_body;
                // 这里处理响应业务
                PLOG_INFO("recv response, context_id=%s, code=%d, body=%s", context_id.c_str(), new_response->http_code,
                          response.rsp_body.c_str());

                HttpService::Instance()->Response(context_id, new_response);
                PLOG_INFO("send response, context_id=%s", context_id.c_str());

                static int recv_count = 0;
                recv_count++;
                if (recv_count % 100 == 0) {
                    std::cout << "get response cout=" << recv_count << std::endl;
                }
            }
        }
    }
};

class RequestLoopHandler {
 public:
    void ThreadFunc() {
        while (1) {
            std::string context_id;
            HttpRequestPtr http_request = make_shared<HttpRequest>();
            if (HttpService::Instance()->TimedWait(context_id, http_request, 2000)) {
                usleep(10000);
            }

            HttpResponsePtr new_response = make_shared<HttpResponse>();
            new_response->http_code = 200;
            new_response->body = http_request->body;
            // 这里处理响应业务
            HttpService::Instance()->Response(context_id, new_response);
            PLOG_INFO("send response, context_id=%s", context_id.c_str());
        }
    }
};

class BusiThread {
 public:
    void Start() { SelfRun(); }

 protected:
    void RunWithCurl() {
        client = new AsyncHttpClient();
        boost::thread_group group;
        for (size_t i = 0; i < 4; i++) {
            group.create_thread(bind(&RequestHandler::ThreadFunc, new RequestHandler));
        }
        for (size_t i = 0; i < 1; i++) {
            group.create_thread(bind(&ResponseHandler::ThreadFunc, new ResponseHandler));
        }
        group.join_all();
    }

    void SelfRun() {
        boost::thread_group group;
        for (size_t i = 0; i < 8; i++) {
            group.create_thread(bind(&RequestLoopHandler::ThreadFunc, new RequestLoopHandler));
        }
        group.join_all();
    }
};
