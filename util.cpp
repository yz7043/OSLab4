#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include "util.h"
/*
Class FileHandler
*/
using namespace std;
void FileHandler::setFileName(const char* fileName){
    FileHandler::fileName = fileName;
}
int FileHandler::openFile(bool rdOnly){
    if(fileName == NULL){
        return -1; // no file
    }
    if(fd >= 0) closeFile();
    struct stat fileStat; int status;
    if(rdOnly)
        fd = open(fileName, O_RDONLY);
    else
        fd = open(fileName, O_RDWR);
    if(fd < 0){
        cerr << "Open file system failed" << endl;
        fileMap = NULL;
        return -1;
    }
    status = fstat(fd, &fileStat);
    fileSize = fileStat.st_size;
    if(rdOnly)
        this->fileMap = (char*)mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    else
        this->fileMap = (char*)mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    return fd;
}
void FileHandler::closeFile(){ 
    munmap(this->fileMap, fileSize);
    close(fd);
    this->fileMap = NULL;
    fd = -1;
    fileSize = 0;
}
FileHandler::FileHandler(const char* fileName){
    fileSize = 0; this->fileName = fileName; fd = -1;
    fileMap = NULL;
}
/*
Class FSHandler
*/
int FSHandler::initHandler(){
    int status = fileH->openFile(true);
    if(status < 0) return -1;
    this->bootEntry = new struct BootEntry();
    memcpy((void*)this->bootEntry, fileH->fileMap, sizeof(BootEntry));
    fileH->closeFile();
    // Update info
    numOfFat = (uint32_t)bootEntry->BPB_NumFATs;
    secPerClus = (uint32_t)bootEntry->BPB_SecPerClus;
    bytePerSec = (uint32_t)bootEntry->BPB_BytsPerSec;
    rsvSecs = (uint32_t)bootEntry->BPB_RsvdSecCnt;
    rsvdAreaSize = (uint32_t)bootEntry->BPB_RsvdSecCnt * bootEntry->BPB_BytsPerSec;
    fatSize = (uint32_t)bootEntry->BPB_FATSz32 * bootEntry->BPB_BytsPerSec;
    dirEntPerClst = bytePerSec * secPerClus / sizeof(DirEntry);
    // update fat start info
    for(int i = 0; i < numOfFat; i++){
        fatStPoint.push_back(rsvdAreaSize + fatSize * i);
    }
    dataSt = *fatStPoint.rbegin() + fatSize;
    return 1;
}

FSHandler::FSHandler(FileHandler* fileH){
    this->fileH = fileH; dataStart = 0;
    numOfFat = 0; secPerClus = 0;
    bytePerSec = 0; bootEntry = NULL;
    rsvSecs=0;
}

FSHandler::~FSHandler(){
    fileH = NULL;
    if(bootEntry != NULL) delete bootEntry;
    bootEntry = NULL;
}

void FSHandler::printDiskInfo(){
    cout << "Number of FATs = " << numOfFat << endl;
    cout << "Number of bytes per sector = " << bytePerSec << endl;
    cout << "Number of sectors per cluster = " << secPerClus << endl;
    cout << "Number of reserved sectors = " << rsvSecs << endl;
}

vector<uint32_t> FSHandler::getAllClsts(uint32_t startClst){
    fileH->openFile(true);
    vector<uint32_t> allClsts;
    allClsts.push_back(startClst);
    uint32_t ntClt;
    memcpy((void*)&ntClt, fileH->fileMap + fatStPoint[0] + startClst * FAT_ENTRY_SIZE, sizeof(uint32_t));
    while(ntClt < FAT_EOF){
        allClsts.push_back(ntClt);
        memcpy((void*)&ntClt,  fileH->fileMap + fatStPoint[0] + ntClt * FAT_ENTRY_SIZE, sizeof(uint32_t));
    }
    fileH->closeFile();
    return allClsts;
}

void FSHandler::printRootDir(){
    vector<uint32_t> allClsts = getAllClsts(bootEntry->BPB_RootClus);
    this->fileH->openFile(true);
    int totalEntries = 0;
    for(int i = 0; i < allClsts.size(); i++){
        int entryOffset = 0;
        while(entryOffset < dirEntPerClst){
            struct DirEntry dirEntry;
            memcpy((void*)&dirEntry, fileH->fileMap + dataSt + entryOffset  * sizeof(DirEntry), sizeof(DirEntry));
            if(!isDelName(dirEntry.DIR_Name) && !isEmptyName(dirEntry.DIR_Name)){
                string name = getName(dirEntry.DIR_Name, dirEntry.DIR_Attr & DIR_MASK);
                uint32_t cls = dirEntry.DIR_FstClusHI << 16 | dirEntry.DIR_FstClusLO;
                cout << name + " (size = " << dirEntry.DIR_FileSize << ", starting cluster = "<< cls <<")" << endl;
                totalEntries++;
            }
            entryOffset++;
        }
    }
    fileH->closeFile();
}

bool FSHandler::isDelName(unsigned char* name){
    return !(name[0] ^ (unsigned char)DEL_FILE_HEAD );
}

bool FSHandler::isEmptyName(unsigned char* name){
    for(int i = 0; i < FAT_NAME_SIZE + FAT_EXT_SIZE; i++){
        if(name[i] != 0){
            return false;
        }
    }
    return true;
}

string FSHandler::getName(unsigned char* name, bool isDir){
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