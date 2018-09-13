#pragma once
#include <cstdint>
#include <string>
#include <thread>
#include "BufferQueue.h"
#include "AlazarError.h"
#include "AlazarApi.h"
#include "AlazarCmd.h"
#include "IoBuffer.h"
#include "windows.h"


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

#define BUFFER_COUNT 8


class AcqStreamer
{
public:
	typedef unsigned char U8, *PU8, BOOLEAN;
	typedef unsigned long U32, *PU32;
	typedef U8 Producer_element_t;
	typedef BufferQueue<Producer_element_t*> Producer_queue_t;

	int channelsPerBoard = 2;
	double samplesPerSec = 1000000000.0;
	double acquisitionLength_sec = 5.;//Select the total acquisition length in seconds
	U32 samplesPerBuffer = 204800;  //Select the number of samples in each DMA buffer, default=204800				
	U32 channelMask = CHANNEL_A;//Select which channels to capture (A, B, or both)
	U32 timeout_ms = 5000; //Set a buffer timeout that is longer than the time required to capture all the records in one buffer, default=5000
	
	int channelCount;
	int buffercopied;
	U8 bitsPerSample;


	float bytesPerSample;
	RETURN_CODE retCode;
	HANDLE AcqHandle;
	U8 *pBuffer;
	U32 bytesPerBuffer;
	U32 maxSamplesPerChannel;
	U32 startTickCount;
	U32 buffersPerAcquisition;
	U32 buffersCompleted;
	U32 bufferIndex;
	INT64 bytesTransferred;
	INT64 bytesCopied;
	INT64 samplesPerAcquisition;
	BOOL success; 	
	
	HANDLE  threadHandle;
	DWORD   threadID;
	static DWORD WINAPI AcqThread(LPVOID lpParam);
	AcqStreamer();
	~AcqStreamer();
	Producer_queue_t* GetProducerInputQueue() { return m_prod_in; }
	void SetProducerInputQueue(Producer_queue_t* in) { m_prod_in = in; }
	Producer_queue_t* GetProducerOutputQueue() { return m_prod_out; }
	void SetProducerOutputQueue(Producer_queue_t* out) { m_prod_out = out; }
	
	BOOL ConfigureBoard(HANDLE CfgBoardHandle);

	void Setup();
	void StartStreaming();
	void StopStreaming();

private:
	Producer_queue_t* m_prod_in;//producer BufferQueue with unused buffers
	Producer_queue_t* m_prod_out;//producer BufferQueue with copied data from AcqStreamer
	IO_BUFFER **IoBufferArray;
	bool m_set_up;
	bool m_streaming;
	void StreamFunc();

};

