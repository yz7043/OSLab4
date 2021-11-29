#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <cstddef>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <string>
#include "nyufile.h"
using namespace std;
void printSystemInfo(const char* fileName);

int main(int argc, char** argv){
    string fileName = "fat32.disk";
    printSystemInfo(fileName.c_str());
}

void printSystemInfo(const char* fileName){
    int fd;
    struct stat fileStat;
    int status;
    size_t fileSize;
    const char* fileMap;
    struct BootEntry bootEntry;
    // read file
    fd = open(fileName, O_RDONLY);
    status = fstat(fd, &fileStat);
    fileSize = fileStat.st_size;
    fileMap = (const char*)mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    memcpy((void*)&bootEntry, fileMap, sizeof(BootEntry));
    cout << "Number of FATs = " << (int) bootEntry.BPB_NumFATs << endl;
    cout << "Number of bytes per sector = " << bootEntry.BPB_BytsPerSec << endl;
    cout << "Number of sectors per cluster = " << (int) bootEntry.BPB_SecPerClus << endl;
    cout << "Number of reserved sectors = " << bootEntry.BPB_RsvdSecCnt << endl;
    close(fd);
}

