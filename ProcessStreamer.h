#pragma once
#include <string>
#include <thread>
#include "BufferQueue.h"
#include "windows.h"

#ifndef INT64
#define INT64 S64
#endif


class ProcessStreamer
{
public:
	typedef unsigned char U8, *PU8, BOOLEAN;
	typedef unsigned long U32, *PU32;
	typedef U8 Consumer_element_t;
	typedef BufferQueue<Consumer_element_t*> Consumer_queue_t;
	ProcessStreamer();
	~ProcessStreamer();
	Consumer_queue_t* GetConsumerInputQueue() { return m_cons_in; }
	void SetConsumerInputQueue(Consumer_queue_t* in) { m_cons_in = in; }
	Consumer_queue_t* GetConsumerOutputQueue() { return m_cons_out; }
	void SetConsumerOutputQueue(Consumer_queue_t* out) { m_cons_out = out; }
	void Setup();
	void StartStreaming();
	void StopStreaming();

	U32 bufferbytes;
//	U8 *ProcessBuf;    //input pointer for host -->buf
	int *photonNumber1; //output pointer for host
	int *photonNumber2; 
	Consumer_element_t *dev_a1;  //input pointer for device
	Consumer_element_t *dev_a2; 
	int *dev_n1;  //output pointer for device
	int *dev_n2;
	int processedbuf;
	int totalnum;

	HANDLE  threadHandle;
	DWORD   threadID;
	static DWORD WINAPI ProcThread(LPVOID lpParam);

//	cudaStream_t stream1;
//	cudaStream_t stream2;

private:
	Consumer_queue_t* m_cons_in;//consumer BufferQueue providing data to ProcessStreamer
	Consumer_queue_t* m_cons_out;//consumer BufferQueue that returns used buffers to the producer
	bool m_streaming;

	void StreamFunc();
	void PrcWithCUDA(ProcessStreamer::Consumer_element_t *buffer);
};

