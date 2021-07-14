#include "service.h"

#include <arpa/inet.h>
#include <netdb.h>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/thread.hpp>
#include <functional>
#include <iostream>
#include <thread>

#include "alarm_client/alarm_client.h"
#include "apollo/apollo.h"
#include "common/app_conf.h"
#include "common/async_http_client.h"
#include "common/string_utility.h"
#include "common/thread_pool.h"
#include "database/db_oracle.h"
#include "log/async_logging.h"
#include "log/log.h"
#include "mexception.h"
#include "transport/async_mq_client.h"
#include "transport/async_mq_server.h"
#include "transport/zk_client.h"
#include "transport/zk_server.h"
#include "common/uuid_generator.h"

void Service::Serve() {
    //初始化服务
    if (!Init()) {
        PLOG_FATAL("Service init failed!");
        return;
    }

    PLOG_INFO("Service::start()..");

    //开始端口服务
    if (!Start()) {
        PLOG_FATAL("Service Start failed!");
        return;
    }
}

//初始化日志服务
static bool InitLog() {
    try {
        AppConf *conf = AppConf::Instance();
        ///设置日志输出级别
        // lspf::log::Log::SetLogPriority(conf->GetLocalConfStr("Log", "Priority"));
        lspf::log::Log::SetLogPriority("INFO");

        ///设置输出设备类型,默认为输出到文件, "FILE", "STDOUT"
        //lspf::log::Log::SetOutputDevice(conf->GetLocalConfStr("Log", "DeviceType"));
        lspf::log::Log::SetOutputDevice("STDOUT");

        ///设置输出路径
        //lspf::log::Log::SetFilePath(conf->GetLocalConfStr("Log", "Path"));
        lspf::log::Log::SetFilePath("/tmp");


        ///设置输出文件
        lspf::log::Log::SetFileName(conf->GetLocalConfStr("Log", "FileName"));

        ///设置单个log文件的最大大小，单位为"M bytes"
        lspf::log::Log::SetMaxFileSize(conf->GetLocalConfInt("Log", "MaxSize"));

        lspf::log::AsyncLogging::Instance()->Start();

    } catch (MException &mx) {
        PLOG_ERROR("call initLog Exception...:%s", mx.what());
        return false;
    }

    return true;
}

bool Service::Init() {
    //初始化日志服务
    InitLog();
    std::cout << "InitLog Done..." << std::endl;

    return true;
}

static void Recv(AsyncHttpClient *client) {
    int count = 0;
    while (1) {
        string session_id;
        AsyncResponseMsg response;
        int timeout_in_ms = 2000;
        if (client->TimedWaitResponse(session_id, response, timeout_in_ms)) {
            lspf::log::Log::SetLogId(session_id);
            PLOG_INFO("recv response, code=%d, count=%d", response.http_code, ++count);
            lspf::log::Log::SetLogId("");
        }
    }
}

//启动自定义服务
bool Service::Start() {
    AsyncHttpClient *client = new AsyncHttpClient();
    string url = "http://127.0.0.1:8080/xxx";
    string request = "abcd";
    string log_id = "log_id";
    vector<string> headers = {"content-type:application/json"};

    thread t(std::bind(Recv, client));
    t.detach();

    while (1) {
        int icount = 200;

        std::cout << "input send count" << std::endl;

        //std::cin >> icount;

        std::cout << "start to send, targe count=" << icount << std::endl;

        for (int i = 0; i < icount; ++i) {
            string uuid = GetStringUUID();
            client->Post(url, uuid, uuid, uuid, headers);
        }
        std::cout << "finished" << std::endl;

        sleep(2);
        
        break;
    }
    while(1)
    {
        sleep(100);
    }
    return true;
}

//停止自定义服务
void Service::Fini() {
    PLOG_INFO("Service::Fini begin...");

    sleep(1);

    PLOG_INFO("Service::Fini end...");
}
