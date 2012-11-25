#include <iostream>
#include <fstream>
#include <list>
#include <windows.h>
#include <stdio.h>
#include "lzfx.h"
using namespace std;

#define THREADCOUNT 16

typedef unsigned int UINT;

// MUTEX HANDLER

HANDLE ghInBufferMutex, ghOutBufferMutex;

// EVENT HANDLER

HANDLE ghDispatcherEvents[THREADCOUNT], ghWorkerEvents[THREADCOUNT], ghCollectorEvent, ghDecompressEvent, ghFinallyDone;
pair<HANDLE, HANDLE> ghPairEvents[THREADCOUNT];

// THREAD HANDLER

HANDLE ghDispatcherThread, ghWorkerThreads[THREADCOUNT], ghCollectorThread;

// GLOBAL RESOURCE

list< pair<char*, UINT> > inBuffer;
list< pair<char*, UINT> > outBuffer;
UINT originalSize, turns, lastChunkLength;

// PROTOTYPE

DWORD WINAPI DispatcherThreadProc(LPVOID);
DWORD WINAPI WorkerThreadProc(LPVOID);
DWORD WINAPI CollectorThreadProc(LPVOID);

void CreateMutexAndEvents(void) {
	int i;

	ghInBufferMutex=CreateMutex(
		NULL,
		FALSE,
		NULL);

	if(ghInBufferMutex==NULL) {
		printf("CreateMutex: InBuffer failed (%d)\n", GetLastError());
		return ;
	}

	ghOutBufferMutex=CreateMutex(
		NULL,
		FALSE,
		NULL);

	if(ghOutBufferMutex==NULL) {
		printf("CreateMutex: OutBuffer failed (%d)\n", GetLastError());
		return ;
	}

	for(i=0; i<THREADCOUNT; i++) {
		ghDispatcherEvents[i]=CreateEvent(
			NULL,
			FALSE,
			FALSE,
			NULL);
		if(ghDispatcherEvents[i]==NULL) {
			printf("CreateEvent: Dispatcher%d failed (%d)\n", i, GetLastError());
			return ;
		}
	}

	for(i=0; i<THREADCOUNT; i++) {
		ghWorkerEvents[i]=CreateEvent(
			NULL,
			FALSE,
			FALSE,
			NULL);
		if(ghWorkerEvents[i]==NULL) {
			printf("CreateEvent: Worker%d failed (%d)\n", i, GetLastError());
			return ;
		}
	}

	for(i=0; i<THREADCOUNT; i++)
		ghPairEvents[i]=make_pair(ghDispatcherEvents[i], ghWorkerEvents[i]);

	ghCollectorEvent=CreateEvent(
		NULL,
		FALSE,
		TRUE,
		NULL);
	if(ghCollectorEvent==NULL) {
		printf("CreateEvent: Collector failed (%d)\n", GetLastError());
		return ;
	}

	ghDecompressEvent=CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);
	if(ghDecompressEvent==NULL) {
		printf("CreateEvent: Decompress failed (%d)\n", GetLastError());
		return ;
	}

	ghFinallyDone=CreateEvent(
		NULL,
		FALSE,
		FALSE,
		NULL);
	if(ghFinallyDone==NULL) {
		printf("CreateEvent: Final failed (%d)\n", GetLastError());
		return ;
	}
}

void CreateThreads(void) {
	int i;
	DWORD dwThreadId;

	ghDispatcherThread=CreateThread(
		NULL,
		0,
		DispatcherThreadProc,
		NULL,
		0,
		&dwThreadId);

	if(ghDispatcherThread==NULL) {
		printf("CreateThread: Dispatcher failed (%d)\n", GetLastError());
		return ;
	}

	for(i=0; i<THREADCOUNT; i++) {
		ghWorkerThreads[i]=CreateThread(
			NULL,
			0,
			WorkerThreadProc,
			&ghPairEvents[i],
			0,
			&dwThreadId);

		if(ghWorkerThreads[i]==NULL) {
			printf("CreateThread: Worker%d failed (%d)\n", i, GetLastError());
			return ;
		}
	}

	ghCollectorThread=CreateThread(
		NULL,
		0,
		CollectorThreadProc,
		NULL,
		0,
		&dwThreadId);

	if(ghCollectorThread==NULL) {
		printf("CreateThread: Collector failed (%d)\n", GetLastError());
		return ;
	}
}

