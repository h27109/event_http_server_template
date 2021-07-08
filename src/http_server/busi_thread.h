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
                vector<string> headers;
                for (auto it : http_request->head) {
                    string field = it.first + ":" + it.second;
                    headers.push_back(field);
                    PLOG_INFO("send head=%s", field.c_str());
                }
                // 这里处理请求业务
                PLOG_INFO("post to remote, context_id=%s, url=%s, body=%s", context_id.c_str(), forward_url.c_str(),
                          forward_request.c_str());
                client->Post(forward_url, forward_request, context_id, log_id, headers);
            }
        }
    }
};

class ResponseHandler {
 public:
    void ThreadFunc() {
        while (1) {
            string context_id;
            AsyncResponseMsg response;
            if (client->TimedWaitResponse(context_id, response, 2000)) {
                HttpResponsePtr new_response = make_shared<HttpResponse>();
                new_response->http_code = response.http_code;
                if (response.curl_code != 0) {
                    new_response->http_code = 500;
                }
                new_response->body = response.rsp_body;
                // 这里处理响应业务
                PLOG_INFO("recv response, context_id=%s, code=%d, body=%s", context_id.c_str(), new_response->http_code,
                          response.rsp_body.c_str());

                HttpService::Instance()->Response(context_id, new_response);
                PLOG_INFO("send response, context_id=%s", context_id.c_str());
            }
        }
    }
};

class BusiThread {
 public:
    void Start() {
        client = new AsyncHttpClient();
        boost::thread_group group;
        for (size_t i = 0; i < 4; i++) {
            group.create_thread(bind(&RequestHandler::ThreadFunc, new RequestHandler));
        }
        for (size_t i = 0; i < 4; i++) {
            group.create_thread(bind(&ResponseHandler::ThreadFunc, new ResponseHandler));
        }
        group.join_all();
    }
};
