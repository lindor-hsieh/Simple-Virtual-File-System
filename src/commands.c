#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h> 
#include "commands.h"
#include "fs.h"
#include "inode.h"
#include "bitmap.h"
#include "utils.h"
#include "editor.h"
#include "security.h"

// 匯檔 (for Put, Redirection)
void import_host_file(char *host_path, char *vfs_name) 
{
    // 開 Host 的檔案
    FILE *fp = fopen(host_path, "rb");
    if(!fp) return;

    // 計算檔案大小
    fseek(fp,0,SEEK_END); int sz=ftell(fp); fseek(fp,0,SEEK_SET);

    // 是否已有同名檔案 (若有則覆寫)
    int old = find_inode_by_name(vfs_name, current_dir_id);
    if(old!=-1) 
    {
        // 若有,先釋出舊檔佔用的 Blocks
        int obs = (inode_table[old].size+BLOCK_SIZE-1)/BLOCK_SIZE;
        for(int b=0; b<obs; b++) free_block(inode_table[old].blocks[b]);
        inode_table[old].size=0; sb->used_inodes--;
    }

    // 取得一個新的 Inode (若有舊檔就用舊的 ID)
    int idx = (old!=-1)?old:find_free_inode();
    if(idx==-1){fclose(fp);return;}
    
    inode_table[idx].is_used=1; inode_table[idx].is_dir=0; inode_table[idx].permission=7;
    strcpy(inode_table[idx].name, vfs_name); inode_table[idx].parent_id=current_dir_id;
    inode_table[idx].size=sz;
    
    // 計算需要多少個 Blocks,並寫入data
    int needs=(sz+BLOCK_SIZE-1)/BLOCK_SIZE;

    for(int i=0; i<needs; i++) 
    {
        int bid=find_free_block(); 
        inode_table[idx].blocks[i]=bid;
        fread(data_blocks[bid].data, 1, BLOCK_SIZE, fp);
    }
    if(old==-1) sb->used_inodes++; // if is new
    fclose(fp);
}

// funtion: ls
void cmd_ls() 
{
    int f=0;

    for(int i=0; i<MAX_FILES; i++) 
    {
        // 顯示已使用且父目錄是當前目錄的檔
        if(inode_table[i].is_used && inode_table[i].parent_id==current_dir_id) 
        {
            if(current_dir_id==0 && i==0) continue; // 跳過自己
            printf("%s%s%s  ", inode_table[i].is_dir?C_DIR:C_FILE, inode_table[i].name, C_RESET);
            f=1;
        }
    }
    if(f) printf("\n");
}

// funtion: ls -l
void cmd_ll() 
{
    printf("%-6s %-6s %-6s %s\n", "Mode", "Type", "Size", "Name");
    printf("----------------------------------------\n");

    for(int i=0; i<MAX_FILES; i++) 
    {
        if(inode_table[i].is_used && inode_table[i].parent_id==current_dir_id) 
        {
            if(current_dir_id==0 && i==0) continue;
            
            char *type = inode_table[i].is_dir ? "DIR" : "FILE";
            
            printf("%-6d %-6s %-6d %s%s%s\n", 
                   inode_table[i].permission, 
                   type, 
                   inode_table[i].size, 
                   inode_table[i].is_dir?C_DIR:C_FILE,
                   inode_table[i].name,
                   C_RESET);
        }
    }
}

// funtion: mkdir
void cmd_mkdir(char *name) 
{
    int idx=find_free_inode(); if(idx==-1) return; // 找空的 Inode
    // 設定屬性 (is_dir=1)
    inode_table[idx].is_used=1; inode_table[idx].is_dir=1; inode_table[idx].permission=7;
    strcpy(inode_table[idx].name, name); inode_table[idx].parent_id=current_dir_id;
    sb->used_inodes++;
}

// funtion: rm -r
void cmd_rm_r(char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1) 
    { 
        printf(C_ERR "Not found.\n" C_RESET); return; 
    }

    // !!權限檢查!! 必須有 Write權限才能刪
    if ( !(inode_table[idx].permission & 2) ) 
    {
        printf(C_ERR "Error: Permission denied (Write protected).\n" C_RESET);
        return;
    }

    recursive_delete(idx);
    printf("Removed '%s'.\n", name);
}

// funtion: rm
void cmd_rm(char *name) 
{ 
    cmd_rm_r(name); 
}

