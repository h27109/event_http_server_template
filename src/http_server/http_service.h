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

#include "abstract_http_service.h"
#include "log/log.h"

using namespace std;

//处理模块
class HttpService : public AbstractHttpSevice {
 public:
    void DoService(void * handler, HttpRequestPtr request) {
        // put into session and queue
        HttpContextPtr context = make_shared<HttpContext>();
        context->id = GenContextId();
        context->request = request;
        context->handler = handler;

        m_queue.PushFront(context);
        m_session.PushSession(context->id, context);
    }

    void Response(const std::string &conext_id, HttpResponsePtr &response);

    bool TimedWait(std::string &conext_id, HttpRequestPtr &request, int timecout_ms) {
        HttpContextPtr context;
        if (!m_queue.TimedPopBack(&context, timecout_ms) || !context) {
            return false;
        }
        conext_id = context->id;
        request = context->request;
        return true;
    }

    static HttpService *Instance() {
        static HttpService instance_;
        return &instance_;
    }

 protected:
    BlockingQueue<HttpContextPtr> m_queue;

    SessionCache<HttpContextPtr, std::string> m_session;

 private:
    std::string GenContextId() {
        static atomic_llong seq_no(0);
        string id = TimeUtility::GetStringTime() + StringUtility::IntToString(++seq_no);
        return id;
    }
};
