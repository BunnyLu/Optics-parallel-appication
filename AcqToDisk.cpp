
#include <stdio.h>
#include <string.h>
#include <thread>
#include "AcqStreamer.h"
#include "ProcessStreamer.h"
#include <deque>
#include "BufferQueue.h"
#include <memory>
#include <vector>
#include "Windows.h"
#include "AlazarError.h"
#include "AlazarApi.h"
#include "AlazarCmd.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cuda.h>
#include <device_functions.h> 


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

/*
inline U32 GetTickCount(void);
inline void Sleep(U32 dwTime_ms);
inline int _kbhit (void);
inline int GetLastError();*/
#endif // ifndef _WIN32

//-------------------------------------------------------------------------------------------------
//
// Function    :  main
//
// Description :  Program entry point
//
//-------------------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    // TODO: Select a board
    U32 systemId = 1;
    U32 boardId = 1;
    // Get a handle to the board
    HANDLE boardHandle = AlazarGetBoardBySystemID(systemId, boardId);
    if (boardHandle == NULL)
    {
        printf("Error: Unable to open board system Id %u board Id %u\n", systemId, boardId);
        return 1;
    }


	AcqStreamer aq;
	if (!aq.ConfigureBoard(boardHandle))
	{
		printf("Error: Configure board failed\n");
		return 1;
	}

	BufferQueue<AcqStreamer::Producer_element_t*> acq_to_prcs;
	BufferQueue<AcqStreamer::Producer_element_t*> prcs_to_acq;
	std::vector<std::unique_ptr<AcqStreamer::Producer_element_t[]>> acq_ps_buffers;
	for (int i = 0; i < 1024; ++i) {
		acq_ps_buffers.emplace_back(new AcqStreamer::Producer_element_t[aq.samplesPerBuffer]);
		prcs_to_acq.push_back(acq_ps_buffers[i].get());
	}
	aq.SetProducerInputQueue(&prcs_to_acq);
	aq.SetProducerOutputQueue(&acq_to_prcs);
	aq.AcqHandle = boardHandle;

	ProcessStreamer ps;
	ps.SetConsumerInputQueue(&acq_to_prcs);
	ps.SetConsumerOutputQueue(&prcs_to_acq);

	
	ps.Setup();  //setup CUDA before open the digitizer
	aq.Setup();   
	aq.StartStreaming();
	ps.StartStreaming();

	Sleep(5000);
	aq.StopStreaming();
	ps.StopStreaming();

    return 0;
}



#ifndef WIN32
inline U32 GetTickCount(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL) != 0)
		return 0;
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

inline void Sleep(U32 dwTime_ms)
{
	usleep(dwTime_ms * 1000);
}

inline int _kbhit (void)
{
  struct timeval tv;
  fd_set rdfs;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&rdfs);
  FD_SET (STDIN_FILENO, &rdfs);

  select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);
  return FD_ISSET(STDIN_FILENO, &rdfs);
}

inline int GetLastError()
{
	return errno;
}
#endif