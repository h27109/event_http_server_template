#ifndef __SESSION_CACHE_H__
#define __SESSION_CACHE_H__

#include <list>
#include <map>
#include <iostream>
#include "string.h"
#include "stdio.h"
#include "assert.h"
#include "common/mutex.h"
#include <vector>
#include "log/log.h"

using namespace std;

template<typename value_type, typename key_type>
class SessionCache
{
public:
    typedef struct
    {
        value_type value;
        key_type key;
        int timestamp;
    }Node;

    typedef typename std::list<Node>::iterator list_It;
    typedef typename std::map<key_type, list_It>::iterator map_It;

    SessionCache(int timeout_ = 60):m_timeout(timeout_), m_max_size(100)
    {
        ClearAll();
    }

    ~SessionCache()
    {
        ClearAll();
    }

    void SetTimeout(const int timeout_)
    {
        if (timeout_ > 0){
            m_timeout = timeout_;
        }
    }

    void SetMaxSize(const size_t max_size)
    {
        m_max_size = max_size;
    }

    void ClearAll()
    {
        AutoLocker lock(&m_mutex);
        m_list.clear();
        m_map.clear();
    }

    size_t Size()
    {
        AutoLocker lock(&m_mutex);
        assert(m_list.size() == m_map.size());
        return m_map.size();
    }

    //插入节点
    bool PushSession(const key_type& key_, const value_type& value_)
    {


        AutoLocker lock(&m_mutex);

        AutoClearTimeout();

        if(m_map.find(key_) == m_map.end())
        {
            Node node;
            node.value = value_;
            node.key = key_;
            node.timestamp = time(NULL);

            m_list.push_back(node);
            list_It listIt = --m_list.end();
            m_map.insert(std::pair<key_type, list_It>(key_, listIt));
            //std::cout << "Push value, key:value is  " << key_ << " : " << value_ << std::endl;
            PLOG_INFO("push to session ok, size=%d", m_map.size());
            return true;
        }
        else
        {
            PLOG_INFO("push to session failed");
            return false;
        }
    }

    //查找结点
    bool FindSession(const key_type& key_, value_type& value_)
    {
        AutoLocker lock(&m_mutex);
        map_It mapIt = m_map.find(key_);
        if(mapIt == m_map.end())
        {
            //std::cout << "find session failed, key is   " << key_ << std::endl;
            return false;
        }
        value_ = mapIt->second->value;
        assert(mapIt->second->key == key_);
        //std::cout << "find session OK, key:value is  " << key_ << " : " << value_ << std::endl;
        return true;
    }

    //删除指定key的结点，并返回结点值
    bool PopSession(const key_type& key_, value_type *value_)
    {
        AutoLocker lock(&m_mutex);
        map_It mapIt = m_map.find(key_);
        if(mapIt == m_map.end())
        {
            //std::cout << "Pop session failed, key is   " << key_ << std::endl;
            PLOG_INFO("pop session faild");
            return false;
        }
        *value_ = mapIt->second->value;
        assert(mapIt->second->key == key_);
        m_list.erase(mapIt->second);
        m_map.erase(key_);
        //std::cout << "Pop session OK, key:value   " << key_ << " : " << value_ << std::endl;
        PLOG_INFO("pop session ok, size=%d", m_map.size());
        return true;
    }

    //删除指定key的结点
    bool DelSession(const key_type& key_)
    {
        AutoLocker lock(&m_mutex);
        map_It mapIt = m_map.find(key_);
        if(mapIt != m_map.end())
        {
            //std::cout << "Del session ok, key:value is   " << key_ << " : " << mapIt->second->value << std::endl;
            m_list.erase(mapIt->second);
            m_map.erase(key_);
            return true;
        }
        else
        {
            //std::cout << "Del session failed, key is  " << key_ << std::endl;
            return false;
        }
    }

    //删除并返回首结点
    bool PopFirstSession(key_type& key_, value_type& value_)
    {

        AutoLocker lock(&m_mutex);
        if(Size() == 0)
        {
            //std::cout << "Pop first session get empyt" << std::endl;
            return false;
        }
        list_It listIt = m_list.begin();
        key_ = listIt->key;
        value_ = listIt->value;
        m_map.erase(key_);
        m_list.erase(listIt);
        //std::cout << "Pop first session OK, key:value is  " << key_ << " : " << value_ << std::endl;
        return true;
    }



    //删除超时的结点
    void ClearTimeout()
    {
        AutoLocker lock(&m_mutex);
        list_It listIt = m_list.begin();
        int curtime = time(NULL);

        std::string key;
        while(listIt != m_list.end()){
            if(curtime - listIt->timestamp < m_timeout){
                ///没有节点超时
                break;
            }

            key = listIt->key;

            list_It listIt1 = listIt;
            listIt++;
            m_list.erase(listIt1);
            m_map.erase(key);
        }
    }

    //根据指定超时时长删除结点
    void ClearTimeout(const int timeout)
    {
        AutoLocker lock(&m_mutex);
        list_It listIt = m_list.begin();
        int curtime = time(NULL);

        std::string key;
        while(listIt != m_list.end()){
            if(curtime - listIt->timestamp < timeout){
                ///没有节点超时
                break;
            }

            key = listIt->key;

            list_It listIt1 = listIt;
            listIt++;
            m_list.erase(listIt1);
            m_map.erase(key);
        }
    }
		
    //删除超时的结点，并返回被删除的节点
    void ClearTimeout(int timeout, std::vector<value_type>& valuevec, std::vector<key_type>& keyvec)
    {
        AutoLocker lock(&m_mutex);
        list_It listIt = m_list.begin();
        int curtime = time(NULL);
        Node node;
        while(listIt != m_list.end()){
            if(curtime - listIt->timestamp < timeout){
                break;
            }

            node = *listIt;

            list_It listIt1 = listIt;
            listIt++;
            valuevec.push_back(node.value);
            keyvec.push_back(node.key);
            m_list.erase(listIt1);
            m_map.erase(node.key);
        }
    }

    //删除超时的结点，并返回被删除的节点
    void ClearTimeout(std::vector<value_type>& valuevec, std::vector<key_type>& keyvec)
    {
        AutoLocker lock(&m_mutex);
        list_It listIt = m_list.begin();
        int curtime = time(NULL);
        Node node;
        while(listIt != m_list.end()){
            if(curtime - listIt->timestamp < m_timeout){
                break;
            }

            node = *listIt;

            list_It listIt1 = listIt;
            listIt++;
            valuevec.push_back(node.value);
            keyvec.push_back(node.key);
            m_list.erase(listIt1);
            m_map.erase(node.key);
        }
    }
private:
    //自动删除超时的结点不上锁
    void AutoClearTimeout()
    {
        if (m_map.size() <= m_max_size){
            return;
        }

        list_It listIt = m_list.begin();
        int curtime = time(NULL);

        std::string key;
        while(listIt != m_list.end()){
            if(curtime - listIt->timestamp < m_timeout){
                ///没有节点超时
                break;
            }

            key = listIt->key;

            list_It listIt1 = listIt;
            listIt++;
            m_list.erase(listIt1);
            m_map.erase(key);
        }
    }
private:
    TMutex m_mutex;
    int m_timeout;
    size_t m_max_size;
    std::map<key_type, list_It> m_map;
    std::list<Node> m_list;
};

#endif
