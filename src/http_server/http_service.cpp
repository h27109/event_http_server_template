#include "http_service.h"

HttpService::HttpService() {}
HttpService::~HttpService() {}

bool HttpService::TimedWait(std::string &conext_id, HttpRequestPtr &request, int timeout_ms) {
    HttpContextPtr context;
    if (!m_queue.TimedPopBack(&context, timeout_ms) || !context) {
        return false;
    }
    conext_id = context->id;
    request = context->request;
    return true;
}

HttpService *HttpService::Instance() {
    static HttpService instance_;
    return &instance_;
}

std::string HttpService::GenContextId() {
    static atomic_llong seq_no(0);
    string id = TimeUtility::GetStringTime() + StringUtility::IntToString(++seq_no);
    return id;
}

void HttpService::DoService(void *handler, HttpRequestPtr request) {
    // put into session and queue
    HttpContextPtr context = make_shared<HttpContext>();
    context->id = GenContextId();
    context->request = request;
    context->handler = handler;

    m_queue.PushFront(context);
    m_session.PushSession(context->id, context);
}

void HttpService::Response(const std::string &context_id, HttpResponsePtr &response) {
    HttpContextPtr context;
    if (!m_session.PopSession(context_id, &context)) {
        PLOG_INFO("get session failed, context_id=%s", context_id.c_str());
        return;
    }
    reply_func(context->handler, response);
}
