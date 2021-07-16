PROJECT(lspf)

CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

set(CMAKE_MODULE_PATH "${CMAKE_LOCAL_PATH}/cmake")

message("Build CMAKE_MODULE_PATH:" ${CMAKE_MODULE_PATH})

#ADD_DEFINITIONS(-DTIXML_USE_STL) #tinyxml stl

include(DefineOptions)

# include directories
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

aux_source_directory(http_server SRC_LIST)

SET(TARGET_FILE event_http_server_template)
add_executable(${TARGET_FILE} ${SRC_LIST})
target_link_libraries(${TARGET_FILE} apollo)
target_link_libraries(${TARGET_FILE} transport)
target_link_libraries(${TARGET_FILE} message)
target_link_libraries(${TARGET_FILE} pthread)
target_link_libraries(${TARGET_FILE} boost_system)
target_link_libraries(${TARGET_FILE} boost_thread)
target_link_libraries(${TARGET_FILE} thrift)
target_link_libraries(${TARGET_FILE} thriftnb)
target_link_libraries(${TARGET_FILE} zookeeper_mt)
target_link_libraries(${TARGET_FILE} event)
target_link_libraries(${TARGET_FILE} ssl)
target_link_libraries(${TARGET_FILE} occi)
target_link_libraries(${TARGET_FILE} jsoncpp)
target_link_libraries(${TARGET_FILE} clntsh)
target_link_libraries(${TARGET_FILE} activemq-cpp)
target_link_libraries(${TARGET_FILE} crypto_alg)
target_link_libraries(${TARGET_FILE} dbpool)
target_link_libraries(${TARGET_FILE} log)
target_link_libraries(${TARGET_FILE} common)
target_link_libraries(${TARGET_FILE} alarm_client)
target_link_libraries(${TARGET_FILE} curl)


aux_source_directory(test SRC_LIST1)

SET(TARGET_FILE1 multi_client)
add_executable(${TARGET_FILE1} ${SRC_LIST1})
target_link_libraries(${TARGET_FILE1} apollo)
target_link_libraries(${TARGET_FILE1} transport)
target_link_libraries(${TARGET_FILE1} message)
target_link_libraries(${TARGET_FILE1} pthread)
target_link_libraries(${TARGET_FILE1} boost_system)
target_link_libraries(${TARGET_FILE1} boost_thread)
target_link_libraries(${TARGET_FILE1} thrift)
target_link_libraries(${TARGET_FILE1} thriftnb)
target_link_libraries(${TARGET_FILE1} zookeeper_mt)
target_link_libraries(${TARGET_FILE1} event)
target_link_libraries(${TARGET_FILE1} ssl)
target_link_libraries(${TARGET_FILE1} occi)
target_link_libraries(${TARGET_FILE1} jsoncpp)
target_link_libraries(${TARGET_FILE1} clntsh)
target_link_libraries(${TARGET_FILE1} activemq-cpp)
target_link_libraries(${TARGET_FILE1} crypto_alg)
target_link_libraries(${TARGET_FILE1} dbpool)
target_link_libraries(${TARGET_FILE1} log)
target_link_libraries(${TARGET_FILE1} common)
target_link_libraries(${TARGET_FILE1} alarm_client)
target_link_libraries(${TARGET_FILE1} curl)
