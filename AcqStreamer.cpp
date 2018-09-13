#include "AcqStreamer.h"
#include <stdexcept>
#include "AlazarError.h"
#include "AlazarApi.h"
#include "AlazarCmd.h"

 

AcqStreamer::AcqStreamer()
{
	IoBufferArray = new IO_BUFFER*[BUFFER_COUNT];
	for (int i = 0; i < BUFFER_COUNT; ++i) IoBufferArray[i] = NULL;
}

AcqStreamer::~AcqStreamer()
{
	delete IoBufferArray;
	StopStreaming();
}

BOOL AcqStreamer::ConfigureBoard(HANDLE CfgBoardHandle)
{
	AcqHandle = CfgBoardHandle;
	RETURN_CODE retCode;

	// TODO: Specify the sample rate (see sample rate id below)

	samplesPerSec = 1000000000.0;

	retCode = AlazarSetCaptureClock(CfgBoardHandle,
		INTERNAL_CLOCK,
		SAMPLE_RATE_1GSPS,
		CLOCK_EDGE_RISING,
		0);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarSetCaptureClock failed -- %s\n", AlazarErrorToText(retCode));
		return FALSE;
	}


	// TODO: Select channel A input parameters as required

	retCode = AlazarInputControlEx(CfgBoardHandle,
		CHANNEL_A,
		DC_COUPLING,
		INPUT_RANGE_PM_400_MV,
		IMPEDANCE_50_OHM);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarInputControlEx failed -- %s\n", AlazarErrorToText(retCode));
		return FALSE;
	}

	// TODO: Select channel A bandwidth limit as required

	retCode = AlazarSetBWLimit(CfgBoardHandle,
		CHANNEL_A,
		0);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarSetBWLimit failed -- %s\n", AlazarErrorToText(retCode));
		return FALSE;
	}

	// TODO: Select channel B input parameters as required

	retCode = AlazarInputControlEx(CfgBoardHandle,
		CHANNEL_B,
		DC_COUPLING,
		INPUT_RANGE_PM_400_MV,
		IMPEDANCE_50_OHM);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarInputControlEx failed -- %s\n", AlazarErrorToText(retCode));
		return FALSE;
	}

	// TODO: Select channel B bandwidth limit as required

	retCode = AlazarSetBWLimit(CfgBoardHandle,
		CHANNEL_B,
		0);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarSetBWLimit failed -- %s\n", AlazarErrorToText(retCode));
		return FALSE;
	}

	// TODO: Select trigger inputs and levels as required

	retCode = AlazarSetTriggerOperation(CfgBoardHandle,
		TRIG_ENGINE_OP_J,
		TRIG_ENGINE_J,
		TRIG_CHAN_A,
		TRIGGER_SLOPE_POSITIVE,
		150,
		TRIG_ENGINE_K,
		TRIG_DISABLE,
		TRIGGER_SLOPE_POSITIVE,
		128);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarSetTriggerOperation failed -- %s\n", AlazarErrorToText(retCode));
		return FALSE;
	}

	// TODO: Select external trigger parameters as required

	retCode = AlazarSetExternalTrigger(CfgBoardHandle,
		DC_COUPLING,
		ETR_5V);

	// TODO: Set trigger delay as required.

	double triggerDelay_sec = 0;
	U32 triggerDelay_samples = (U32)(triggerDelay_sec * samplesPerSec + 0.5);
	retCode = AlazarSetTriggerDelay(CfgBoardHandle, triggerDelay_samples);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarSetTriggerDelay failed -- %s\n", AlazarErrorToText(retCode));
		return FALSE;
	}


	double triggerTimeout_sec = 0;
	U32 triggerTimeout_clocks = (U32)(triggerTimeout_sec / 10.e-6 + 0.5);

	retCode = AlazarSetTriggerTimeOut(CfgBoardHandle, triggerTimeout_clocks);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarSetTriggerTimeOut failed -- %s\n", AlazarErrorToText(retCode));
		return FALSE;
	}

	// TODO: Configure AUX I/O connector as required

	retCode = AlazarConfigureAuxIO(CfgBoardHandle, AUX_OUT_TRIGGER, 0);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarConfigureAuxIO failed -- %s\n", AlazarErrorToText(retCode));
		return FALSE;
	}

	return TRUE;
}