// funtion: mv
void cmd_mv(char *src, char *dest) 
{
    int s=find_inode_by_name(src, current_dir_id); if(s==-1) return;
    int d=find_inode_by_name(dest, current_dir_id);
    
    // 如果target是一個已存在的目錄,則將檔案移進去 (改 parent_id)
    if(d!=-1 && inode_table[d].is_dir) 
    {
        inode_table[s].parent_id=d;
    } 
    else 
    {
        strcpy(inode_table[s].name, dest);
    }
}

// funtion: cp
void cmd_cp(char *src, char *dest) 
{
    int s=find_inode_by_name(src, current_dir_id); if(s==-1 || inode_table[s].is_dir) return;
    int d=find_free_inode(); if(d==-1) return;

    // 複製屬性
    inode_table[d]=inode_table[s]; inode_table[d].id=d;
    strcpy(inode_table[d].name, dest);

    // 複製資料
    int blks=(inode_table[s].size+BLOCK_SIZE-1)/BLOCK_SIZE;
    for(int i=0; i<blks; i++) 
    {
        int nb=find_free_block();
        inode_table[d].blocks[i]=nb;
        memcpy(data_blocks[nb].data, data_blocks[inode_table[s].blocks[i]].data, BLOCK_SIZE);
    }
    sb->used_inodes++;
}

// funtion: put
void cmd_put(char *host_filename) 
{
    FILE *f = fopen(host_filename, "rb");
    if (f == NULL) 
    {
        printf("Error: Host file '%s' not found.\n", host_filename);
        return;
    }

    // 計算檔案大小
    fseek(f, 0, SEEK_END);
    int filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    // 找空 Inode
    int idx = find_free_inode();
    if (idx == -1) 
    {
        printf("Error: No free inodes in VFS.\n");
        fclose(f);
        return;
    }

    // 去除路徑,只留檔名
    char *vfs_name = strrchr(host_filename, '/');
    if (!vfs_name) vfs_name = strrchr(host_filename, '\\');
    if (vfs_name) vfs_name++;
    else vfs_name = host_filename;

    inode_table[idx].is_used = 1;
    inode_table[idx].is_dir = 0;
    inode_table[idx].size = filesize;
    inode_table[idx].parent_id = current_dir_id;
    inode_table[idx].permission = 7;
    strncpy(inode_table[idx].name, vfs_name, 31);
    inode_table[idx].name[31] = '\0';

    // 分配 Block 並寫入data
    int blocks_needed = (filesize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    char buffer[BLOCK_SIZE];

    for (int i = 0; i < blocks_needed; i++) 
    {
        int bid = find_free_block();
        if (bid == -1) 
        {
            printf("Error: Disk full (partial write).\n");
            break;
        }
        inode_table[idx].blocks[i] = bid;
        
        memset(buffer, 0, BLOCK_SIZE);
        fread(buffer, 1, BLOCK_SIZE, f);
        memcpy(data_blocks[bid].data, buffer, BLOCK_SIZE);
    }

    sb->used_inodes++;
    fclose(f);
    printf("Put '%s' done. (Size: %d bytes)\n", vfs_name, filesize);
}

// funtion: get
void cmd_get(char *fs_filename) 
{
    int idx=find_inode_by_name(fs_filename, current_dir_id);
    if(idx==-1 || inode_table[idx].is_dir) return;

    // !!權限檢查!! 必須有 Read 權限
    if ( !(inode_table[idx].permission & 4) ) 
    {
        printf(C_ERR "Error: Permission denied (Read protected).\n" C_RESET);
        return;
    }

    create_host_dir("dump"); 
    char path[512]; snprintf(path, 512, "dump/%s", fs_filename);
    
    FILE *fp=fopen(path, "wb"); if(!fp) return;
    int rem=inode_table[idx].size; int b=0;
    
    // 寫出資料到 Host 檔案
    while(rem>0) 
    {
        int cp=(rem>BLOCK_SIZE)?BLOCK_SIZE:rem;
        fwrite(data_blocks[inode_table[idx].blocks[b]].data, 1, cp, fp);
        rem-=cp; b++;
    }
    fclose(fp);
    printf("Saved to %s\n", path);
}

// funtion: cat
void cmd_cat(char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1) 
    {
        printf(C_ERR "File not found.\n" C_RESET);
        return;
    }
    if(inode_table[idx].is_dir) {
        printf(C_ERR "Is a directory.\n" C_RESET);
        return;
    }

    // !!權限檢查!! 必須有 Read權限
    if ( !(inode_table[idx].permission & 4) ) 
    { 
        printf(C_ERR "Error: Permission denied (Read protected).\n" C_RESET);
        return;
    }

    // 讀取 Block 資料
    int rem=inode_table[idx].size; int b=0;
    while(rem>0) 
    {
        int cp=(rem>BLOCK_SIZE)?BLOCK_SIZE:rem;
        fwrite(data_blocks[inode_table[idx].blocks[b]].data, 1, cp, stdout);
        rem-=cp; b++;
    }
    printf("\n");
}

