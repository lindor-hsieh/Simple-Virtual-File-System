#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "fs.h"
#include "commands.h"
#include "editor.h"
#include "utils.h"

// 平台相容性設定
#ifdef _WIN32
    #include <conio.h>    // _getch()
    #include <windows.h>  // Console API
    #include <io.h>       // _dup, _fileno
    #define STDOUT_FILENO 1

    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
#else
    #include <unistd.h>   // read, write, dup2
    #include <termios.h>  // 終端機設定
#endif

// History
#define HIST_MAX 10 
#define CMD_LEN 256
char history[HIST_MAX][CMD_LEN]; 
int h_cnt = 0;           // 目前History的數量

// 取代 scanf/fgets,實作一個支援「方向鍵」與「歷史紀錄」的 Line Editor
int get_input(char *buf) 
{
    int len = 0; 
    int index = 0;  // 目前游標位置
    int h_idx = h_cnt; // 歷史紀錄索引 (預設指到最新)
    memset(buf, 0, CMD_LEN); 

    while(1) 
    {
        int c;
        #ifdef _WIN32
            c = _getch(); // [Windows] 直接讀取按鍵 (不需按 Enter)
            
            // 偵測特殊鍵
            if (c == 0 || c == 224) 
            {
                int arrow = _getch(); // 讀取第二個碼
                switch(arrow) 
                {
                    case 72: // 上箭
                        if(h_cnt > 0 && h_idx > 0) 
                        {
                            h_idx--; // 往前找歷史紀錄
                            
                            while(index > 0) 
                            { 
                                printf("\b"); index--; // 游標倒退
                            }
                            for(int i=0; i<len; i++) printf(" "); 
                            for(int i=0; i<len; i++) printf("\b"); 
                            
                            strcpy(buf, history[h_idx]); len = strlen(buf); index = len; printf("%s", buf);
                        }
                        break;
                    case 80: // 下箭
                        if(h_idx < h_cnt) 
                        {
                            h_idx++;

                            while(index > 0) 
                            { 
                                printf("\b"); index--; 
                            }
                            for(int i=0; i<len; i++) printf(" ");
                            for(int i=0; i<len; i++) printf("\b");
                            
                            if(h_idx < h_cnt) 
                            { 
                                strcpy(buf, history[h_idx]); len = strlen(buf); 
                            } 
                            else 
                            { 
                                buf[0] = 0; len = 0; 
                            }
                            index = len; printf("%s", buf);
                        }
                        break;
                    case 75: // 左箭
                        if (index > 0) 
                        { 
                            index--; printf("\b"); // 移動游標但不刪除
                        } 
                        break;
                    case 77: // 右箭
                        if (index < len) 
                        { 
                            printf("%c", buf[index]); index++; // 重印字元以移動游標
                        } 
                        break;
                    case 83: // Delete 鍵
                        if (index < len) 
                        {
                            // 把後面的字元往前補
                            memmove(&buf[index], &buf[index+1], len - index); len--;
                            // 重繪後面的字串
                            printf("%s ", &buf[index]); 
                            // 游標歸位
                            for(int i=0; i < (len - index) + 1; i++) printf("\b");
                        } 
                        break;
                }
                continue;
            }
        #else
            c = getchar(); // [Linux] 
        #endif

        // Enter 處理
        if(c == '\n' || c == '\r') 
        { 
            buf[len] = 0; printf("\n"); break; 
        }
        // Backspace 處理
        else if(c == 127 || c == 8) 
        {
            if(index > 0) 
            {
                // 刪除前一個字元
                memmove(&buf[index-1], &buf[index], len - index);
                index--; len--; buf[len] = 0;
                // 更新畫面：倒退 -> 重印 -> 補空白 -> 倒退
                printf("\b%s ", &buf[index]);
                for(int i=0; i < (len - index) + 1; i++) printf("\b");
            }
        }
        // 一般文字輸入
        else if(!iscntrl(c) && len < CMD_LEN - 1) 
        {
            // 插入模式：把游標後的字元往後搬
            memmove(&buf[index+1], &buf[index], len - index);
            buf[index] = c; len++;
            // 印出新字元及後面的字串
            printf("%c%s", c, &buf[index+1]);
            // 游標歸位
            for(int i=0; i < (len - index - 1); i++) printf("\b");
            index++;
        }
    }

    // 儲存到歷史紀錄
    if(len > 0) 
    {
        if(h_cnt == 0 || strcmp(history[h_cnt-1], buf) != 0) 
        {
            // FIFO
            if(h_cnt >= HIST_MAX) 
            { 
                for(int i = 1; i < HIST_MAX; i++) strcpy(history[i-1], history[i]); h_cnt--; 
            }
            strcpy(history[h_cnt++], buf);
        }
    }
    return len;
}

