#!/bin/sh

# activate debugging from here
#set -x
 # stop debugging from here
# set +x

buildtype="debug"
buildwithtest=TRUE
argcount=$#
run_testcase=FALSE
run_app=FALSE

CURR_DIR="$( cd "$( dirname "$0"  )" && pwd  )"

cd $CURR_DIR

run_app_name=`echo $CURR_DIR |awk -F'/' '{print $NF}' | sed 's#\-#\_#g'`
etc_file=${run_app_name}.ini

for ((i=0; i<argcount; i++))
do
    if [ $1 == "run_testcase" ]; then
        run_testcase=TRUE
    elif [ $1 == "run_app" ]; then
        run_app=TRUE
    elif [ $1 == "build_no_test" ]; then
        buildwithtest=FALSE
    elif [ $1 == "debug" -o $1 == "release" ]; then
        buildtype=$1
    elif [ $1 == "install" ]; then
        BUILD_INSTALL=install
    elif [ $1 == "-r" ]; then
        shift
        BUILD_SUB_DIR=$1
        i=$i+1
    fi
    shift
done

if ["$BUILD_SUB_DIR" == ""] ; then
	BUILD_SUB_DIR=$run_app_name
fi

PROJECT_DIR=`pwd`
SOURCE_DIR=`pwd`
BUILD_DIR=`pwd`/build
BUILD_TYPE=${BUILD_TYPE:-$buildtype}

BASE_LAB_COMMON_PATH=$PROJECT_DIR/../base-library/common/contrib/lib
BASE_ORACLE_PATH=$PROJECT_DIR/../base-library/oracle
BASE_TESTFW_PATH=$PROJECT_DIR/../base-testfw

INSTALL_DIR=${INSTALL_DIR:-./}
BUILD_NO_EXAMPLES=${BUILD_NO_EXAMPLES:-0}

mkdir -p $BUILD_DIR/$BUILD_TYPE/$BUILD_SUB_DIR \
  && mkdir -p $BUILD_DIR/$BUILD_TYPE/bin \
  && cd $BUILD_DIR/$BUILD_TYPE/$BUILD_SUB_DIR \
  && cmake \
           -DCMAKE_PROJECT_DIR=$PROJECT_DIR \
           -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
           -DCMAKE_LOCAL_PATH=$BUILD_DIR \
           -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
           -DCMAKE_BUILD_NO_EXAMPLES=$BUILD_NO_EXAMPLES \
           -DBUILD_WITH_TEST=${buildwithtest} \
           $SOURCE_DIR 
           
if ! make -j8 $BUILD_INSTALL; then
    exit  1
fi


# 服务启动
if [ ${run_app} == TRUE ]; then
    # 服务启动
    echo "run app -->" $run_app_name
    cd $BUILD_DIR/
    rm -rf $BUILD_DIR/lib
    mkdir $BUILD_DIR/lib
    # 数据库参数
    export ORACLE_HOME=$BASE_ORACLE_PATH
    export PATH=$PATH:$ORACLE_HOME/bin/
    # 运行库参数
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$ORACLE_HOME/lib
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$BUILD_DIR/lib
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$BASE_LAB_COMMON_PATH
    export NLS_LANG=AMERICAN_AMERICA.AL32UTF8
    export LANG=en_US.UTF-8
    export TZ='Asia/Shanghai'
    # ln 运行库
    ln -b -s $BASE_LAB_COMMON_PATH/libboost_system.so $BUILD_DIR/lib/libboost_system.so.1.55.0
    ln -b -s $BASE_LAB_COMMON_PATH/libboost_thread.so $BUILD_DIR/lib/libboost_thread.so.1.55.0
    ln -b -s $BASE_LAB_COMMON_PATH/libthrift.so $BUILD_DIR/lib/libthrift-0.9.3.so
    ln -b -s $BASE_LAB_COMMON_PATH/libthriftnb.so $BUILD_DIR/lib/libthriftnb-0.9.3.so
    ln -b -s $BASE_LAB_COMMON_PATH/libzookeeper_mt.so $BUILD_DIR/lib/libzookeeper_mt.so.2
    ln -b -s $BASE_LAB_COMMON_PATH/libevent.so $BUILD_DIR/lib/libevent-2.0.so.5
    ln -b -s $BASE_LAB_COMMON_PATH/libactivemq-cpp.so $BUILD_DIR/lib/libactivemq-cpp.so.19
    # 结束accp_record进程
    PROCESS=`ps -ef|grep $run_app_name|grep -v grep|grep -v PPID|awk '{ print $2}'`
    for i in $PROCESS
    do
        kill -9 $i
    done
    rm -rf $BUILD_DIR/run
    mkdir $BUILD_DIR/run
    $BUILD_DIR/$buildtype/bin/$run_app_name $BUILD_DIR/etc/$etc_file &
    # gdb $BUILD_DIR/$buildtype/bin/$run_app_name
fi

# 执行测试案例
if [ ${run_testcase} == TRUE ]; then
    echo "test app -->" $run_app_name

    export PROJECT_DIR_TESTCASE=$PROJECT_DIR/testcase
    # 执行自动化案例
    cd $BASE_TESTFW_PATH
    source  ./runtestcase.sh
    # 服务终止
    PROCESS=`ps -ef|grep $run_app_name|grep -v grep|grep -v PPID|awk '{ print $2}'`
    for i in $PROCESS
    do
        kill -9 $i
    done
fi

killall event_http_server_template
killall multi_client

sleep 5

cd ../../..

echo `pwd`

mv ./build/release/bin/event_http_server_template .
mv ./build/release/bin/multi_client .

./event_http_server_template

sleep 5

./multi_client