// funtion: tree
void print_tree_rec(int dir_id, int depth) 
{
    for(int i=0; i<MAX_FILES; i++) 
    {
        if(inode_table[i].is_used && inode_table[i].parent_id==dir_id) 
        {
            if(dir_id==0 && i==0) continue;
            // 根據深度印縮排
            for(int k=0; k<depth; k++) printf("  ");
            printf("|-- %s%s%s\n", inode_table[i].is_dir?C_DIR:C_FILE, inode_table[i].name, C_RESET);
            // 如果是目錄,遞迴呼叫
            if(inode_table[i].is_dir) print_tree_rec(i, depth+1);
        }
    }
}

void cmd_tree() 
{ 
    printf(".\n"); print_tree_rec(current_dir_id, 0); 
}

// funtion: chmod
void cmd_chmod(char *mode, char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx!=-1) 
    {
        // only owner permission
        inode_table[idx].permission=atoi(mode);
        printf("Changed permission of '%s' to %d\n", name, inode_table[idx].permission);
    } 
    else 
    {
        printf(C_ERR "File not found.\n" C_RESET);
    }
}

// funtion: pwd
void cmd_pwd() 
{ 
    printf("%s\n", current_path); 
}

// funtion: touch
void cmd_touch(char *name) 
{
    if(find_inode_by_name(name, current_dir_id)!=-1) return; 
    int idx=find_free_inode(); if(idx==-1) return;
    
    inode_table[idx].is_used=1; inode_table[idx].is_dir=0; inode_table[idx].permission=7;
    strcpy(inode_table[idx].name, name); inode_table[idx].parent_id=current_dir_id;
    inode_table[idx].size=0; sb->used_inodes++;
    
    // 預先分配一個 Block
    int bid=find_free_block(); inode_table[idx].blocks[0]=bid;
}

// funtion: rmdir
void cmd_rmdir(char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1 || !inode_table[idx].is_dir) return;
    
    for(int i=0; i<MAX_FILES; i++) if(inode_table[i].is_used && inode_table[i].parent_id==idx) 
    {
        printf(C_ERR "Dir not empty.\n" C_RESET); return;
    }
    inode_table[idx].is_used=0; sb->used_inodes--;
}

// funtion: append
void cmd_append(char *name, char *text) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1) 
    { 
        cmd_touch(name); idx=find_inode_by_name(name, current_dir_id); 
    }
    if(inode_table[idx].is_dir) return;

    // !!權限檢查!! 必須有 Write 權限
    if ( !(inode_table[idx].permission & 2) ) 
    {
        printf(C_ERR "Error: Permission denied (Write protected).\n" C_RESET);
        return;
    }

    int len=strlen(text);
    int offset = inode_table[idx].size;
    
    // 一一寫入,若 Block 滿了自動要新的 Block
    for(int i=0; i<len; i++) 
    {
        int pos = offset+i;
        int b_idx = pos/BLOCK_SIZE;
        int b_off = pos%BLOCK_SIZE;
        if(inode_table[idx].blocks[b_idx]==0) inode_table[idx].blocks[b_idx]=find_free_block();
        data_blocks[inode_table[idx].blocks[b_idx]].data[b_off]=text[i];
    }
    inode_table[idx].size+=len;
    printf("Appended.\n");
}

// funtion: grep
void cmd_grep(char *key, char *fname) 
{
    int idx=find_inode_by_name(fname, current_dir_id);
    if(idx==-1 || inode_table[idx].is_dir) return;

    // !!權限檢查!! 必須有 Read 權限
    if ( !(inode_table[idx].permission & 4) ) 
    { 
        printf(C_ERR "Permission denied.\n" C_RESET); return; 
    }

    // 讀整個檔案到 buffer
    char *buf=malloc(inode_table[idx].size+1);
    int rem=inode_table[idx].size; int p=0; int b=0;
    while(rem>0) 
    {
        int cp=(rem>BLOCK_SIZE)?BLOCK_SIZE:rem;
        memcpy(buf+p, data_blocks[inode_table[idx].blocks[b]].data, cp);
        p+=cp; rem-=cp; b++;
    }
    buf[p]=0;
    
    // 逐行搜尋key word
    char *line=strtok(buf, "\n");
    while(line) { if(strstr(line, key)) printf("%s\n", line); line=strtok(NULL, "\n"); }
    free(buf);
}

