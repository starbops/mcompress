#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdlib.h>
#include "lzfx.h"
using namespace std;

unsigned int compFile(const char * fn, unsigned int chunkSize) {
    char * memblock;
    char * memblock2;
	unsigned int oriSize=0;
    ifstream srcFile(fn, ios::in|ios::binary|ios::ate);
	ofstream desFile("CompFile", ios::out|ios::binary|ios::app);
    if(srcFile.is_open()&&desFile.is_open()) {
		unsigned int ilen=0;
        ifstream::pos_type totalSize=srcFile.tellg();
		oriSize=(unsigned int)totalSize;
		srcFile.seekg(0, ios::beg);
		ifstream::pos_type current=srcFile.tellg();
		while(current<totalSize) {
			if(((unsigned int)current+chunkSize)<totalSize) {
				ilen=chunkSize;
				memblock=new char [ilen];
			}
			else {
				ilen=(unsigned int)(totalSize-current);
				memblock=new char [ilen];
			}
			current+=ilen;
			srcFile.read(memblock, (ilen));
        
			unsigned int olen=ilen*2;
			memblock2=new char [olen];
		
			lzfx_compress((const void*)memblock, ilen, (void*)memblock2, &olen);
			
		    desFile.write(memblock2, olen);

			delete[] memblock;
			delete[] memblock2;
		}
		cout<<"Compression is done!"<<endl;
    }
    else cout<<"Unable to open source file or create destination file during compression.\n";
    return oriSize;
}

void decompFile(const char * fn, unsigned int oriSize) {
    char * memblock;
    char * memblock2;
    ifstream srcFile(fn, ios::in|ios::binary|ios::ate);
    if(srcFile.is_open()) {
        ifstream::pos_type size=srcFile.tellg();
        unsigned int size2=oriSize*2;
        memblock=new char [(unsigned int)size];
		memblock2=new char [size2];
        srcFile.seekg(0, ios::beg);
        srcFile.read(memblock, size);

        lzfx_decompress((const void*)memblock, (unsigned int)size, (void*)memblock2, &size2);
        ofstream desFile("output", ios::out|ios::binary);
        if(desFile.is_open())
            desFile.write(memblock2, size2);
        else
            cout<<"Unable to create destination file during decompression.\n";
		delete[] memblock;
		delete[] memblock2;
		cout<<"Decompression is done!"<<endl;
    }
    else cout<<"Unable to open source file during decompression.\n";
    return;
}

int main(int argc, char **argv) {
	unsigned int oriSize=0, chunkSize=256;
    oriSize=compFile("input", chunkSize*1024);
    decompFile("CompFile", oriSize);
	system("PAUSE");
    return 0;
}
