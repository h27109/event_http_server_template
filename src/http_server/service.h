#ifndef __SERVICE_H__
#define __SERVICE_H__

#include <string>

class Service
{
public:
    static void Serve();
    //停止自定义服务
    static void Fini();

private:
    static bool Init();

    //启动自定义服务
    static bool Start();

    static bool InitHttpServer();

    static bool InitCpp2Sky();
};

#endif  //__SERVICE_H__
