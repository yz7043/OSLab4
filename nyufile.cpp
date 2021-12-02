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
#include <bitset> // helper library
#include <arpa/inet.h> // helper
#include <stdint.h>
#include "nyufile.h"
using namespace std;
void printSystemInfo(const struct BootEntry*);
void printRootDir(const struct BootEntry*, const char*);
unsigned char reverse(unsigned char);
bool isEmptyName(unsigned char*);
bool isDelName(unsigned char*);
string getName(unsigned char*, bool);
struct BootEntry* getRootEntry(const char* fileName);
string USAGE_INFO = "Usage: ./nyufile disk <options>\n"
"  -i                     Print the file system information.\n"
"  -l                     List the root directory.\n"
"  -r filename [-s sha1]  Recover a contiguous file.\n"
"  -R filename -s sha1    Recover a possibly non-contiguous file.";
/*
Helper function
*/
void print4B(uint32_t num);


int main(int argc, char** argv){
    int opt; bool recover = false, hasSHA = false;
    char* recoverFile = NULL, * shaStr = NULL;
    const char* fileName = argv[optind];
    bool getInfo = false, getRoot = false;
    struct BootEntry* bootEntry = getRootEntry(fileName);
    while((opt = getopt(argc, argv, "ilr:R:s:")) != -1){
        switch(opt){
            case 'i':
                getInfo = true;
                break;
            case 'l':
                getRoot = true;
                break;
            case 'r':
                recover = true;
                recoverFile = argv[optind];
                break;
            case 'R':
                recover = true;
                recoverFile = argv[optind];
                break;
            case 's':
                hasSHA = true;
                shaStr = argv[optind];
                break;
            default:
                cout << USAGE_INFO << endl;
                return -1;
        }
    }
    if(getInfo){
        printSystemInfo(bootEntry);
    }
    if(getRoot){
        printRootDir(bootEntry, fileName);
    }
    if(recover){
        if(hasSHA){
            cout << "recover with SHA" << endl;
        }else{
            cout << "recover without SHA" << endl;
        }
    }
}

void printSystemInfo(const struct BootEntry* bootEntry){
    cout << "Number of FATs = " << (int) bootEntry->BPB_NumFATs << endl;
    cout << "Number of bytes per sector = " << bootEntry->BPB_BytsPerSec << endl;
    cout << "Number of sectors per cluster = " << (int) bootEntry->BPB_SecPerClus << endl;
    cout << "Number of reserved sectors = " << bootEntry->BPB_RsvdSecCnt << endl;

}

void printRootDir(const struct BootEntry* bootEntry, const char* fileName){
    uint32_t dataStart = ((unsigned int)32 + bootEntry->BPB_FATSz32 * bootEntry->BPB_NumFATs) * bootEntry->BPB_BytsPerSec;
    uint32_t firstFat = ((unsigned int)32) * bootEntry->BPB_BytsPerSec;
    // read disk
    int fd; struct stat fileStat;
    int status; size_t fileSize;
    const char* fileMap;
    fd = open(fileName, O_RDONLY);
    if(fd < 0){
        cerr << "Open file system failed" << endl;
        exit(1);
    }
    status = fstat(fd, &fileStat);
    fileSize = fileStat.st_size;
    fileMap = (const char*)mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    // read Fat
    uint32_t ntClt;
    int totalEntires = 0;
    memcpy((void*)&ntClt, fileMap + firstFat + 2 * FAT_ENTRY_SIZE, sizeof(uint32_t));
    uint32_t entryPerClus = (uint32_t)bootEntry->BPB_BytsPerSec * (uint32_t)bootEntry->BPB_SecPerClus / sizeof(DirEntry);
    int entryOffset = 0;
    while(entryOffset < entryPerClus){
        struct DirEntry dirEntry;
        memcpy((void*)&dirEntry, fileMap + dataStart + entryOffset  * sizeof(DirEntry), sizeof(DirEntry));
        if(!isDelName(dirEntry.DIR_Name) & !isEmptyName(dirEntry.DIR_Name)){
            string name = getName(dirEntry.DIR_Name, dirEntry.DIR_Attr & DIR_MASK);
            uint32_t cls = dirEntry.DIR_FstClusHI << 16 | dirEntry.DIR_FstClusLO;
            cout << name + " (size = " << dirEntry.DIR_FileSize << ", starting cluster = "<< cls <<")" << endl;
            totalEntires++;
        }
        entryOffset++;
    }
    // new start: 32 reserve sec + 2 fat sec + (cluster number - 2) 
    uint32_t dataConStart = 0;
    while(ntClt < FAT_EOF){
        dataConStart = ((unsigned int)32 + 
            bootEntry->BPB_FATSz32 * bootEntry->BPB_NumFATs + 
            (ntClt - 2) * bootEntry->BPB_SecPerClus
            ) * bootEntry->BPB_BytsPerSec;
        entryOffset = 0;
        while(entryOffset < entryPerClus){
            struct DirEntry dirEntry;
            memcpy((void*)&dirEntry, fileMap + dataConStart + entryOffset  * sizeof(DirEntry), sizeof(DirEntry));
            if(!isDelName(dirEntry.DIR_Name) & !isEmptyName(dirEntry.DIR_Name)){
                string name = getName(dirEntry.DIR_Name, dirEntry.DIR_Attr & DIR_MASK);
                uint32_t cls = dirEntry.DIR_FstClusHI << 16 | dirEntry.DIR_FstClusLO;
                cout << name + " (size = " << dirEntry.DIR_FileSize << ", starting cluster = "<< cls <<")" << endl;
                totalEntires++;
            }
            entryOffset++;
        }
        // update ntClt
        memcpy((void*)&ntClt, fileMap + firstFat + ntClt * FAT_ENTRY_SIZE, sizeof(uint32_t));
        
    }
    cout << "Total number of entries = " << totalEntires << endl;

    //close file
    close(fd);
}

struct BootEntry* getRootEntry(const char* fileName){
    int fd;
    struct stat fileStat;
    int status;
    size_t fileSize;
    const char* fileMap;
    struct BootEntry* bootEntry = new struct BootEntry();
    // read file
    fd = open(fileName, O_RDONLY);
    if(fd < 0){
        cerr << "Open file system failed" << endl;
        exit(1);
    }
    status = fstat(fd, &fileStat);
    fileSize = fileStat.st_size;
    fileMap = (const char*)mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    memcpy((void*)bootEntry, fileMap, sizeof(BootEntry));
    close(fd);
    return bootEntry;
}

bool isEmptyName(unsigned char* name){
    for(int i = 0; i < FAT_NAME_SIZE + FAT_EXT_SIZE; i++){
        if(name[i] != 0){
            return false;
        }
    }
    return true;
}

bool isDelName(unsigned char* name){
    return name[0] == 0xE && name[1] == 0x5;
}
string getName(unsigned char* name, bool isDir){
    // TODO: distinguish EMPTY and DIR ?
    string n = "", ext = ""; 
    for(int i = 0; i < FAT_EXT_SIZE + FAT_NAME_SIZE; i++){
        if(i < FAT_NAME_SIZE){
            if(name[i] != (unsigned char)' '){
                n += (char)name[i];
            }else{
                continue;
            }
        }else{
            if(isDir){
                break;
            }
            if(name[i] != (unsigned char)' '){
                ext += (char)name[i];
            }else{
                continue;
            }
        }
    }
    if(isDir){return n+"/";}
    else{
        return ext == "" ? n : n + "." + ext;
    }
}