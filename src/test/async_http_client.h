#ifndef _LSPF_ASYNC_HTTP_CLIENT_H_
#define _LSPF_ASYNC_HTTP_CLIENT_H_

#include <curl/curl.h>

#include <boost/shared_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "common/blocking_queue.h"
#include "common/time_utility.h"
#include "json/json.h"
#include "transport/session_cache.h"

using std::string;
using std::vector;

struct AsyncResponseMsg {
   string session_id;  // 会话ID
   string rsp_body;  // 应答报文体
   int http_code;
   int curl_code;
};

// http缓存的内容
class HttpSession {
 public:
    HttpSession(const string& url, const string& request, const string& session_id,
                const string& log_id, bool need_rsp = true)
        : url(url), request(request), id(session_id), log_id(log_id), m_head_slist(NULL) {}

    virtual ~HttpSession() {}

    string url;
    string id;
    string request;
    string response;
    string log_id;

    struct curl_slist* m_head_slist;

    long http_code;
    CURLcode curl_code;

    TimerClock timer_clock;

    AsyncResponseMsg PackageResponse();

    void PackHttpHead(const vector<string>& headers);
};
typedef boost::shared_ptr<HttpSession> HttpSessionPtr;

/*
 * 异步Http客户端，利用curl multi API实现
 */
class AsyncHttpClient {
 private:
    CURLM* m_multi_handle;

    boost::shared_ptr<BlockingQueue<HttpSession*> > m_request_queue;
    boost::shared_ptr<BlockingQueue<HttpSessionPtr> > m_session_queue;

    string m_ca_path;

    int m_dns_timeout;
    int m_connect_timeout;
    int m_process_timeout;
    int m_wait_time;

 private:
    void InitEasyHandle(CURL* easy_handle, HttpSession* psession);

    /// @brief 处理请求的循环函数
    /// @return void
    void PerformTask();

    /// @brief 获取需要处理的请求
    /// @return 是否有新增的请求
    bool AddHandle();

    /// @brief 处理请求
    /// @return 剩余待处理的请求
    int PerformHandle();

    /// @brief 读取并保存请求应答
    /// @return void
    void ReadHandle();

    static size_t WriteCallback(void* ptr, size_t size, size_t nmemb, void* data);

 public:
    AsyncHttpClient();
    virtual ~AsyncHttpClient();

    /// @brief 设置CA证书参数
    /// @param path - CA证书参数
    /// @return void
    void SetCAPath(const string& path) { m_ca_path = path; }

    /// @brief 设置超时相关参数
    /// @param dns_timeout - 设置DNS缓存的生命周期
    /// @param conn_timeout - 连接超时
    /// @param proc_timeout - 设置请求允许花费的最大时间
    /// @param wait_time - 设置请求允许花费的最大时间
    /// @return void
    void SetTimeout(int32_t dns_timeout, int32_t conn_timeout, int32_t proc_timeout, int32_t wait_time) {
        m_dns_timeout = dns_timeout;
        m_connect_timeout = conn_timeout;
        m_process_timeout = proc_timeout;
        m_wait_time = wait_time;
    }

    /// @brief 异步http post操作
    /// @param request - 请求体
    /// @param session_id - 自定义会话标志
    /// @return true 成功；false 失败
    bool Post(const string& url, const string& request, const string& session_id,
              const string& log_id, const vector<string>& headers);

    /// @brief 异步http post通知 无应答
    /// @param request - 请求体
    /// @return true 成功；false 失败
    bool Post(const string& url, const string& request, const string& log_id, const vector<string>& headers);

    /// @brief 阻塞获取异步http会话信息
    /// @param session
    /// @param response - 会话结构体
    /// @return void
    void WaitResponse(string& session_id, AsyncResponseMsg& response);

    /// @brief 设置超时阻塞获取异步http会话信息
    /// @param session
    /// @param response - 会话结构体
    /// @param timeout_in_ms - 超时时间
    /// @return 是否有获取到会话
    bool TimedWaitResponse(string& session_id, AsyncResponseMsg& response, int timeout_in_ms);
};

#endif  // _LSPF_ASYNC_HTTP_CLIENT_H_