void AcqStreamer::Setup()
{  
	channelCount = 0;
	channelsPerBoard = 2;
	for (int channel = 0; channel < channelsPerBoard; channel++)
	{
		U32 channelId = 1U << channel;
		if (channelMask & channelId)
			channelCount++;
	}
	// Get the sample size in bits, and the on-board memory size in samples per channel
	RETURN_CODE retCode = AlazarGetChannelInfo(AcqHandle, &maxSamplesPerChannel, &bitsPerSample);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarGetChannelInfo failed -- %s\n", AlazarErrorToText(retCode));
		return;
	}

	// Calculate the size of each DMA buffer in bytes
	bytesPerSample = (float)((bitsPerSample + 7) / 8);
	bytesPerBuffer = (U32)(bytesPerSample * samplesPerBuffer * channelCount + 0.5);

	// Calculate the number of buffers in the acquisition
	samplesPerAcquisition = (INT64)(this->samplesPerSec * acquisitionLength_sec + 0.5);
	buffersPerAcquisition =
		(U32)((samplesPerAcquisition + samplesPerBuffer - 1) / samplesPerBuffer);

	// Allocate memory for DMA buffers
	success = TRUE;
	for (bufferIndex = 0; (bufferIndex < BUFFER_COUNT) && success; bufferIndex++)
	{
		this->IoBufferArray[bufferIndex] = CreateIoBuffer(bytesPerBuffer);
		if (this->IoBufferArray[bufferIndex] == NULL)
		{
			printf("Error: Alloc %u bytes failed\n", bytesPerBuffer);
			success = FALSE;
		}
	}


	if (success)
	{
		U32 admaFlags = ADMA_EXTERNAL_STARTCAPTURE | ADMA_CONTINUOUS_MODE;
		if (channelCount > 1)
			admaFlags |= ADMA_INTERLEAVE_SAMPLES; // Interleave samples for highest transfer rate
		retCode = AlazarBeforeAsyncRead(AcqHandle, channelMask,
			0, // Must be 0
			samplesPerBuffer,
			1,          // Must be 1
			0x7FFFFFFF, // Ignored. Behave as if infinite
			admaFlags);
		if (retCode != ApiSuccess)
		{
			printf("Error: AlazarBeforeAsyncRead failed -- %s\n", AlazarErrorToText(retCode));
			success = FALSE;
		}
	}

	// Add the buffers to a list of buffers available to be filled by the board

	for (bufferIndex = 0; (bufferIndex < BUFFER_COUNT) && (success == TRUE); bufferIndex++)
	{
		IO_BUFFER *pIoBuffer = IoBufferArray[bufferIndex];
		if (!ResetIoBuffer(pIoBuffer))
		{
			success = FALSE;
		}
		else
		{
			retCode =
				AlazarAsyncRead(
					AcqHandle,					// HANDLE -- board handle
					pIoBuffer->pBuffer,				// void* -- buffer
					pIoBuffer->uBufferLength_bytes,	// U32 -- buffer length in bytes
					&pIoBuffer->overlapped			// OVERLAPPED*
				);
			if (retCode != ApiDmaPending)
			{
				printf("Error: Setup AlazarAsyncRead %u failed -- %s\n", bufferIndex, AlazarErrorToText(retCode));
				success = FALSE;
			}
		}	
	}
	// Arm the board system to wait for a trigger event to begin the acquisition
	if (success)
	{
		retCode = AlazarStartCapture(AcqHandle);
		if (retCode != ApiSuccess)
		{
			printf("Error: AlazarStartCapture failed -- %s\n", AlazarErrorToText(retCode));
			success = FALSE;
		}
	}
		
}

void AcqStreamer::StartStreaming()
{
	m_streaming = true;
	threadHandle = CreateThread(
		NULL,           // default security attributes
		0,              // use default stack size  
		AcqThread,      // thread function name
		this,           // argument to thread function 
		0,              // use default creation flags 
		&threadID);     // returns the thread identifier 
		
}

void AcqStreamer::StopStreaming()
{
	m_streaming = false;
	WaitForSingleObject(threadHandle, INFINITE);
	CloseHandle(threadHandle);

}

