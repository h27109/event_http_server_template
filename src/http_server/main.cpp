#include <iostream>
#include "service.h"
#include "log/log.h"

using namespace std;

#include "service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <iostream>
#include <sys/types.h> /* about files */
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

#include "common/app_conf.h"
#include "common/process_control.h"
#include "common/base64.h"
#include "log/log.h"
#include "crypto_alg/crypto_alg.h"

static struct sigaction act;

bool loadConfig(int argc, char *argv[])
{
    std::string filename;
    if (argc > 1){
        filename.assign(argv[1]);
    }else{
        filename = std::string(argv[0]) + ".ini";
    }

    return AppConf::Instance()->LoadConf(filename);
}

//截获KILL/PKILL信号
void catcher(int signo)
{
	//停止服务
	Service::Fini();
	exit(0);
}

//CTRL_C退出
void trapPrompt(int signo)
{
	char prompt[] = "Are you sure exit program now[y|n]? ";
	char op[80] = {0};
	std::cin.clear();
	std::cout.write(prompt, strlen(prompt));
	std::cin.read(op, 1);

	if ('y' == op[0] || 'Y' == op[0])
	{
		Service::Fini();
		exit(0);
	}
}

void trapSigint()
{
	struct sigaction act;
	act.sa_handler = trapPrompt;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sigaction(SIGINT, &act, NULL) < 0)
	{
		PLOG_ERROR("Install signal action error: %s\n", strerror(errno));
		exit(1);
	}
}

//安装守护进程
void initDaemon(const std::string &app_name)
{
	int pid;
	//忽略终端I/O信号,STOP信号
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	if ((pid = fork()) > 0){
		exit(0);    //是父进程，结束父进程
	}else if (pid < 0){
		exit(1);    //fork失败，退出
	}

	//是第一子进程,后台继续执行
	setsid();

	//与控制终端分离
	if ((pid = fork()) > 0){
		exit(0);    //是第一子进程，结束第一子进程
	}else if (pid < 0){
		exit(1);    //fork失败，退出
	}
    ///避免进程重复启动
    if (!ProcessControl::AlreadyRunning()){
        std::cout << "process is already running!" << std::endl;
        exit(1);
    }

	signal(SIGCHLD, SIG_IGN);   //不处理僵尸进程（由系统处理）
	signal(SIGINT, SIG_IGN);    //忽略Control-C
	signal(SIGHUP, SIG_IGN);    //忽略LoggedOut
	//截取kill/pkill命令
	act.sa_handler = catcher;
	act.sa_flags = 0;
	sigfillset(&(act.sa_mask));
	sigaction(SIGTERM, &act, NULL);     //KILL
	sigaction(SIGKILL, &act, NULL);     //PKILL
	sigaction(SIGQUIT, &act, NULL);     //PKILL

    char* chCurPath = getcwd(NULL, 0); // 当前工作目录
	//设置当前工作目录
	chdir(chCurPath);
    std::cout << "chCurPath=" << chCurPath << std::endl;
    free(chCurPath);
}
//安装普通进程
void initNodaemon()
{
    trapSigint();
}
/**
 * 应用程序入口
 **/
int main(int argc, char* argv[])
{
	//打开程序crash记录调用栈功能
    lspf::log::Log::EnableCrashRecord();

	//读入配置文件信息
	loadConfig(argc, argv);

#ifdef DEBUG
  //以控制台程序的方式启动
	initNodaemon();
#else
    //以后台服务的方式启动
	initDaemon(argv[0]);
#endif  //DEBUG

    Service::Serve();

    std::string xxtea_key_uri = "$ENC1Zj68XXLa7swl7Tw";
    std::string passwd = "$ENC1Zj68XXLa7swl7Tw";
    Base64::Decode("$ENC1Zj68XXLa7swl7Tw",&passwd);
    XxteaCrypto::Decrypt("$ENC1Zj68XXLa7swl7Tw", xxtea_key_uri, passwd);

	return 0;
}
