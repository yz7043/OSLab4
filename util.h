#ifndef UTIL_H_
#define UTIL_H_
#include <string>
#include <stdint.h>
#include <vector>
#include "nyufile.h"
class DelFileInfo{
public:
    uint32_t fatStart;
    uint32_t fileSize;
    uint32_t entryStart; // reletaive to dataSt.
    DelFileInfo(uint32_t fat, uint32_t size, uint32_t ent){
        fatStart = fat; fileSize = size; entryStart = ent;
    }
};
class FileHandler{
public:
    size_t fileSize;
    const char* fileName;

    int fd;
    char* fileMap;
    void setFileName(const char*);
    int openFile(bool rdOnly=true);
    void closeFile();
    FileHandler(const char*);
};

class FSHandler{
public:
    FileHandler* fileH;
    uint32_t dataStart;
    std::vector<uint32_t> fatStPoint;
    uint32_t numOfFat;
    uint32_t secPerClus;
    uint32_t bytePerSec;
    uint32_t rsvSecs;
    uint32_t fatSize;
    uint32_t rsvdAreaSize;
    uint32_t dataSt;
    uint32_t dirEntPerClst;
    struct BootEntry* bootEntry;
    FSHandler(FileHandler*);
    ~FSHandler();
    int initHandler();
    void printDiskInfo();
    std::vector<uint32_t> getAllClsts(uint32_t);
    void printRootDir();
    bool isDelName(unsigned char*);
    bool isEmptyName(unsigned char*);
    std::string getName(unsigned char*, bool);
    int recoverConFile(const char*);
    int recoverConFileSha(const char*, const char*);
    std::vector<DelFileInfo> getAllDelFiles(const char*);
    unsigned char* getUCName(const char*);
    uint32_t getClstFromLoHi(unsigned short hi, unsigned short lo);

};
#define FILE_NOT_FOUND ": file not found"
#define MULTI_FILE_FOUND ": multiple candidates found"

#endif