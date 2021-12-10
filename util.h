#ifndef UTIL_H_
#define UTIL_H_
#include <string>
#include <stdint.h>
#include <vector>
#include "nyufile.h"
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
};
#endif