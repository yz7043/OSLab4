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
#include <vector>
#include "util.h"
#include "nyufile.h"
using namespace std;
unsigned char reverse(unsigned char);
bool isEmptyName(unsigned char*);
bool isDelName(unsigned char*);
string getName(unsigned char*, bool);
void openFile();
void closeFile();
vector<uint32_t> getAllMatchFiles(const char* name, bool isDel = false);

uint32_t dataStart=0;
uint32_t firstFatStart = 0;
uint32_t sndFatStart = 0;
uint32_t entryPerClus;
size_t fileSize;
// read fat32 disk
const char* fileName;
int fd = -1;
char* fileMap = NULL;
// Task 4,5,6,7
vector<uint32_t> getAllClsts(uint32_t startClst);
unsigned char* getUCName(const char* name);
struct BootEntry* bootEntry;
struct BootEntry* getRootEntry(const char* fileName);
string USAGE_INFO = "Usage: ./nyufile disk <options>\n"
"  -i                     Print the file system information.\n"
"  -l                     List the root directory.\n"
"  -r filename [-s sha1]  Recover a contiguous file.\n"
"  -R filename -s sha1    Recover a possibly non-contiguous file.";
FileHandler* fh;
FSHandler* fsh;

/*
Helper function
*/
void print4B(uint32_t num);

int main(int argc, char** argv){
    int opt; bool recover = false, hasSHA = false;
    char* recoverFile = NULL, * shaStr = NULL;
    fileName = argv[optind];
    bool getInfo = false, getRoot = false;
    bool nonCont = false;
    bootEntry = getRootEntry(fileName);
    // some helpful global value
    dataStart = ((unsigned int)32 + bootEntry->BPB_FATSz32 * bootEntry->BPB_NumFATs) * bootEntry->BPB_BytsPerSec;
    firstFatStart = ((unsigned int)32) * bootEntry->BPB_BytsPerSec;
    sndFatStart = ((unsigned int)32 + bootEntry->BPB_FATSz32) * bootEntry->BPB_BytsPerSec;
    entryPerClus = (uint32_t)bootEntry->BPB_BytsPerSec * (uint32_t)bootEntry->BPB_SecPerClus / sizeof(DirEntry);
    fh = new FileHandler(fileName);
    fsh = new FSHandler(fh);
    fsh->initHandler();
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
                recoverFile = optarg;
                break;
            case 'R':
                recover = true;
                nonCont = true;
                recoverFile = optarg;
                break;
            case 's':
                hasSHA = true;
                shaStr = optarg;
                break;
            default:
                std::cout << USAGE_INFO << endl;
                return -1;
        }
    }
    if(getInfo){
        fsh->printDiskInfo();
    }
    if(getRoot){
        fsh->printRootDir();
    }
    if(recover){
        if(!nonCont){
            if(hasSHA){
                unsigned char* ucShaStr = reinterpret_cast<unsigned char*>(shaStr);
                int res = fsh->recoverConFileSha(recoverFile, ucShaStr);
                if(res == 1){
                    cout << fileName << SUCC_RECOVER_SHA << endl;
                }else if(res == -1){
                    cout << fileName << FILE_NOT_FOUND << endl;
                }
            }else{
                int res = fsh->recoverConFile(recoverFile);
                if(res == 1){
                    cout << recoverFile << SUCC_RECOVER << endl;
                }else if(res == -1){
                    cout << recoverFile << MULTI_FILE_FOUND << endl;
                }else if(res == -2){
                    cout << recoverFile << FILE_NOT_FOUND << endl;
                }
            }
        }else{
            if(hasSHA){
                std::cout << "recover with SHA" << endl;
            }else{
                std::cout << "recover without SHA" << endl;
            }
        }
        
    }
    delete fh; delete fsh;
    delete bootEntry;
}

