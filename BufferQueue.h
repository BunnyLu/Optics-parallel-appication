#pragma once

#include <shared_mutex>
#include <deque>
#include "windows.h"
#include <mutex>


template <typename T>
class BufferQueue
{
public:
	
	BufferQueue() :m_beingmodified(), m_queue()
	{
	}
	~BufferQueue()
	{
	}
	T& front()
	{
		std::lock_guard<std::mutex> lock(m_beingmodified);
		return m_queue.front();
	}
	const T& front() const
	{
		std::lock_guard<std::mutex> lock(m_beingmodified);
		return m_queue.front();
	}
	void push_back(T& item)
	{
		std::lock_guard<std::mutex> lock(m_beingmodified);
		m_queue.emplace_back(item);
	}
	void push_back(const T& item)
	{
		std::lock_guard<std::mutex> lock(m_beingmodified);
		m_queue.emplace_back(item);
	}
	void pop_front()
	{
		std::lock_guard<std::mutex> lock(m_beingmodified);
		return m_queue.pop_front();
	}
	size_t size()
	{
		std::lock_guard<std::mutex> lock(m_beingmodified);
		return m_queue.size();
	}
private:
	std::mutex m_beingmodified;	//protects the queue when it's being read or modified
	std::deque<T> m_queue;	//queue that holds buffers (but can technically hold anything else)
};

