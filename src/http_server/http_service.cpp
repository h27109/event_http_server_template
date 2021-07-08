#include "http_service.h"

#include "http_server.h"

void HttpService::Response(const std::string &context_id, HttpResponsePtr &response) {
    HttpContextPtr context;
    if (!m_session.PopSession(context_id, &context)) {
        PLOG_INFO("get session failed, context_id=%s", context_id.c_str());
        return;
    }
    reply_func(context->handler, response);
}
