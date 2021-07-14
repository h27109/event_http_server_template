#include "http_service.h"

HttpService::HttpService() {}
HttpService::~HttpService() {}

bool HttpService::TimedWait(std::string &conext_id, HttpRequestPtr &request, int timeout_ms) {
    HttpContextPtr context;

    bool ret = m_queue.TimedPopBack(&context, timeout_ms);
/*
    std::vector<HttpContextPtr> context_list;
    std::vector<std::string> context_id_list;

    m_session.ClearTimeout(5, context_list, context_id_list);
    HttpResponsePtr response = make_shared<HttpResponse>();
    response->http_code = 500; // 超过时间的会话向前端返回500的错误，并释放掉会话
    for (auto context : context_list) {
        reply_func(context->handler, response);
    }
*/
    if (!ret || !context) {
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
    PLOG_INFO("recv msg from client");
    HttpContextPtr context = make_shared<HttpContext>();
    context->id = GenContextId();
    context->request = request;
    context->handler = handler;

    m_queue.PushFront(context);
    if(!m_session.PushSession(context->id, context))
    {
        PLOG_ERROR("push to session failed");
    }
}

void HttpService::Response(const std::string &context_id, HttpResponsePtr &response) {
    HttpContextPtr context;
    if (!m_session.PopSession(context_id, &context)) {
        // 找不到会话说明已经超时了
        PLOG_INFO("get session failed, context_id=%s", context_id.c_str());
        return;
    }
    lspf::log::Log::SetLogId(context->request->body);
    PLOG_INFO("do response");
    reply_func(context->handler, response);
}
