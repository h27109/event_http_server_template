#include "service.h"

#include <arpa/inet.h>
#include <netdb.h>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>

#include "alarm_client/alarm_client.h"
#include "apollo/apollo.h"
#include "common/app_conf.h"
#include "common/async_http_client.h"
#include "common/string_utility.h"
#include "common/thread_pool.h"
#include "database/db_oracle.h"
#include "http_server.h"
#include "http_service.h"
#include "log/async_logging.h"
#include "log/log.h"
#include "mexception.h"
#include "transport/async_mq_client.h"
#include "transport/async_mq_server.h"
#include "transport/zk_client.h"
#include "transport/zk_server.h"

#include "http_server.h"
#include "http_service.h"
#include "busi_thread.h"

#include <functional>

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
        //lspf::log::Log::SetLogPriority(conf->GetLocalConfStr("Log", "Priority"));
        lspf::log::Log::SetLogPriority("INFO");

        ///设置输出设备类型,默认为输出到文件, "FILE", "STDOUT"
        //lspf::log::Log::SetOutputDevice(conf->GetLocalConfStr("Log", "DeviceType"));
        lspf::log::Log::SetOutputDevice("FILE");

        ///设置输出路径
        //lspf::log::Log::SetFilePath(conf->GetLocalConfStr("Log", "Path"));
        lspf::log::Log::SetFilePath(".");

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

bool Service::InitHttpServer() {
    // 设置http service的响应发送处理函数
    ReplyFunc reply_func = std::bind(&HttpServer::Reply, HttpServer::Instance(), std::placeholders::_1, std::placeholders::_2);
    HttpService::Instance()->SetReplyFunc(reply_func);

    // 注册业务处理函数
    // HttpService::Instance()->Registe("/healthz", health);
    // HttpService::Instance()->Registe("/ReadyHealthz", ready);
    // HttpService::Instance()->Registe("/bank_sale", bank_sale);

    // 设置http service的响应发送处理函数
    //HttpServer::Instance()->Start(8080, HttpService::Instance());

    thread t(bind(&HttpServer::Start, HttpServer::Instance(), 8080, HttpService::Instance()));
    t.detach();
    return true;
}


bool Service::Init() {
    //初始化日志服务
    InitLog();
    std::cout << "InitLog Done..." << std::endl;

    InitHttpServer();
    std::cout << "InitHttpServer Done..." << std::endl;

    return true;
}

//启动自定义服务
bool Service::Start() {

    PLOG_INFO("===============starting=======================");
    BusiThread busi_thread;

    busi_thread.Start();

    return true;
}

//停止自定义服务
void Service::Fini() {
    PLOG_INFO("Service::Fini begin...");

    sleep(1);

    PLOG_INFO("Service::Fini end...");
}
