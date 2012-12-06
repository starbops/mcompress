#include <iostream>
#include <fstream>
#include <vector>
#include <windows.h>
#include <stdio.h>
#include "lzfx.h"
using namespace std;

typedef unsigned int UINT;

// MUTEX HANDLER

HANDLE ghInBufferMutex, ghOutBufferMutex;

// EVENT HANDLER

HANDLE *ghDispatcherEvents, *ghWorkerEvents, ghCollectorEvent, ghDecompressEvent, ghFinallyDone;
pair<HANDLE, HANDLE> * ghPairEvents;

// THREAD HANDLER

HANDLE ghDispatcherThread, *ghWorkerThreads, ghCollectorThread;

// GLOBAL RESOURCE

vector< pair< pair<char*, UINT>, UINT> > inBuffer;
vector< pair< pair<char*, UINT>, UINT> > outBuffer;
UINT chunkSize, originalSize, turns, threadCount, lastChunkLength;
char oFileName[50], cFileName[50];

// PROTOTYPE

DWORD WINAPI DispatcherThreadProc(LPVOID);
DWORD WINAPI WorkerThreadProc(LPVOID);
DWORD WINAPI CollectorThreadProc(LPVOID);

void CreateMutexAndEvents(void) {
	UINT i;
	ghDispatcherEvents=new HANDLE [threadCount];
	ghWorkerEvents=new HANDLE [threadCount];
	ghPairEvents=new pair<HANDLE, HANDLE> [threadCount];

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

	for(i=0; i<threadCount; i++) {
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

	for(i=0; i<threadCount; i++) {
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

	for(i=0; i<threadCount; i++)
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
	UINT i;
	DWORD dwThreadId;
	ghWorkerThreads=new HANDLE [threadCount];

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

	for(i=0; i<threadCount; i++) {
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
	UINT i;
	for(i=0; i<threadCount; i++)
		CloseHandle(ghDispatcherEvents[i]);
	for(i=0; i<threadCount; i++)
		CloseHandle(ghWorkerEvents[i]);
	CloseHandle(ghCollectorEvent);
	CloseHandle(ghDecompressEvent);
	CloseHandle(ghFinallyDone);
	delete [] ghDispatcherEvents;
	delete [] ghWorkerEvents;
	delete [] ghPairEvents;
}

void GetUser(void) {

	cout<<"Input file name? ";
	cin>>oFileName;
	cout<<"Output file name? ";
	cin>>cFileName;
	cout<<"Chunk size? ";
	cin>>chunkSize;
	chunkSize*=1024;
	cout<<"# of worker threads? ";
	cin>>threadCount;

	return ;
}

void CalcTurns(void) {
	ifstream f(oFileName, ios::in|ios::ate);
	if(f.is_open())
		originalSize=(UINT)f.tellg();
	turns=(originalSize%chunkSize==0)? (originalSize/chunkSize): (originalSize/chunkSize)+1;
	lastChunkLength=originalSize-chunkSize*(turns-1);

	return ;
}

void Decompression(void) {
	WaitForSingleObject(
		ghDecompressEvent,
		INFINITE);
	ifstream cFile;
	ofstream dFile;
	cFile.open(cFileName, ios::in|ios::binary|ios::ate);
	if(cFile.is_open()) {
		dFile.open("output", ios::out|ios::binary);
		if(dFile.is_open()) {
			char *cMemBlock=NULL, *dMemBlock=NULL;
			UINT cSize=0, dSize=0;

			cSize=(UINT)cFile.tellg();
			cFile.seekg(0, cFile.beg);
			dSize=originalSize;
			cMemBlock=new char [cSize];
			dMemBlock=new char [dSize];
			cFile.read(cMemBlock, cSize);
			lzfx_decompress(cMemBlock, cSize, dMemBlock, &dSize);
			dFile.write(dMemBlock, dSize);

			delete [] cMemBlock;
			delete [] dMemBlock;

			dFile.close();
		}
		cFile.close();
	}

	return ;
}

int main(void) {
	LARGE_INTEGER ticksPerSecond;
	LARGE_INTEGER startTick;
	LARGE_INTEGER endTick;
	double elapsed=0.0;

	GetUser();
	QueryPerformanceFrequency(&ticksPerSecond);
	QueryPerformanceCounter(&startTick);
	CalcTurns();
	CreateMutexAndEvents();
	CreateThreads();

	WaitForSingleObject(
		ghCollectorThread,
		INFINITE);

	CloseEvents();

	QueryPerformanceCounter(&endTick);
	elapsed=((double)(endTick.QuadPart-startTick.QuadPart)/ticksPerSecond.QuadPart);
	printf("Time elapse = %lf (ms)\n", elapsed*1000);

	system("PAUSE");

	return 0;
}

DWORD WINAPI DispatcherThreadProc(LPVOID lpParam) {
	UNREFERENCED_PARAMETER(lpParam);
	char * memBlock=NULL;
	UINT i, j, current=0, end=0, size=0;
	ifstream srcFile;

	srcFile.open(oFileName, ios::in|ios::binary|ios::ate);
	if(srcFile.is_open()) {
		end=(UINT)srcFile.tellg();
		srcFile.seekg(0, srcFile.beg);

		for(i=0; i<turns; i+=j) {
			WaitForSingleObject(
				ghCollectorEvent,
				INFINITE);
			for(j=0; (j<threadCount)&&((j+i)<turns); j++) {
				if((j+i)==(turns-1))
					size=lastChunkLength;
				else
					size=chunkSize;
				memBlock=new char [size];
				srcFile.read(memBlock, size);
				inBuffer.push_back(make_pair(make_pair(memBlock, size), j));
			}
			for(UINT i=0; i<threadCount; i++)
				SetEvent(ghDispatcherEvents[i]);
		}

		srcFile.close();
	}
	else
		return 1;

	Decompression();
	SetEvent(ghFinallyDone);

	return 0;
}

DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
	pair<HANDLE, HANDLE> * it=(pair<HANDLE, HANDLE>*) lpParam;
	HANDLE hDispatcherEvent=it->first;
	HANDLE hWorkerEvent=it->second;

	char * memBlock=NULL, *tmpBlock=NULL;
	UINT cChunkSize=0, oChunkSize=0, order=0;

	while(1) {
		WaitForSingleObject(
			hDispatcherEvent,
			INFINITE);
		WaitForSingleObject(
			ghInBufferMutex,
			INFINITE);
		if(!inBuffer.empty()) {
			tmpBlock=inBuffer.front().first.first;
			oChunkSize=inBuffer.front().first.second;
			cChunkSize=oChunkSize*2;
			order=inBuffer.front().second;
			inBuffer.erase(inBuffer.begin());
			
			ReleaseMutex(ghInBufferMutex);

			memBlock=new char [cChunkSize];
			lzfx_compress(tmpBlock, oChunkSize, memBlock, &cChunkSize);
			delete [] tmpBlock;

			WaitForSingleObject(
				ghOutBufferMutex,
				INFINITE);
			outBuffer.push_back(make_pair(make_pair(memBlock, cChunkSize), order));
			ReleaseMutex(ghOutBufferMutex);
		}
		else
			ReleaseMutex(ghInBufferMutex);
		SetEvent(hWorkerEvent);
	}

	return 0;
}

DWORD WINAPI CollectorThreadProc(LPVOID lpParam) {
	UNREFERENCED_PARAMETER(lpParam);
	UINT i, j, k, end=0;
	ofstream desFile;

	desFile.open(cFileName, ios::out|ios::binary);
	if(desFile.is_open()) {
		for(i=0; i<turns; i+=j) {
			WaitForMultipleObjects(
				threadCount,
				ghWorkerEvents,
				TRUE,
				INFINITE);
			for(j=0; (j<threadCount)&&((j+i)<turns); j++) {
				if(outBuffer.empty())
					break;
				for(k=0; outBuffer[k].second!=j; k++) ;
				desFile.write(outBuffer[k].first.first, outBuffer[k].first.second);
				delete [] outBuffer[k].first.first;
				outBuffer.erase(outBuffer.begin()+k);
			}
			SetEvent(ghCollectorEvent);
		}
		desFile.close();
	}
	else
		return 1;

	SetEvent(ghDecompressEvent);
	WaitForSingleObject(
		ghFinallyDone,
		INFINITE);

	return 0;
}
