#include <iostream>
#include <vector>
#include <list>
#include <windows.h>
#include <stdio.h>
using namespace std;

#define THREADCOUNT 4

typedef unsigned int UINT;

// MUTEX HANDLER

HANDLE ghInBufferMutex, ghOutBufferMutex;

// EVENT HANDLER

HANDLE ghDispatcherEvents[THREADCOUNT], ghWorkerEvents[THREADCOUNT], ghCollectorEvent;
pair<HANDLE, HANDLE> ghPairEvents[THREADCOUNT];

// THREAD HANDLER

HANDLE ghDispatcherThread, ghWorkerThreads[THREADCOUNT], ghCollectorThread;

// GLOBAL RESOURCE

char * data="abcdefghijklmn";
list<char> inBuffer, outBuffer;

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
}

void WriteToBuffer(char * p, UINT size) {
	DWORD dwWaitResult;

	dwWaitResult=WaitForSingleObject(
		ghInBufferMutex,
		INFINITE);

	switch(dwWaitResult) {
		case WAIT_OBJECT_0:
			printf("Dispatcher thread writing to the shared buffer...\n");
			for(UINT i=0; i<size; i++)
				inBuffer.push_back(*(p+i));
			ReleaseMutex(ghInBufferMutex);
			break;
		default:
			printf("WaitForSingleObject failed (%d)\n", GetLastError());
			return ;
	}

	for(int i=0; i<THREADCOUNT; i++)
		SetEvent(ghDispatcherEvents[i]);

	printf("Dispatcher thread writing to the shared buffer is complete.\n");
}

int main(void) {
	DWORD dwWaitResult;

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

	system("PAUSE");

	return 0;
}

DWORD WINAPI DispatcherThreadProc(LPVOID lpParam) {
	UNREFERENCED_PARAMETER(lpParam);
	char * p=data;
	UINT size=0;

	for(; *p!='\0'; p+=size) {
		size=0;
		WaitForSingleObject(
			ghCollectorEvent,
			INFINITE);
		for(char * tmp=p; (size<THREADCOUNT)&&(*tmp!='\0'); tmp++)
			size++;
		WriteToBuffer(p, size);
	}

	printf("Dispatcher thread exiting\n");

	return 0;
}

DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
	pair<HANDLE, HANDLE> * it=(pair<HANDLE, HANDLE>*) lpParam;
	HANDLE hDispatcherEvent=it->first;
	HANDLE hWorkerEvent=it->second;

	DWORD dwWaitResult;

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
				outBuffer.push_back(inBuffer.front());
				inBuffer.pop_front();
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
	UINT end=0;
	for(UINT i=0; data[i]!='\0'; i++)
		end++;
	UINT count=(end%THREADCOUNT==0)? (end/THREADCOUNT): (end/THREADCOUNT)+1;
	DWORD dwWaitResult;

	for(UINT i=0; i<count; i++) {
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
				for(int i=0; i<THREADCOUNT; i++) {
					if(outBuffer.empty()) {
						printf("OHHHHHHHHHHHHHHHHHHHHHOHOHOH\n");
						break;
					}
					printf("Collector Thread reading from buffer, got %c\n", outBuffer.front());
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

	printf("Collector Thread exiting\n");

	return 0;
}