// funtion: stat
void cmd_stat(char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1) 
    { 
        printf(C_ERR "File not found.\n" C_RESET); return; 
    }
    printf("File: %s\nSize: %d\nInode: %d\nType: %s\nMode: %d\n", 
           inode_table[idx].name, inode_table[idx].size, idx, 
           inode_table[idx].is_dir?"DIR":"FILE", inode_table[idx].permission);
}

// funtion: find
void recursive_find(int dir_id, char *target, char *path) 
{
    for(int i=0; i<MAX_FILES; i++) 
    {
        if(inode_table[i].is_used && inode_table[i].parent_id==dir_id) 
        {
            if(dir_id==0 && i==0) continue;
            char newp[256];
            // 組合完整路徑
            if(strcmp(path,"/")==0) snprintf(newp, 256, "/%s", inode_table[i].name);
            else snprintf(newp, 256, "%s/%s", path, inode_table[i].name);
            
            // 比對檔名
            if(strstr(inode_table[i].name, target)) printf("%s%s%s\n", inode_table[i].is_dir?C_DIR:C_FILE, newp, C_RESET);
            if(inode_table[i].is_dir) recursive_find(i, target, newp);
        }
    }
}

void cmd_find(char *name) 
{ 
    recursive_find(0, name, "/"); 
}

// funtion: encrypt/decrypt
void cmd_encrypt(char *filename, char *key) 
{
    if (!key || strlen(key) == 0) 
    {
        printf("Error: Password required.\n");
        return;
    }

    int idx = find_inode_by_name(filename, current_dir_id);
    if (idx == -1) 
    {
        printf("Error: File '%s' not found.\n", filename);
        return;
    }
    if (inode_table[idx].is_dir) 
    {
        printf("Error: Cannot encrypt directory.\n");
        return;
    }

    // !!權限檢查!! 需要 Write 權限
    if ( !(inode_table[idx].permission & 2) ) 
    {
        printf("Error: Permission denied (Write protected).\n");
        return;
    }

    for (int i = 0; i < MAX_BLOCKS_PER_FILE; i++) 
    {
        int bid = inode_table[idx].blocks[i];
        if (bid == 0) continue;

        xor_cipher(data_blocks[bid].data, BLOCK_SIZE, key);
    }

    printf("File '%s' encrypted/decrypted with key '%s'.\n", filename, key);
}

// funtion: status
void cmd_status() 
{
    printf("\n" "\033[7m" " SYSTEM STATUS " "\033[0m" "\n");
    printf("Total Size:   %d bytes\n", sb->total_size);
    printf("Blocks:       %d/%d used\n", sb->used_blocks, sb->total_blocks);
    
    // 進度條實作
    float usage = (float)sb->used_blocks / sb->total_blocks * 100.0;
    printf("Usage: %.1f%%\n[", usage);
    int bar = 40; int fill = (int)((usage/100.0)*bar);
    for(int i=0; i<bar; i++) printf(i<fill?C_OK "#" C_RESET:".");
    printf("]\nInodes:       %d/%d used\n", sb->used_inodes, sb->total_inodes);
}

// funtion: diskmap
void cmd_diskmap() 
{
    printf("\n--- Disk Block Map ---\n");
    printf("Legend: " C_OK "[#]" C_RESET " Used  " "\033[1;30m" "[ ]" C_RESET " Free\n\n");
    int limit = sb->total_blocks > 400 ? 400 : sb->total_blocks;
    for(int i=0; i<limit; i++) 
    {
        // 讀取 Bitmap,顯示區塊使用狀態
        if(get_bit(i)) printf(C_OK "[#]" C_RESET);
        else printf("\033[1;30m[ ]\033[0m");
        if((i+1)%32==0) printf("\n"); 
    }
    printf("\n");
}

// funtion: hexdump
void cmd_hexdump(char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1 || inode_table[idx].is_dir) return;

    // !!權限檢查!! 必須有 Read (4) 權限
    if ( !(inode_table[idx].permission & 4) ) 
    { 
        printf(C_ERR "Permission denied.\n" C_RESET); return; 
    }

    unsigned char *buf=malloc(inode_table[idx].size);
    int rem=inode_table[idx].size; int p=0; int b=0;
    while(rem>0) 
    {
        int cp=(rem>BLOCK_SIZE)?BLOCK_SIZE:rem;
        memcpy(buf+p, data_blocks[inode_table[idx].blocks[b]].data, cp);
        p+=cp; rem-=cp; b++;
    }
    printf("Hex Dump of %s:\n", name);

    for(int i=0; i<inode_table[idx].size; i+=16) 
    {
        printf("%04x  ", i); // Offset
        // 印出 Hex
        for(int j=0; j<16; j++) 
        {
            if(i+j<inode_table[idx].size) printf("%02x ", buf[i+j]); else printf("   ");
            if(j==7) printf(" ");
        }
        printf(" |");

        for(int j=0; j<16; j++) 
        {
            if(i+j<inode_table[idx].size) 
            {
                unsigned char c=buf[i+j];
                printf("%c", (c>=32&&c<=126)?c:'.');
            }
        }
        printf("|\n");
    }
    free(buf);
}

