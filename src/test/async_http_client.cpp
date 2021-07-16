#include "async_http_client.h"

#include <boost/atomic.hpp>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>
#include <cstring>

#include "common/mutex.h"
#include "common/string_utility.h"
#include "common/time_utility.h"
#include "log/log.h"

AsyncResponseMsg HttpSession::PackageResponse() {
    AsyncResponseMsg msg;

    msg.session_id = id;
    msg.rsp_body = response;
    msg.http_code = int(http_code);
    msg.curl_code = int(curl_code);

    return msg;
}

void HttpSession::PackHttpHead(const vector<string>& headers) {
    for (size_t i = 0; i < headers.size(); ++i) {
        m_head_slist = curl_slist_append(m_head_slist, headers[i].c_str());
    }
}

AsyncHttpClient::AsyncHttpClient()
    : m_multi_handle(curl_multi_init()),
      m_dns_timeout(120),
      m_connect_timeout(10),
      m_process_timeout(30),
      m_wait_time(5) {
    m_request_queue = boost::make_shared<BlockingQueue<HttpSession*> >();
    m_session_queue = boost::make_shared<BlockingQueue<HttpSessionPtr> >();

    boost::thread multi_perform_task(boost::bind(&AsyncHttpClient::PerformTask, this));
    multi_perform_task.detach();
}

AsyncHttpClient::~AsyncHttpClient() {
    if (m_multi_handle != NULL) {
        curl_multi_cleanup(m_multi_handle);
    }
}

bool AsyncHttpClient::AddHandle() {
    BlockingQueue<HttpSession*>::UnderlyContainerType all_request;
    bool rs = m_request_queue->TryPopAll(&all_request);

    if (all_request.size() == 0) {
        return false;
    }

    PLOG_INFO("request_size=%d", all_request.size());

    for (size_t i = 0; i < all_request.size(); ++i) {
        CURL* easy_handle = curl_easy_init();

        InitEasyHandle(easy_handle, all_request[i]);
        curl_multi_add_handle(m_multi_handle, easy_handle);
    }

    return true;
}

int AsyncHttpClient::PerformHandle() {
    TimerClock timer_clock;
    CURLMcode mc;
    int numfds, still_running;

    mc = curl_multi_perform(m_multi_handle, &still_running);

    if (mc != CURLM_OK) {
        PLOG_INFO("curl_multi_perform() error:%s", curl_multi_strerror(mc));
    }

    if (still_running) {
        curl_multi_wait(m_multi_handle, NULL, 0, m_wait_time, &numfds);
    }

    PLOG_DEBUG("perform cost time=%dms, still_running=%d",
                timer_clock.Elapsed(), still_running);

    return still_running;
}

void AsyncHttpClient::ReadHandle() {
    int msgs_left = 0;
    CURLMsg* msg = NULL;

    while (true) {
        msg = curl_multi_info_read(m_multi_handle, &msgs_left);
        if (msg == NULL) {
            break;
        }
        if (msg->msg != CURLMSG_DONE) {
            continue;
        }

        CURL* easy_handle = msg->easy_handle;
        HttpSession* psession = NULL;
        curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &psession);

        lspf::log::Log::SetLogId(psession->log_id);
        long response_code;
        curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
        psession->http_code = response_code;

        CURLcode result = msg->data.result;
        if (result != CURLE_OK) {
            PLOG_INFO("curl error:%s", curl_easy_strerror(result));
        }
        psession->curl_code = result;

        curl_off_t connect_time;
        curl_easy_getinfo(easy_handle, CURLINFO_CONNECT_TIME_T, &connect_time);

        curl_multi_remove_handle(m_multi_handle, easy_handle);
        curl_slist_free_all(psession->m_head_slist);
        curl_easy_cleanup(easy_handle);

        PLOG_INFO("async http client elapsed:%dms, connect time:%dus, url:%s, status code:%d.",
                    psession->timer_clock.Elapsed(),
                    (long)connect_time,
                    psession->url.c_str(), response_code);
        if (!psession->id.empty()) {
            m_session_queue->PushBack(boost::shared_ptr<HttpSession>(psession));
        }
    }
}

void AsyncHttpClient::PerformTask() {
    int still_running = 0;
    while (true) {
        lspf::log::Log::SetLogId("");

        if (!AddHandle() && still_running == 0) {
            struct timeval wait = {0, 10 * 1000};  // 10ms
            select(0, NULL, NULL, NULL, &wait);
            continue;
        }

        still_running = PerformHandle();

        ReadHandle();
    }
}

