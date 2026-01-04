#include <stdio.h>
#include <sys/stat.h>
#include "utils.h"

// Windows 需要這個標頭檔才有 _mkdir
#ifdef _WIN32
#include <direct.h>
#endif

// 建立 Host 電腦上的目錄
// 因為 Windows 和 Linux/Mac 的 mkdir 參數不同
void create_host_dir(const char *path) 
{
    struct stat st = {0};
    if (stat(path, &st) == -1) 
    {
        #ifdef _WIN32
            _mkdir(path); // Windows
        #else
            mkdir(path, 0700); // Linux
        #endif
    }
}