// function: run
void cmd_run(char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1 || inode_table[idx].is_dir) 
    { 
        printf("Not executable.\n"); return; 
    }

    // !!權限檢查!! 必須有 Exec 權限
    if ( !(inode_table[idx].permission & 1) ) 
    { 
        printf(C_ERR "Error: Permission denied (Not executable).\n" C_RESET);
        return; 
    }

    // 將 FS 中的內容匯出到暫存實體檔案
    char tpath[64];
    #ifdef _WIN32
    sprintf(tpath, "._temp_exec.exe");
    #else
    sprintf(tpath, "./._temp_exec");
    #endif
    
    FILE *fp=fopen(tpath, "wb"); if(!fp) return;

    int rem=inode_table[idx].size; int b=0;
    while(rem>0) 
    {
        int cp=(rem>BLOCK_SIZE)?BLOCK_SIZE:rem;
        fwrite(data_blocks[inode_table[idx].blocks[b]].data, 1, cp, fp);
        rem-=cp; b++;
    }

    fclose(fp);

    // 透過 system() 呼叫 OS 執行該暫存檔
    #ifndef _WIN32
    chmod(tpath, 0755);
    #endif
    printf(C_DIR "Running %s...\n" C_RESET, name);
    system(tpath);
    remove(tpath); 
}

// function: cd
void cmd_cd(char *name) 
{
    // 處理 ".."
    if(strcmp(name, "..")==0) 
    {
        current_dir_id=inode_table[current_dir_id].parent_id;
        if(current_dir_id==0) strcpy(current_path, "/"); else strcpy(current_path, "/...");
        return;
    }
    
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx!=-1 && inode_table[idx].is_dir) 
    {
        current_dir_id=idx; strcat(current_path, name); strcat(current_path, "/");
    }
}

// funtion: help
void cmd_help() 
{
    printf("\n--- MyFS Command List ---\n");

    printf(" [Navigation]\n");
    printf("  ls        : List directory content\n");
    printf("  ll or ls -l: List detailed content (Mode/Size/Date)\n");
    printf("  cd <dir>  : Change directory (.. for parent)\n");
    printf("  pwd       : Show current path\n");
    printf("  tree      : Show directory structure recursively\n");
    printf("  mkdir <d> : Create new directory\n");
    printf("  rmdir <d> : Remove empty directory\n");

    printf("\n [File Operations]\n");
    printf("  touch <f> : Create empty file\n"); 
    printf("  rm <name> : Remove file or directory (supports -r)\n");
    printf("  mv <s, d> : Move or rename file\n");
    printf("  cp <s, d> : Copy file\n");
    printf("  nano <f>  : Open text editor\n");
    printf("  append    : Append text to file (Usage: append <file> <text>)\n"); 

    printf("\n [View & Search]\n");
    printf("  cat <f>   : Display file content\n");
    printf("  hexdump<f>: View file in hexadecimal\n");
    printf("  grep <k,f>: Search keyword in file\n");
    printf("  find <n>  : Search file by name (recursive)\n");
    printf("  stat <f>  : Show inode details (Size, ID, Perm)\n");

    printf("\n [Host I/O]\n");
    printf("  put <f>   : Import file from Host (Windows) to MyFS\n");
    printf("  get <f>   : Export file from MyFS to Host\n");

    printf("\n [Security & System]\n");
    printf("  chmod <m> : Change permission (e.g., chmod 7 file)\n");
    printf("  encrypt   : Encrypt file with XOR key (Usage: encrypt <file> <key>)\n");
    printf("  decrypt   : Decrypt file (Same as encrypt)\n"); 
    printf("  run <f>   : Execute binary file (.exe)\n");
    printf("  status    : Show system status (Inode/Block usage)\n");
    printf("  diskmap   : Visualize disk block usage (Heatmap)\n");

    printf("\n [Shell]\n");
    printf("  help      : Show this help message\n");
    printf("  exit      : Save disk image and exit\n");

    printf("\n[Features]\n");
    printf(" * Use Up/Down arrow keys for Command History.\n");
    printf(" * Use '>' to redirect output (e.g., ls > list.txt).\n");
}