bool AsyncHttpClient::Post(const string& url, const string& request, const string& session_id, 
                           const string& log_id, const vector<string>& headers) {
    if (url.empty()) {
        return false;
    }

    HttpSession* psession = new HttpSession(url, request, session_id, log_id);

    psession->PackHttpHead(headers);

    if (!m_request_queue->TryPushBack(psession)) {
        PLOG_ERROR("send to request queue failed");
        if (psession->m_head_slist) {
            curl_slist_free_all(psession->m_head_slist);
        }
        delete psession;
        return false;
    }

    PLOG_INFO("Async http post, wait size=%d", m_request_queue->Size());
    return true;
}

bool AsyncHttpClient::Post(const string& url, const string& request, const string& log_id,
                           const vector<string>& headers) {
    if (url.empty()) {
        return false;
    }

    HttpSession* psession = new HttpSession(url, request, "", log_id);

    psession->PackHttpHead(headers);

    if (!m_request_queue->TryPushBack(psession)) {
        PLOG_ERROR("send to request queue failed");
        if (psession->m_head_slist) {
            curl_slist_free_all(psession->m_head_slist);
        }
        delete psession;
        return false;
    }

    PLOG_INFO("Async http post, wait size=%d", m_request_queue->Size());
    return true;
}

void AsyncHttpClient::WaitResponse(string& session_id, AsyncResponseMsg& response) {
    HttpSessionPtr psession;
    m_session_queue->PopFront(&psession);

    session_id = psession->id;
    lspf::log::Log::SetLogId(psession->log_id);

    PLOG_DEBUG("get response, session_id=%s", session_id.c_str());
    response = psession->PackageResponse();
}

bool AsyncHttpClient::TimedWaitResponse(string& session_id, AsyncResponseMsg& response, int timeout_in_ms) {
    HttpSessionPtr psession;
    if (!m_session_queue->TimedPopFront(&psession, timeout_in_ms)) {
        return false;
    }
    session_id = psession->id;
    lspf::log::Log::SetLogId(psession->log_id);

    PLOG_DEBUG("get response, session_id=%s", session_id.c_str());

    response = psession->PackageResponse();
    return true;
}

void AsyncHttpClient::InitEasyHandle(CURL* easy_handle, HttpSession* psession) {
    curl_easy_setopt(easy_handle, CURLOPT_ENCODING, "UTF-8");
    curl_easy_setopt(easy_handle, CURLOPT_URL, psession->url.c_str());
    curl_easy_setopt(easy_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(easy_handle, CURLOPT_POSTFIELDS, psession->request.c_str());
    curl_easy_setopt(easy_handle, CURLOPT_POSTFIELDSIZE, psession->request.length());

    curl_easy_setopt(easy_handle, CURLOPT_PRIVATE, psession);

    curl_easy_setopt(easy_handle, CURLOPT_CONNECTTIMEOUT, m_connect_timeout);
    curl_easy_setopt(easy_handle, CURLOPT_TIMEOUT, m_process_timeout);
    curl_easy_setopt(easy_handle, CURLOPT_DNS_CACHE_TIMEOUT, m_dns_timeout);

    // 参考：https://curl.haxx.se/mail/lib-2017-10/0142.html
    // curl_easy_setopt(easy_handle, CURLOPT_FORBID_REUSE, 1L);   // 强制关闭连接
    // curl_easy_setopt(easy_handle, CURLOPT_FRESH_CONNECT, 1L);  // 强制使用新连接

    //回调
    curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, &(psession->response));

    /**
     * 当多个线程都使用超时处理的时候，同时主线程中有sleep或是wait等操作。
     * 如果不设置这个选项，libcurl将会发信号打断这个wait从而导致程序退出。
     */
    curl_easy_setopt(easy_handle, CURLOPT_NOSIGNAL, 1);

    if (!psession->m_head_slist) {
        psession->m_head_slist = curl_slist_append(psession->m_head_slist,
            "Content-type:application/x-www-form-urlencoded;charset=UTF-8");
    }
    curl_easy_setopt(easy_handle, CURLOPT_HTTPHEADER, psession->m_head_slist);

    if (m_ca_path.empty()) {
        curl_easy_setopt(easy_handle, CURLOPT_SSL_VERIFYPEER, 0L);  //不验证证书
        curl_easy_setopt(easy_handle, CURLOPT_SSL_VERIFYHOST, 0L);  //不验证HOST
    } else {
        //缺省情况就是PEM，所以无需设置，另外支持DER
        curl_easy_setopt(easy_handle, CURLOPT_SSL_VERIFYPEER, true);
        curl_easy_setopt(easy_handle, CURLOPT_CAINFO, m_ca_path.c_str());
    }
}

size_t AsyncHttpClient::WriteCallback(void* ptr, size_t size, size_t nmemb, void* data) {
    std::string* response = (std::string*)data;
    response->append((char*)ptr, size * nmemb);

    return (size * nmemb);
}
