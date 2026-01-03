#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "fs.h"
#include "commands.h"
#include "editor.h"
#include "utils.h"

// --- 平台相容性設定 ---
#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #include <io.h>
    #define STDOUT_FILENO 1
    
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
#else
    #include <unistd.h>
    #include <termios.h>
#endif

#define HIST_MAX 10
#define CMD_LEN 256
char history[HIST_MAX][CMD_LEN];
int h_cnt = 0;

int get_input(char *buf) 
{
    int len = 0; int index = 0; int h_idx = h_cnt;
    memset(buf, 0, CMD_LEN);

    while(1) 
    {
        int c;
        #ifdef _WIN32
            c = _getch();
            if (c == 0 || c == 224) 
            {
                int arrow = _getch();
                switch(arrow) 
                {
                    case 72: // UP
                        if(h_cnt > 0 && h_idx > 0) 
                        {
                            h_idx--;
                            while(index > 0) 
                            { 
                                printf("\b"); index--; 
                            }
                            for(int i=0; i<len; i++) printf(" ");
                            for(int i=0; i<len; i++) printf("\b");
                            strcpy(buf, history[h_idx]); len = strlen(buf); index = len; printf("%s", buf);
                        }
                        break;
                    case 80: // DOWN
                        if(h_idx < h_cnt) 
                        {
                            h_idx++;
                            while(index > 0) 
                            { 
                                printf("\b"); index--; 
                            }
                            for(int i=0; i<len; i++) printf(" ");
                            for(int i=0; i<len; i++) printf("\b");
                            if(h_idx < h_cnt) { strcpy(buf, history[h_idx]); len = strlen(buf); } 
                            else 
                            { 
                                buf[0] = 0; len = 0; 
                            }
                            index = len; printf("%s", buf);
                        }
                        break;
                    case 75: // LEFT
                        if (index > 0) 
                        { 
                            index--; printf("\b"); 
                        } 
                        break;
                    case 77: // RIGHT
                        if (index < len) 
                        { 
                            printf("%c", buf[index]); index++; 
                        } 
                        break;
                    case 83: // DEL
                        if (index < len) 
                        {
                            memmove(&buf[index], &buf[index+1], len - index); len--;
                            printf("%s ", &buf[index]); 
                            for(int i=0; i < (len - index) + 1; i++) printf("\b");
                        } 
                        break;
                }
                continue;
            }
        #else
            c = getchar();
        #endif

        if(c == '\n' || c == '\r') 
        { 
            buf[len] = 0; printf("\n"); break; 
        }
        else if(c == 127 || c == 8) 
        {
            if(index > 0) 
            {
                memmove(&buf[index-1], &buf[index], len - index);
                index--; len--; buf[len] = 0;
                printf("\b%s ", &buf[index]);
                for(int i=0; i < (len - index) + 1; i++) printf("\b");
            }
        }
        else if(!iscntrl(c) && len < CMD_LEN - 1) 
        {
            memmove(&buf[index+1], &buf[index], len - index);
            buf[index] = c; len++;
            printf("%c%s", c, &buf[index+1]);
            for(int i=0; i < (len - index - 1); i++) printf("\b");
            index++;
        }
    }

    if(len > 0) 
    {
        if(h_cnt == 0 || strcmp(history[h_cnt-1], buf) != 0) 
        {
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
        printf("%s $ ", current_path);
        if(get_input(input) == 0) continue;

        char *redir = strchr(input, '>'); 
        char *rfile = NULL; 
        int is_redirecting = 0;
        char tmpf[] = "temp_output.dat";

        #ifndef _WIN32
        int sav_out = -1; // Linux 仍使用 dup2
        #endif

        if(redir) 
        {
            *redir = 0; rfile = redir + 1; while(*rfile == ' ') rfile++;
            rfile[strcspn(rfile, "\n")] = 0;
            
            fflush(stdout);
            
            #ifdef _WIN32
                if (freopen(tmpf, "w", stdout) != NULL) 
                {
                    is_redirecting = 1;
                }
            #else
                // [Linux 解法] 
                sav_out = dup(STDOUT_FILENO);
                FILE *f = fopen(tmpf, "w");
                if(f) { dup2(fileno(f), STDOUT_FILENO); fclose(f); is_redirecting = 1; }
            #endif
        }

        cmd = strtok(input, " "); a1 = strtok(NULL, " "); a2 = strtok(NULL, " ");
        
        if(!cmd) 
        { 
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
            // 檢查有沒有 -l 參數
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
        else if(strcmp(cmd, "exit") == 0) { save_fs("my_fs.dump"); break; }
        else printf("Unknown command: %s\n", cmd);

        if(is_redirecting) 
        {
            fflush(stdout); 

            #ifdef _WIN32
                freopen("CON", "w", stdout);
            #else
                dup2(sav_out, STDOUT_FILENO);
                close(sav_out);
            #endif
            
            import_host_file(tmpf, rfile); 
            remove(tmpf); 
            printf("Redirected to '%s'\n", rfile);
        }
    }
    return 0;
}