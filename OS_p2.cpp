#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdlib.h>
#include "lzfx.h"
using namespace std;

int compFile(const char * fn) {
    char * memblock;
    char * memblock2;
	int oriSize=0;
    ifstream srcFile(fn, ios::in|ios::binary|ios::ate);
    if(srcFile.is_open()) {
        ifstream::pos_type size=srcFile.tellg();
		oriSize=size;
        unsigned int size2=size*2;
        memblock=new char [size];
		memblock2=new char [size2];
        srcFile.seekg(0, ios::beg);
        srcFile.read(memblock, size);
		
        lzfx_compress((const void*)memblock, size, (void*)memblock2, &size2);
		
        ofstream desFile("CompFile", ios::out|ios::binary);

        if(desFile.is_open())
            desFile.write(memblock2, size2);
        else
            cout<<"Unable to create destination file during compression.\n";
		delete[] memblock;
		delete[] memblock2;
    }
    else cout<<"Unable to open source file during compression.\n";
    return oriSize;
}

void decompFile(const char * fn, int oriSize) {
    char * memblock;
    char * memblock2;
    ifstream srcFile(fn, ios::in|ios::binary|ios::ate);
    if(srcFile.is_open()) {
        ifstream::pos_type size=srcFile.tellg();
        unsigned int size2=oriSize;
        memblock=new char [size];
		memblock2=new char [size2];
        srcFile.seekg(0, ios::beg);
        srcFile.read(memblock, size);

        lzfx_decompress((const void*)memblock, size, (void*)memblock2, &size2);

        ofstream desFile("output", ios::out|ios::binary);
        if(desFile.is_open())
            desFile.write(memblock2, size2);
        else
            cout<<"Unable to create destination file during decompression.\n";
		delete[] memblock;
		delete[] memblock2;
    }
    else cout<<"Unable to open source file during decompression.\n";
    return;
}

int main(int argc, char **argv) {
    int size=compFile("input");
    decompFile("CompFile", size);
	//system("PAUSE");
    return 0;
}