int main() 
{
    int ch, sz; char input[CMD_LEN]; char *cmd, *a1, *a2; char tmp_buf[32];
    
    setvbuf(stdout, NULL, _IONBF, 0); 

    // 啟用 ANSI 顏色
    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    #endif

    printf("1. Load\n2. New Partition\nOption: "); 
    fgets(tmp_buf, sizeof(tmp_buf), stdin); ch = atoi(tmp_buf);
    if(ch == 1) init_fs(0, 1); 
    else 
    { 
        printf("Size (e.g., 2048000): "); fgets(tmp_buf, sizeof(tmp_buf), stdin); sz = atoi(tmp_buf);
        init_fs(sz, 0); 
    }

    while(1) 
    {
        // 顯示 Prompt
        printf("%s $ ", current_path);
        
        if(get_input(input) == 0) continue;

        // 重導向處理
        char *redir = strchr(input, '>'); 
        char *rfile = NULL; 
        int is_redirecting = 0;
        char tmpf[] = "temp_output.dat"; 

        #ifndef _WIN32
        int sav_out = -1; // Linux 用來儲存原本的 stdout
        #endif

        if(redir) 
        {
            // 解析目標檔名
            *redir = 0; // 將 '>' 替換為字串結束符,切斷指令
            rfile = redir + 1; while(*rfile == ' ') rfile++; // 跳過空白
            rfile[strcspn(rfile, "\n")] = 0; // 去除換行
            
            fflush(stdout); 
            
            // 將 stdout 重導向到暫存檔 "temp_output.dat"
            // 這樣所有 cmd_xxx 的 printf 都會寫到這個檔案裡
            #ifdef _WIN32
                if (freopen(tmpf, "w", stdout) != NULL) 
                {
                    is_redirecting = 1;
                }
            #else
                // [Linux 解法] 使用 dup2
                sav_out = dup(STDOUT_FILENO);
                FILE *f = fopen(tmpf, "w");
                if(f) 
                { 
                    dup2(fileno(f), STDOUT_FILENO); fclose(f); is_redirecting = 1; 
                }
            #endif
        }

        // 切割字串 (Command, Arg1, Arg2)
        cmd = strtok(input, " "); a1 = strtok(NULL, " "); a2 = strtok(NULL, " ");
        
        if(!cmd) 
        { 
            // 如果有開啟重導向,必須還原 stdout,否則terminal will crash
            if(is_redirecting) 
            {
                #ifdef _WIN32
                freopen("CON", "w", stdout);
                #else
                dup2(sav_out, STDOUT_FILENO); close(sav_out);
                #endif
            }
            continue; 
        }

        if(strcmp(cmd, "ls") == 0) 
        {
            if(a1 && strcmp(a1, "-l") == 0) cmd_ll(); 
            else cmd_ls();
        }
        else if(strcmp(cmd, "ll") == 0)   cmd_ll();
        else if(strcmp(cmd, "mkdir") == 0 && a1) cmd_mkdir(a1);
        else if(strcmp(cmd, "cd") == 0 && a1)    cmd_cd(a1);
        else if(strcmp(cmd, "pwd") == 0)         cmd_pwd();
        else if(strcmp(cmd, "touch") == 0 && a1) cmd_touch(a1);
        else if(strcmp(cmd, "rmdir") == 0 && a1) cmd_rmdir(a1);
        else if(strcmp(cmd, "rm") == 0 && a1)    cmd_rm(a1);
        else if(strcmp(cmd, "cp") == 0 && a1 && a2) cmd_cp(a1, a2);
        else if(strcmp(cmd, "mv") == 0 && a1 && a2) cmd_mv(a1, a2);
        else if(strcmp(cmd, "cat") == 0 && a1)   cmd_cat(a1);
        else if(strcmp(cmd, "put") == 0 && a1)   cmd_put(a1);
        else if(strcmp(cmd, "get") == 0 && a1)   cmd_get(a1);
        else if(strcmp(cmd, "append") == 0 && a1 && a2) cmd_append(a1, a2);
        else if(strcmp(cmd, "nano") == 0 && a1)  cmd_nano(a1);
        else if(strcmp(cmd, "grep") == 0 && a1 && a2) cmd_grep(a1, a2);
        else if(strcmp(cmd, "tree") == 0)        cmd_tree();
        else if(strcmp(cmd, "stat") == 0 && a1)  cmd_stat(a1);
        else if(strcmp(cmd, "find") == 0 && a1)  cmd_find(a1);
        else if(strcmp(cmd, "encrypt") == 0 && a1 && a2) cmd_encrypt(a1, a2);
        else if(strcmp(cmd, "decrypt") == 0 && a1 && a2) cmd_encrypt(a1, a2); // 解密其實就是再加密一次
        else if(strcmp(cmd, "chmod") == 0 && a1 && a2) cmd_chmod(a1, a2);
        else if(strcmp(cmd, "status") == 0)      cmd_status();
        else if(strcmp(cmd, "diskmap") == 0)     cmd_diskmap();
        else if(strcmp(cmd, "hexdump") == 0 && a1) cmd_hexdump(a1);
        else if(strcmp(cmd, "run") == 0 && a1)   cmd_run(a1);
        else if(strcmp(cmd, "defrag") == 0)      defrag_system();
        else if(strcmp(cmd, "help") == 0)        cmd_help();
        else if(strcmp(cmd, "exit") == 0) 
        { 
            save_fs("my_fs.dump"); break; 
        } 
        else printf("Unknown command: %s\n", cmd);

        if(is_redirecting) 
        {
            fflush(stdout); 

            // 1. 還原 stdout 到控制台
            #ifdef _WIN32
                freopen("CON", "w", stdout);
            #else
                dup2(sav_out, STDOUT_FILENO);
                close(sav_out);
            #endif
            
            // 2. 將暫存檔內容匯入到 VFS
            // 先輸出到 Host 實體檔案,再用 put 的邏輯吸進來
            import_host_file(tmpf, rfile); 
            
            remove(tmpf); 
            printf("Redirected to '%s'\n", rfile);
        }
    }
    return 0;
}