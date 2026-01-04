#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "editor.h"
#include "fs.h"
#include "inode.h"
#include "bitmap.h"
#include "utils.h"

// 根據作業系統選擇不同的標頭檔和函式
#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #include <io.h>
    #define STDOUT_FILENO 1
    #define STDIN_FILENO 0

    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
#else
    #include <unistd.h>
    #include <termios.h>
#endif

// 定義 Ctrl+Key 的組合鍵數值
#define CTRL_KEY(k) ((k) & 0x1f)
#define MAX_BUFFER_SIZE (BLOCK_SIZE * MAX_BLOCKS_PER_FILE)

// 定義特殊按鍵的 Enum
enum editorKey 
{ 
    ARROW_LEFT = 1000, ARROW_RIGHT, ARROW_UP, ARROW_DOWN, DEL_KEY 
};

struct EditorConfig 
{
    int cx, cy;             // 游標在螢幕上的座標 (x, y)
    int file_idx;           // 游標在 buffer 中的索引位置
    char buffer[MAX_BUFFER_SIZE]; // 檔案內容buffer
    int len;                // 目前檔案內容長度
    char filename[32]; 
    char status_msg[80];
    #ifndef _WIN32
    struct termios orig_termios; // Linux: 儲存原始終端機設定 
    #endif
};
struct EditorConfig E;

// [Linux] 恢復正常終端機模式
void disableRawMode() 
{
    #ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
    #endif
}

// 讓程式能直接讀取每一個按鍵輸入 (不需按 Enter),並關閉系統預設的 Ctrl+C 等
void enableRawMode() 
{
    #ifdef _WIN32
    // [Windows] 啟用 VT100 模式以支援 ANSI Escape Code (顏色、游標移動)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    #else
    // [Linux] 修改 termios 結構,關閉 ECHO, CANONical mode 等
    tcgetattr(STDIN_FILENO, &E.orig_termios); atexit(disableRawMode);
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT|ICRNL|INPCK|ISTRIP|IXON); raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8); raw.c_lflag &= ~(ECHO|ICANON|IEXTEN|ISIG);
    raw.c_cc[VMIN]=0; raw.c_cc[VTIME]=1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    #endif
}

// 處理特殊鍵 ex方向鍵
int editorReadKey() 
{
    #ifdef _WIN32
        // [Windows] 使用 _getch()
        int c = _getch();
        if (c == 0 || c == 224) 
        { 
            switch (_getch()) 
            {
                case 72: return ARROW_UP; case 80: return ARROW_DOWN;
                case 75: return ARROW_LEFT; case 77: return ARROW_RIGHT;
                case 83: return DEL_KEY;
            }
        }
        return c;
    #else
        // [Linux] 使用 read() 並解析 Escape Sequence (ex ^[[A 代表上)
        int nread; char c; while ((nread = read(STDIN_FILENO, &c, 1)) != 1) { if (nread == -1 && errno != EAGAIN) exit(1); }
        if (c == '\x1b') 
        {
            char seq[3]; if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b'; if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
            if (seq[0] == '[') 
            { switch (seq[1]) 
                { 
                    case 'A': return ARROW_UP; case 'B': return ARROW_DOWN; case 'C': return ARROW_RIGHT; case 'D': return ARROW_LEFT; case '3': if (read(STDIN_FILENO, &seq[2], 1)!=1) return '\x1b'; if (seq[2]=='~') return DEL_KEY; break; 
                } 
            }
            return '\x1b';
        }
        return c;
    #endif
}

// 根據 buffer 中的 '\n' 更新游標的螢幕座標 (cx, cy)
void updateWindowCoords() 
{
    int x=0, y=0;
    for(int i=0; i<E.file_idx; i++) 
    { 
        if(E.buffer[i]=='\n') { x=0; y++; } else x++; 
    }
    E.cx = x; E.cy = y;
}

// 處理游標移動
void editorMoveCursor(int key) 
{
    switch(key) 
    {
        case ARROW_LEFT: if(E.file_idx>0) E.file_idx--; break;
        case ARROW_RIGHT: if(E.file_idx<E.len) E.file_idx++; break;
        case ARROW_UP: 
        {
            // 向上移動：計算上一行的對應位置
            if(E.cy==0) break;
            updateWindowCoords();
            int col = E.cx;
            int i = E.file_idx;
            while(i>0 && E.buffer[i-1]!='\n') i--; // 移到行首
            i--; // 移到上一行行尾
            int p_end = i;
            while(i>0 && E.buffer[i-1]!='\n') i--; // 移到上一行行首
            int p_start = i;
            int p_len = p_end - p_start + 1;
            if(col > p_len) col = p_len; // 避免超出上一行長度
            E.file_idx = p_start + col;
        } 
        break;
        case ARROW_DOWN: 
        {
            // 向下移動：計算下一行的對應位置
            updateWindowCoords();
            int col = E.cx;
            int i = E.file_idx;
            while(i<E.len && E.buffer[i]!='\n') i++; // 移到目前行尾
            if(i>=E.len) break;
            i++; // 移到下一行行首
            int n_start = i;
            while(i<E.len && E.buffer[i]!='\n') i++; // 移到下一行行尾
            int n_len = i - n_start;
            if(col > n_len) col = n_len; // 避免超出下一行長度
            E.file_idx = n_start + col;
        } 
        break;
    }
    updateWindowCoords();
}

