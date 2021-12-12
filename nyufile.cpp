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
// read fat32 disk
const char* fileName;
int fd = -1;
char* fileMap = NULL;
struct BootEntry* bootEntry;
FileHandler* fh;
FSHandler* fsh;
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
    fileName = argv[optind];
    bool getInfo = false, getRoot = false;
    bool nonCont = false;
    // some helpful global value
    fh = new FileHandler(fileName);
    fsh = new FSHandler(fh);
    int testDisk = fh->openFile();
    fh->closeFile();
    if(testDisk == -1){
        std::cout << USAGE_INFO << endl;
        return 0;
    }

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
                int res = fsh->recoverDisFileSha(recoverFile, reinterpret_cast<unsigned char*>(shaStr));
                if(res == 1)
                    std::cout << recoverFile << SUCC_RECOVER_SHA << endl;
                else
                    std::cout << recoverFile << FILE_NOT_FOUND << endl;
            }else{
                std::cout << recoverFile << FILE_NOT_FOUND << endl;
            }
        }
        
    }
    if(!getInfo && !getRoot && !recover){
        std::cout << USAGE_INFO << endl;
    }
    delete fh; delete fsh;
}