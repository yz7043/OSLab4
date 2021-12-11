#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <openssl/sha.h>
#include <stdio.h>
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
        this->fileMap = (char*)mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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
    // dataStart = (uint32_t)bootEntry->BPB_RsvdSecCnt * bytePerSec + numOfFat * fatSize * bytePerSec;
    // update fat start info
    for(int i = 0; i < numOfFat; i++){
        fatStPoint.push_back(rsvdAreaSize + fatSize * i);
    }
    dataSt = *fatStPoint.rbegin() + fatSize;
    return 1;
}

FSHandler::FSHandler(FileHandler* fileH){
    this->fileH = fileH;
    //  dataStart = 0;
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

int FSHandler::recoverConFile(const char* name){
    vector<DelFileInfo> allPosFiles = getAllDelFiles(name); 
    if(allPosFiles.size() > 1){
        return -1;
    }else if(allPosFiles.empty()){
        return -2;
    }
    fileH->openFile(false);
    DelFileInfo fileInfo = *allPosFiles.begin();
    // uint32_t startCls = getClstFromLoHi(fileInfo->DIR_FstClusHI, fileInfo->DIR_FstClusLO);
    uint32_t numClsts = fileInfo.fileSize / (bytePerSec * secPerClus) + (fileInfo.fileSize % (bytePerSec * secPerClus) != 0);
    // get the root entry
    DirEntry tempEnt;
    memcpy((void*)&tempEnt, fileH->fileMap+dataSt+fileInfo.entryStart, sizeof(DirEntry));
    // write name first
    tempEnt.DIR_Name[0] = (unsigned char) name[0];
    memcpy((void*)(fileH->fileMap+dataSt+fileInfo.entryStart), (void *)tempEnt.DIR_Name, FAT_FILE_NAME);
    uint32_t curClst = fileInfo.fatStart;
    uint32_t fatContent;
    for(uint32_t i = 0; i < numClsts; i++){
        if(i == numClsts - 1){
            fatContent = FAT_EOF;
        }else{
            fatContent = curClst + 1;
        }
        // cout << "Which Clster: " << std::hex << curClst << ", To write: "<< fatContent << endl;
        for(uint32_t i = 0; i < numOfFat; i++){
            uint32_t fatSt = fatStPoint[i];
            memcpy((void*)(fileH->fileMap+fatSt+curClst*FAT_ENTRY_SIZE), &fatContent, sizeof(fatContent));
        }
        curClst++;
    }
    fileH->closeFile();
    return 1;
}

int FSHandler::recoverConFileSha(const char* name, const unsigned char* shaStr){
    vector<DelFileInfo> allPosFiles = getAllDelFiles(name); 
    if(allPosFiles.empty()) return -1;
    fileH->openFile(false);
    bool findSHA = false;
    for(vector<DelFileInfo>::iterator it = allPosFiles.begin(); it != allPosFiles.end(); it++){
        unsigned char* fileCont = new unsigned char[it->fileSize];
        getConFileContent(fileH->fileMap, fileCont, *it);
        unsigned char shaHash[SHA_DIGEST_LENGTH];
        unsigned char fileShaStr[SHA_DIGEST_LENGTH*2];
        SHA1(reinterpret_cast<unsigned char*>(fileCont), it->fileSize, shaHash);
        for (int i=0; i < SHA_DIGEST_LENGTH; i++) {
            sprintf((char*)&(fileShaStr[i*2]), "%02x", shaHash[i]);
        }
        bool isSame = true;
        for(int i = 0; i < SHA_DIGEST_LENGTH * 2; i++){
            // cout << (char)fileShaStr[i];
            if(fileShaStr[i] != shaStr[i]){
                isSame = false; break;
            }
        }
        if(isSame){
            DelFileInfo& fileInfo = *it;
            findSHA = true;
            uint32_t numClsts = fileInfo.fileSize / (bytePerSec * secPerClus) + (fileInfo.fileSize % (bytePerSec * secPerClus) != 0);
            // recover root dir
            DirEntry tempEnt;
            memcpy((void*)&tempEnt, fileH->fileMap+dataSt+fileInfo.entryStart, sizeof(DirEntry));
            tempEnt.DIR_Name[0] = (unsigned char) name[0];
            memcpy((void*)(fileH->fileMap+dataSt+fileInfo.entryStart), (void *)tempEnt.DIR_Name, FAT_FILE_NAME);
            // recover fats table
            uint32_t curClst = fileInfo.fatStart;
            uint32_t fatContent;
            for(uint32_t i = 0; i < numClsts; i++){
                if(i == numClsts - 1){
                    fatContent = FAT_EOF;
                }else{
                    fatContent = curClst + 1;
                }
                for(uint32_t i = 0; i < numOfFat; i++){
                    uint32_t fatSt = fatStPoint[i];
                    memcpy((void*)(fileH->fileMap+fatSt+curClst*FAT_ENTRY_SIZE), &fatContent, sizeof(fatContent));
                }
                curClst++;
            }
        }
        delete fileCont;
    }
    fileH->closeFile();
    if (!findSHA) return -1;
    return 1;
}

vector<DelFileInfo> FSHandler::getAllDelFiles(const char* name){
    /**
     * @brief: Take a file name and return all possible deleted file name start with 0xE5
     * @return: (start block, file size)
     */
    vector<DelFileInfo> delFilesSt;
    unsigned char* fsName = getUCName(name);
    vector<uint32_t> allClsts = getAllClsts(bootEntry->BPB_RootClus);
    fileH->openFile();
    for(int i = 0; i < allClsts.size(); i++){
        int entryOffset = 0;
        while(entryOffset < dirEntPerClst){
            struct DirEntry dirEntry;
            memcpy((void*)&dirEntry, fileH->fileMap + dataSt + entryOffset  * sizeof(DirEntry), sizeof(DirEntry));
            if(isDelName(dirEntry.DIR_Name)){
                string name = getName(dirEntry.DIR_Name, dirEntry.DIR_Attr & DIR_MASK);
                uint32_t cls = getClstFromLoHi(dirEntry.DIR_FstClusHI, dirEntry.DIR_FstClusLO);
                DelFileInfo tempDel = DelFileInfo(cls, dirEntry.DIR_FileSize, entryOffset);
                delFilesSt.push_back(tempDel);
            }
            entryOffset++;
        }
    }
    fileH->closeFile();
    delete fsName;
    return delFilesSt;
}

unsigned char* FSHandler::getUCName(const char* name){
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

uint32_t FSHandler::getClstFromLoHi(unsigned short hi, unsigned short lo){
    return (uint32_t)hi << 16 | (uint32_t)lo;
}

void FSHandler::getConFileContent(const char* fMap, unsigned char* des, const DelFileInfo& fileInfo){
    uint32_t numOfClsts = fileInfo.fileSize / (bytePerSec * secPerClus) + (fileInfo.fileSize % (bytePerSec * secPerClus) != 0);
    // First get where is the file
    uint32_t curClstSt; uint32_t bytePerCls = bytePerSec * secPerClus;
    uint32_t rtClst = (uint32_t)bootEntry->BPB_RootClus;
    for(uint32_t i = 0; i < numOfClsts; i++){
        curClstSt = dataSt+ (fileInfo.fatStart - rtClst + i) * bytePerSec * secPerClus;
        if((i+1) * bytePerCls < fileInfo.fileSize){
            memcpy((void*)(des+i*bytePerCls), fMap+curClstSt, bytePerCls);
        }else{
            memcpy((void*)(des+i*bytePerCls), fMap+curClstSt, fileInfo.fileSize - i * bytePerCls);
        }
    }
}