struct BootEntry* getRootEntry(const char* fileName){
    openFile();
    struct BootEntry* bootEntry = new struct BootEntry();
    memcpy((void*)bootEntry, fileMap, sizeof(BootEntry));
    // close(fd);
    closeFile();
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
    return !(name[0] ^ (unsigned char)0xE5);
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

vector<uint32_t> getAllClsts(uint32_t startClst){
    openFile();
    vector<uint32_t> allClsts;
    allClsts.push_back(startClst);
    uint32_t firstFat = ((unsigned int)32) * bootEntry->BPB_BytsPerSec;
    uint32_t ntClt;
    memcpy((void*)&ntClt, fileMap + firstFat + startClst * FAT_ENTRY_SIZE, sizeof(uint32_t));
    while(ntClt < FAT_EOF){
        allClsts.push_back(ntClt);
        memcpy((void*)&ntClt, fileMap + firstFat + ntClt * FAT_ENTRY_SIZE, sizeof(uint32_t));
    }
    closeFile();
    return allClsts;
}

unsigned char* getUCName(const char* name){
    vector<unsigned char> head;
    vector<unsigned char> ext;
    bool _findDot = false;
    for(int i = 0; i < strlen(name); i++){
        if(name[i] == '.'){
            _findDot = true;
        }else{
            if(_findDot){
                ext.push_back((unsigned int) name[i]);
            }else{
                head.push_back((unsigned int) name[i]);
            }
        }
    }
    unsigned char* ucName = new unsigned char[FAT_NAME_SIZE + FAT_EXT_SIZE];
    for(int i = 0; i < FAT_NAME_SIZE; i++){
        if(i < head.size()){
            ucName[i] = head[i];
        }else{
            ucName[i] = (unsigned char)' ';
        }
    }
    for(int i = 0; i < FAT_EXT_SIZE; i++){
        if(i < ext.size()){
            ucName[i + FAT_NAME_SIZE] = ext[i];
        }else{
            ucName[i + FAT_NAME_SIZE] = (unsigned int) ' ';
        }
    }
    return ucName;
}
/**
Get all files with certain name and return all match dirEntries start point.
Param:
    name: const char* name
    isDel: bool. If this file is an deleted file name
*/
vector<uint32_t> getAllMatchFiles(const char* name, bool isDel){
    vector<uint32_t> allClsts = getAllClsts((uint32_t)2); // get all root clusters
    openFile();
    vector<uint32_t> allFiles;
    
    // 1. Find the entry in root cluster
    unsigned char* ucName = getUCName(name);
    if(isDel) ucName[0] = (unsigned char)(0xE5);
    uint32_t dataStart = ((unsigned int)32 + bootEntry->BPB_FATSz32 * bootEntry->BPB_NumFATs) * bootEntry->BPB_BytsPerSec;
    for(int clst = 0; clst < allClsts.size(); clst++){
        uint32_t entryOffset = 0;
        uint32_t dataConStart = dataStart + bootEntry->BPB_BytsPerSec * ((allClsts[clst] - 2)* bootEntry->BPB_SecPerClus);
        struct DirEntry dirEntry;
        while(entryOffset < entryPerClus){
            memcpy((void*)&dirEntry, fileMap+dataConStart+entryOffset*sizeof(DirEntry), sizeof(DirEntry));
            if(isDelName(dirEntry.DIR_Name)){
                bool matches = true;
                for(int n = 0; n < FAT_NAME_SIZE; n++){
                    if(ucName[n] != dirEntry.DIR_Name[n]){
                        matches = false; break;
                    }
                }
                if(matches){
                    allFiles.push_back(dataConStart+entryOffset*sizeof(DirEntry));
                }
            }
            entryOffset++;
        }
    }
    delete ucName;
    closeFile();
    return allFiles;
}

void openFile(){
    struct stat fileStat;
    int status;
    // read file
    fd = open(fileName, O_RDWR);
    if(fd < 0){
        cerr << "Open file system failed" << endl;
        fileMap = NULL;
        exit(1);
    }
    status = fstat(fd, &fileStat);
    fileSize = fileStat.st_size;
    fileMap = (char*)mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
}
void closeFile(){
    munmap(fileMap, fileSize);
    close(fd);
    fileMap = NULL;
    fd = -1;
    fileSize = 0;
}