// 插入字元
void editorInsertChar(int c) 
{
    if(E.len>=MAX_BUFFER_SIZE-1) return; 
    // 將游標後的資料往後搬移一格
    memmove(&E.buffer[E.file_idx+1], &E.buffer[E.file_idx], E.len-E.file_idx);
    E.buffer[E.file_idx]=c; E.len++; E.file_idx++; updateWindowCoords();
}

// 刪除字元 (Backspace)
void editorDelChar() 
{
    if(E.file_idx==0) return;
    // 將游標後的資料往前搬移一格
    memmove(&E.buffer[E.file_idx-1], &E.buffer[E.file_idx], E.len-E.file_idx);
    E.len--; E.file_idx--; updateWindowCoords();
}

void editorSave() 
{
    int idx = find_inode_by_name(E.filename, current_dir_id);
    if(idx != -1) 
    {
        if( !(inode_table[idx].permission & 2) ) 
        {
            strcpy(E.status_msg, "Error: Permission denied (Write protected)");
            return;
        }
    }

    if(idx==-1) 
    {
        idx=find_free_inode(); if(idx==-1) 
        { 
            strcpy(E.status_msg,"Error: No Inodes"); return; 
        }
        inode_table[idx].is_used=1; inode_table[idx].is_dir=0; inode_table[idx].permission=7;
        strcpy(inode_table[idx].name, E.filename); inode_table[idx].parent_id=current_dir_id; sb->used_inodes++;
        int bid=find_free_block(); if(bid==-1) 
        { 
            strcpy(E.status_msg,"Error: Disk Full"); return; 
        }
        inode_table[idx].blocks[0]=bid;
    }

    // 將 Buffer 資料寫入 Blocks
    inode_table[idx].size = E.len;
    int needs = (E.len + BLOCK_SIZE - 1)/BLOCK_SIZE;
    for(int i=0; i<needs; i++) 
    {
        int bid; if(inode_table[idx].blocks[i]>0) bid = inode_table[idx].blocks[i]; else { bid=find_free_block(); inode_table[idx].blocks[i]=bid; }
        int w_size = (E.len-(i*BLOCK_SIZE)>BLOCK_SIZE) ? BLOCK_SIZE : (E.len-(i*BLOCK_SIZE));
        memcpy(data_blocks[bid].data, E.buffer+(i*BLOCK_SIZE), w_size);
    }
    strcpy(E.status_msg, "File Saved.");
}

// 更新螢幕畫面
void editorRefreshScreen() 
{
    // 隱藏游標 -> 移到左上角 -> 清除螢幕
    printf("\x1b[?25l\x1b[H\x1b[2J");
    
    // 標題列 (反白)
    printf("\033[7m  Nano: %s \033[m\r\n", E.filename);
    
    // 內容
    for(int i=0; i<E.len; i++) 
    { 
        if(E.buffer[i]=='\n') printf("\r\n"); else printf("%c", E.buffer[i]); 
    }
    
    // 底部狀態列
    printf("\x1b[22;1H\033[7m [Ctrl+S] Save  [Ctrl+X] Exit  |  %s \033[m", E.status_msg);
    
    // 移動游標到正確位置 -> 顯示游標
    updateWindowCoords(); printf("\x1b[%d;%dH\x1b[?25h", E.cy+2, E.cx+1); fflush(stdout);
}

// 
void cmd_nano(char *name) 
{
    E.cx=0; E.cy=0; E.file_idx=0; E.len=0; strcpy(E.filename, name); strcpy(E.status_msg, "Ready"); memset(E.buffer, 0, MAX_BUFFER_SIZE);
    
    int idx = find_inode_by_name(name, current_dir_id);
    if(idx!=-1 && !inode_table[idx].is_dir) 
    {
        int rem=inode_table[idx].size; int b=0; int p=0;
        while(rem>0) 
        { 
            int cp=(rem>BLOCK_SIZE)?BLOCK_SIZE:rem; int bid=inode_table[idx].blocks[b]; memcpy(E.buffer+p, data_blocks[bid].data, cp); p+=cp; rem-=cp; b++; 
        }
        E.len=p;
    }
    
    enableRawMode(); 
    #ifdef _WIN32
    system("cls");
    #endif

    // 繪畫面 -> 讀鍵 -> 處理動作
    while(1) 
    {
        editorRefreshScreen(); int c = editorReadKey();
        if(c == CTRL_KEY('x')) break; 
        switch(c) 
        {
            case CTRL_KEY('s'): editorSave(); break; 
            case '\r': editorInsertChar('\n'); break; // Enter 鍵
            case 127: case 8: case DEL_KEY: editorDelChar(); break; // 刪除鍵
            case ARROW_UP: case ARROW_DOWN: case ARROW_LEFT: case ARROW_RIGHT: editorMoveCursor(c); break; // 方向鍵
            default: if(!iscntrl(c)) editorInsertChar(c); break; // 一般文字
        }
    }
    
    disableRawMode(); // 恢復終端機
    #ifdef _WIN32
    system("cls");
    #else
    printf("\x1b[2J\x1b[H"); fflush(stdout);
    #endif
}