void AcqStreamer::StreamFunc()
{
	// Wait for each buffer to be filled, process the buffer, and re-post it to
	// the board.
	if (success)
	{
		startTickCount = GetTickCount();
		buffersCompleted = 0;
		buffercopied = 0;
		bytesTransferred = 0;
		bytesCopied = 0;

		while (m_streaming)
		{
			// Wait for the buffer at the head of the list of available buffers
			// to be filled by the board.
			bufferIndex = buffersCompleted % BUFFER_COUNT;
			IO_BUFFER *pIoBuffer = IoBufferArray[bufferIndex];
			U32 result = WaitForSingleObject(pIoBuffer->hEvent, timeout_ms);
			if (result == WAIT_OBJECT_0)
			{
				// The wait succeeded
				U32 bytesTransferred;
				success = GetOverlappedResult(AcqHandle, &pIoBuffer->overlapped,
					&bytesTransferred, FALSE);
				if (!success)
				{
					// The transfer completed with an error
					U32 error = GetLastError();
					if (error != ERROR_OPERATION_ABORTED)
						printf("Error: The transfer failed with error %lu\n", error);
				}
			}
			else
			{
				// The wait failed
				success = FALSE;

				switch (result)
				{
				case WAIT_TIMEOUT:
					printf("Error: Wait timeout after %lu ms\n", timeout_ms);
					break;

				case WAIT_ABANDONED:
					printf("Error: Wait abandoned\n");
					break;

				default:
					printf("Error: Wait failed with error %lu -- %s\n", GetLastError());
					break;
				}
			}

			if (success)
			{
				// The buffer is full and has been removed from the list of buffers available for the board
				if (m_prod_in->size() <= 0)
				{
					continue;
				}

				buffersCompleted++;
				bytesTransferred += bytesPerBuffer;

				AcqStreamer::Producer_element_t* buf = m_prod_in->front();
				m_prod_in->pop_front();        //remove the buffer from the input queue
				memcpy(buf, pIoBuffer, bytesPerBuffer);
				m_prod_out->push_back(buf);    //return buffer to end of output queue
			}

			// Add the buffer to the end of the list of available buffers.
			if (success)
			{
				if (!ResetIoBuffer(pIoBuffer))
				{
					success = FALSE;
					printf("Reset iobuffer error \n");
				}
				else
				{
					retCode =
						AlazarAsyncRead(AcqHandle, pIoBuffer->pBuffer,
							pIoBuffer->uBufferLength_bytes, &pIoBuffer->overlapped);
					if (retCode != ApiDmaPending)
					{
						printf("Error:AlazarAsyncRead failed -- %s\n",
							AlazarErrorToText(retCode));
						success = FALSE;
					}
				}
			}

			// If the acquisition failed, exit the acquisition loop
			if (!success)
				break;

			// If a key was pressed, exit the acquisition loop
			if (_kbhit())
			{
				printf("Aborted...\n");
				break;
			}

			// Display progress
			printf("Completed %u buffers\r", buffersCompleted);
		}

		// Display results
		double transferTime_sec = (GetTickCount() - startTickCount) / 1000.;
		printf("Capture completed in %.2lf sec\n", transferTime_sec);

		double buffersPerSec;
		double bytesPerSec;

		if (transferTime_sec > 0.)
		{
			buffersPerSec = buffersCompleted / transferTime_sec;
			bytesPerSec = bytesTransferred / transferTime_sec;
		}
		else
		{
			buffersPerSec = 0.;
			bytesPerSec = 0.;
		}

		printf("Captured %u buffers (%.4g buffers per sec)\n", buffersCompleted, buffersPerSec);
		printf("Transferred %I64d bytes (%.4g bytes per sec)\n", bytesTransferred, bytesPerSec);
	}
	// Abort the acquisition
	retCode = AlazarAbortAsyncRead(AcqHandle);
	if (retCode != ApiSuccess)
	{
		printf("Error: AlazarAbortAsyncRead failed -- %s\n", AlazarErrorToText(retCode));
		success = FALSE;
	}

	// Free all memory allocated
	for (bufferIndex = 0; bufferIndex < BUFFER_COUNT; bufferIndex++)
	{
		if (IoBufferArray[bufferIndex] != NULL)
			DestroyIoBuffer(IoBufferArray[bufferIndex]);
	}
	printf("Acquisition successful!\n");
}
		
DWORD WINAPI AcqStreamer::AcqThread(LPVOID lpParam)
{
	AcqStreamer* streamer = (AcqStreamer*)lpParam;
	streamer->StreamFunc();
	return 0;
}