#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "abstract_http_service.h"
#include "common/blocking_queue.h"
#include "common/string_utility.h"
#include "common/time_utility.h"
#include "log/log.h"
#include "transport/session_cache.h"

using namespace std;

//处理模块
class HttpService : public AbstractHttpSevice {
 public:
    HttpService();
    virtual ~HttpService();

    static HttpService *Instance();

    /// @brief 回调的业务处理函数，此函数必须为异步处理，处理出错时，由Response来处理
    /// @param handler-http请求的句柄,request-请求消息体
    /// @return void
    virtual void DoService(void *handler, HttpRequestPtr request) override;

    /// @brief 回http响应
    /// @param context_id-上下文id，response-响应内容
    /// @return void
    void Response(const std::string &context_id, HttpResponsePtr &response);

    /// @brief 获取http请求
    /// @param context_id-上下文id，request-请求体，timeout_ms-超时毫秒数
    /// @return bool-是否有新的http请求
    bool TimedWait(std::string &context_id, HttpRequestPtr &request, int timecout_ms);

 private:
    std::string GenContextId();
};
