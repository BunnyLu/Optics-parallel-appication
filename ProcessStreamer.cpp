#include "ProcessStreamer.h"
#include <stdexcept>
#include <iostream>
#include <cstdint>
#include <string>
#include <thread>
#include "BufferQueue.h"
#include "AlazarError.h"
#include "AlazarApi.h"
#include "AlazarCmd.h"
#include "cuda_runtime.h"
#include "cuda_runtime_api.h"
#include <cuda_runtime_api.h>

#ifdef _WIN32
#include <conio.h>
#else // ifndef _WIN32
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define TRUE  1
#define FALSE 0

#define _snprintf snprintf

inline U32 GetTickCount(void);
inline void Sleep(U32 dwTime_ms);
inline int _kbhit(void);
inline int GetLastError();
#endif // ifndef _WIN32


void ProcessStreamer::StreamFunc()
{
	while (m_streaming) 
	{
		if (m_cons_in->size() <= 0)	continue;

		ProcessStreamer::Consumer_element_t* buf = m_cons_in->front();
		m_cons_in->pop_front();
		if (processedbuf == 0) 
		{
			cudaMemcpy(dev_a1, buf, bufferbytes, cudaMemcpyHostToDevice);
			m_cons_out->push_back(buf);
		}
		else
		{
			PrcWithCUDA(buf);
			m_cons_out->push_back(buf);
		}	
		processedbuf++;
		if (cudaGetLastError() != cudaSuccess) {printf( "Kernel launch failed\n");}

		if (processedbuf % 1000 == 0)
		{
			cudaMemcpy(photonNumber1, dev_n1, sizeof(int), cudaMemcpyDeviceToHost);
			cudaMemcpy(photonNumber2, dev_n2, sizeof(int), cudaMemcpyDeviceToHost);
	//		printf("%d photons in pool 1.\n", (*photonNumber1));
		//	printf("%d photons in pool 2.\n", (*photonNumber2));
			cudaMemset(dev_n1, 0, sizeof(int));
			cudaMemset(dev_n2, 0, sizeof(int));
		}

	}
	
 }

ProcessStreamer::ProcessStreamer()
{
}


ProcessStreamer::~ProcessStreamer()
{
	StopStreaming();
}


void ProcessStreamer::Setup()
{
	bufferbytes = 204800;
	m_streaming = true;
	processedbuf = 0;
	totalnum = 0;
 
	cudaMallocHost(&photonNumber1, sizeof(int));
	cudaMallocHost(&photonNumber2, sizeof(int));
	memset(photonNumber1, 0, sizeof(int));
	memset(photonNumber2, 0, sizeof(int));

	cudaMalloc((void**)&dev_a1, (size_t)bufferbytes);
	cudaMalloc((void**)&dev_n1, sizeof(int));
	cudaMemset(dev_n1, 0, sizeof(int));	
	cudaMalloc((void**)&dev_a2, (size_t)bufferbytes);
	cudaMalloc((void**)&dev_n2, sizeof(int));
	cudaMemset(dev_n2, 0, sizeof(int));

//	cudaStreamCreate(&stream1);
//	cudaStreamCreate(&stream2);
}

void ProcessStreamer::StartStreaming()
{
	m_streaming = true;

	threadHandle = CreateThread(
		NULL,          // default security attributes
		0,             // use default stack size  
		ProcThread,    // thread function name
		this,          // argument to thread function 
		0,             // use default creation flags 
		&threadID);    // returns the thread identifier 
}


void ProcessStreamer::StopStreaming()
{
	cudaFreeHost(photonNumber1);
	cudaFreeHost(photonNumber2);
	cudaFree(dev_a1);
	cudaFree(dev_a2);
	cudaFree(dev_n1);
	cudaFree(dev_n2);
//	cudaStreamDestroy(stream1);
//	cudaStreamDestroy(stream2);
	m_streaming = false;
	WaitForSingleObject(threadHandle, INFINITE);
	CloseHandle(threadHandle);

//	printf("%d photons \n", totalnum);
//	printf("Process %d buffers \n", processedbuf);
}

DWORD WINAPI ProcessStreamer::ProcThread(LPVOID lpParam)
{
	ProcessStreamer* streamer = (ProcessStreamer*)lpParam;
	streamer->StreamFunc();
	return 0;
}