void CloseEvents() {
	int i;
	for(i=0; i<THREADCOUNT; i++)
		CloseHandle(ghDispatcherEvents[i]);
	for(i=0; i<THREADCOUNT; i++)
		CloseHandle(ghWorkerEvents[i]);
	CloseHandle(ghCollectorEvent);
	CloseHandle(ghDecompressEvent);
	CloseHandle(ghFinallyDone);
}

void CalcTurns(char * fileName, UINT chunkSize) {
	ifstream f(fileName, ios::in|ios::ate);
	if(f.is_open())
		originalSize=(UINT)f.tellg();
	turns=(originalSize%chunkSize==0)? (originalSize/chunkSize): (originalSize/chunkSize)+1;
	lastChunkLength=originalSize-chunkSize*(turns-1);

	return ;
}

void WriteToBuffer(char * memBlock, UINT size) {
	DWORD dwWaitResult;

	dwWaitResult=WaitForSingleObject(
		ghInBufferMutex,
		INFINITE);

	switch(dwWaitResult) {
		case WAIT_OBJECT_0:
			printf("Dispatcher thread writing to the shared buffer...\n");
			inBuffer.push_back(make_pair(memBlock, size));
			ReleaseMutex(ghInBufferMutex);
			break;
		default:
			printf("WaitForSingleObject failed (%d)\n", GetLastError());
			return ;
	}

	printf("Dispatcher thread writing to the shared buffer is complete.\n");
}

int main(void) {
	DWORD dwWaitResult;
	char * fileName="input";
	UINT chunkSize=256*1024;

	CalcTurns(fileName, chunkSize);
	CreateMutexAndEvents();
	CreateThreads();

	dwWaitResult=WaitForSingleObject(
		ghCollectorThread,
		INFINITE);

	switch(dwWaitResult) {
		case WAIT_OBJECT_0:
			printf("All threads ended, cleaning up for application exit...\n");
			break;

		default:
			printf("WaitForSingleObject failed (%d)\n", GetLastError());
			return 1;
	}

	CloseEvents();

	//system("PAUSE");

	return 0;
}

DWORD WINAPI DispatcherThreadProc(LPVOID lpParam) {
	UNREFERENCED_PARAMETER(lpParam);
	char * memBlock=NULL;
	UINT i, j, current=0, end=0, size=0, chunkSize=256*1024;
	ifstream srcFile;

	srcFile.open("input", ios::in|ios::binary|ios::ate);
	if(srcFile.is_open()) {
		end=(UINT)srcFile.tellg();
		srcFile.seekg(0, srcFile.beg);

		for(i=0; i<turns; i+=j) {
			WaitForSingleObject(
				ghCollectorEvent,
				INFINITE);
			for(j=0; (j<THREADCOUNT)&&((j+i)<turns); j++) {
				current=(UINT)srcFile.tellg();
				if((end-current)<chunkSize)
					size=end-current;
				else
					size=chunkSize;
				memBlock=new char [size];
				srcFile.read(memBlock, size);
				WriteToBuffer(memBlock, size);			// BONUS
			}											// MAY
			for(int i=0; i<THREADCOUNT; i++)			// HAPPENS
				SetEvent(ghDispatcherEvents[i]);		// HERE
		}

		srcFile.close();
	}
	else
		return 1;

	WaitForSingleObject(
		ghDecompressEvent,
		INFINITE);
	printf("Dispatcher starts decompressing...\n");
	ifstream cFile;
	ofstream dFile;
	cFile.open("cmpFile", ios::in|ios::binary|ios::ate);
	dFile.open("output", ios::out|ios::binary);
	if(cFile.is_open()&&dFile.is_open()) {
		char *cMemBlock=NULL, *dMemBlock=NULL;
		UINT cSize=0, dSize=0;

		cSize=(UINT)cFile.tellg();
		cFile.seekg(0, cFile.beg);
		dSize=originalSize;
		cMemBlock=new char [cSize];
		dMemBlock=new char [dSize];
		cFile.read(cMemBlock, cSize);
		printf("GGGGG%d\n", lzfx_decompress(cMemBlock, cSize, dMemBlock, &dSize));
		dFile.write(dMemBlock, dSize);

		cFile.close();
		dFile.close();
	}

	SetEvent(ghFinallyDone);
	printf("Dispatcher thread exiting\n");

	return 0;
}

DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
	pair<HANDLE, HANDLE> * it=(pair<HANDLE, HANDLE>*) lpParam;
	HANDLE hDispatcherEvent=it->first;
	HANDLE hWorkerEvent=it->second;

	char * memBlock=NULL;
	DWORD dwWaitResult;
	UINT chunkSize=256*1024, cChunkSize=0;

	while(1) {
	printf("Worker Thread %d waiting for write event...\n", GetCurrentThreadId());

	dwWaitResult=WaitForSingleObject(
		hDispatcherEvent,
		INFINITE);

	switch(dwWaitResult) {
		case WAIT_OBJECT_0:
			WaitForSingleObject(
				ghInBufferMutex,
				INFINITE);
			WaitForSingleObject(
				ghOutBufferMutex,
				INFINITE);
			if(!inBuffer.empty()) {
				printf("Worker Thread %d reading from buffer, got %c\n", GetCurrentThreadId(), inBuffer.front());
				cChunkSize=((inBuffer.front()).second)*2;
				memBlock=new char [cChunkSize];
				printf("!ERROR! (%d)\n", lzfx_compress((inBuffer.front()).first, (inBuffer.front()).second,memBlock, &cChunkSize));
				inBuffer.pop_front();
				outBuffer.push_back(make_pair(memBlock, cChunkSize));
			}
			ReleaseMutex(ghOutBufferMutex);
			ReleaseMutex(ghInBufferMutex);
			SetEvent(hWorkerEvent);
			break;

		default:
			printf("Wait error (%d)\n", GetLastError());
			return 0;
	}
	}

	printf("Worker Thread %d exiting\n", GetCurrentThreadId());

	return 0;
}

DWORD WINAPI CollectorThreadProc(LPVOID lpParam) {
	UNREFERENCED_PARAMETER(lpParam);
	UINT i, j, end=0;
	UINT chunkSize=0;
	DWORD dwWaitResult;
	ofstream desFile;

	desFile.open("cmpFile", ios::out|ios::binary);
	if(desFile.is_open()) {
		for(i=0; i<turns; i+=j) {
			printf("Collector Thread waiting for worker events...\n");

			dwWaitResult=WaitForMultipleObjects(
				THREADCOUNT,
				ghWorkerEvents,
				TRUE,
				INFINITE);

			switch(dwWaitResult) {
				case WAIT_OBJECT_0:
					WaitForSingleObject(
						ghOutBufferMutex,
						INFINITE);
					for(j=0; (j<THREADCOUNT)&&((j+i)<turns); j++) {
						if(outBuffer.empty()) {
							printf("OHHHHHHHHHHHHHHHHHHHHHOHOHOH\n");
							break;
						}
						printf("Collector Thread reading from buffer, got %c\n", (outBuffer.front()).second);
						desFile.write((outBuffer.front()).first, (outBuffer.front()).second);
						outBuffer.pop_front();
					}
					ReleaseMutex(ghOutBufferMutex);
					break;

				default:
					printf("Wait error (%d)\n", GetLastError());
					return 0;
			}

			SetEvent(ghCollectorEvent);
		}

		desFile.close();
	}
	else
		return 1;

	printf("Collector Thread exiting\n");
	SetEvent(ghDecompressEvent);
	WaitForSingleObject(
		ghFinallyDone,
		INFINITE);

	return 0;
}
