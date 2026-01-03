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

// 匯入檔案 (for Put, Redirection)
void import_host_file(char *host_path, char *vfs_name) 
{
    FILE *fp = fopen(host_path, "rb");
    if(!fp) return;
    fseek(fp,0,SEEK_END); int sz=ftell(fp); fseek(fp,0,SEEK_SET);
    int old = find_inode_by_name(vfs_name, current_dir_id);
    if(old!=-1) 
    {
        int obs = (inode_table[old].size+BLOCK_SIZE-1)/BLOCK_SIZE;
        for(int b=0; b<obs; b++) free_block(inode_table[old].blocks[b]);
        inode_table[old].size=0; sb->used_inodes--;
    }
    int idx = (old!=-1)?old:find_free_inode();
    if(idx==-1){fclose(fp);return;}
    
    inode_table[idx].is_used=1; inode_table[idx].is_dir=0; inode_table[idx].permission=7;
    strcpy(inode_table[idx].name, vfs_name); inode_table[idx].parent_id=current_dir_id;
    inode_table[idx].size=sz;
    
    int needs=(sz+BLOCK_SIZE-1)/BLOCK_SIZE;
    for(int i=0; i<needs; i++) 
    {
        int bid=find_free_block();
        inode_table[idx].blocks[i]=bid;
        fread(data_blocks[bid].data, 1, BLOCK_SIZE, fp);
    }
    if(old==-1) sb->used_inodes++;
    fclose(fp);
}

// ls
void cmd_ls() 
{
    int f=0;
    for(int i=0; i<MAX_FILES; i++) 
    {
        if(inode_table[i].is_used && inode_table[i].parent_id==current_dir_id) 
        {
            if(current_dir_id==0 && i==0) continue;
            printf("%s%s%s  ", inode_table[i].is_dir?C_DIR:C_FILE, inode_table[i].name, C_RESET);
            f=1;
        }
    }
    if(f) printf("\n");
}

// ls -l
void cmd_ll() 
{
    // 標題列：增加 "Mode"
    printf("%-6s %-6s %-6s %s\n", "Mode", "Type", "Size", "Name");
    printf("----------------------------------------\n");

    for(int i=0; i<MAX_FILES; i++) 
    {
        if(inode_table[i].is_used && inode_table[i].parent_id==current_dir_id) 
        {
            if(current_dir_id==0 && i==0) continue;
            
            char *type = inode_table[i].is_dir ? "DIR" : "FILE";
            
            // 印出 permission, type, size, name
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

// mkdir 建目錄
void cmd_mkdir(char *name) 
{
    int idx=find_free_inode(); if(idx==-1) return;
    inode_table[idx].is_used=1; inode_table[idx].is_dir=1; inode_table[idx].permission=7;
    strcpy(inode_table[idx].name, name); inode_table[idx].parent_id=current_dir_id;
    sb->used_inodes++;
}

// rm_r (含權限檢查)
void cmd_rm_r(char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1) 
    { 
        printf(C_ERR "Not found.\n" C_RESET); return; 
    }

    // 檢查寫入權限 (Permission & 2)
    if ( !(inode_table[idx].permission & 2) ) 
    {
        printf(C_ERR "Error: Permission denied (Write protected).\n" C_RESET);
        return;
    }

    recursive_delete(idx);
    printf("Removed '%s'.\n", name);
}

void cmd_rm(char *name) 
{ 
    cmd_rm_r(name); 
}

// mv
void cmd_mv(char *src, char *dest) 
{
    int s=find_inode_by_name(src, current_dir_id); if(s==-1) return;
    int d=find_inode_by_name(dest, current_dir_id);
    if(d!=-1 && inode_table[d].is_dir) 
    {
        inode_table[s].parent_id=d;
    } 
    else 
    {
        strcpy(inode_table[s].name, dest);
    }
}

// cp
void cmd_cp(char *src, char *dest) 
{
    int s=find_inode_by_name(src, current_dir_id); if(s==-1 || inode_table[s].is_dir) return;
    int d=find_free_inode(); if(d==-1) return;
    inode_table[d]=inode_table[s]; inode_table[d].id=d;
    strcpy(inode_table[d].name, dest);
    int blks=(inode_table[s].size+BLOCK_SIZE-1)/BLOCK_SIZE;
    for(int i=0; i<blks; i++) 
    {
        int nb=find_free_block();
        inode_table[d].blocks[i]=nb;
        memcpy(data_blocks[nb].data, data_blocks[inode_table[s].blocks[i]].data, BLOCK_SIZE);
    }
    sb->used_inodes++;
}

// put
void cmd_put(char *host_filename) 
{
    FILE *f = fopen(host_filename, "rb");
    if (f == NULL) {
        printf("Error: Host file '%s' not found.\n", host_filename);
        return;
    }

    // Calculate File Size
    fseek(f, 0, SEEK_END);
    int filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    // find inode
    int idx = find_free_inode();
    if (idx == -1) 
    {
        printf("Error: No free inodes in VFS.\n");
        fclose(f);
        return;
    }

    // 只留檔名
    char *vfs_name = strrchr(host_filename, '/');
    if (!vfs_name) vfs_name = strrchr(host_filename, '\\');
    if (vfs_name) vfs_name++;
    else vfs_name = host_filename;

    // 設定 Inode 資訊
    inode_table[idx].is_used = 1;
    inode_table[idx].is_dir = 0;
    inode_table[idx].size = filesize;
    inode_table[idx].parent_id = current_dir_id;
    inode_table[idx].permission = 7; // 預設權限
    strncpy(inode_table[idx].name, vfs_name, 31);
    inode_table[idx].name[31] = '\0';

    // 分配 Block 並寫入資料
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

// get (含權限檢查)
void cmd_get(char *fs_filename) 
{
    int idx=find_inode_by_name(fs_filename, current_dir_id);
    if(idx==-1 || inode_table[idx].is_dir) return;

    // 檢查讀取權限 (Permission & 4)
    if ( !(inode_table[idx].permission & 4) ) 
    {
        printf(C_ERR "Error: Permission denied (Read protected).\n" C_RESET);
        return;
    }

    create_host_dir("dump");
    char path[512]; snprintf(path, 512, "dump/%s", fs_filename);
    FILE *fp=fopen(path, "wb"); if(!fp) return;
    int rem=inode_table[idx].size; int b=0;
    while(rem>0) 
    {
        int cp=(rem>BLOCK_SIZE)?BLOCK_SIZE:rem;
        fwrite(data_blocks[inode_table[idx].blocks[b]].data, 1, cp, fp);
        rem-=cp; b++;
    }
    fclose(fp);
    printf("Saved to %s\n", path);
}

// cat (含權限檢查)
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

    // 檢查是否有 Read (4) 權限
    if ( !(inode_table[idx].permission & 4) ) 
    { 
        printf(C_ERR "Error: Permission denied (Read protected).\n" C_RESET);
        return;
    }

    int rem=inode_table[idx].size; int b=0;
    while(rem>0) 
    {
        int cp=(rem>BLOCK_SIZE)?BLOCK_SIZE:rem;
        fwrite(data_blocks[inode_table[idx].blocks[b]].data, 1, cp, stdout);
        rem-=cp; b++;
    }
    printf("\n");
}

// tree
void print_tree_rec(int dir_id, int depth) 
{
    for(int i=0; i<MAX_FILES; i++) 
    {
        if(inode_table[i].is_used && inode_table[i].parent_id==dir_id) 
        {
            if(dir_id==0 && i==0) continue;
            for(int k=0; k<depth; k++) printf("  ");
            printf("|-- %s%s%s\n", inode_table[i].is_dir?C_DIR:C_FILE, inode_table[i].name, C_RESET);
            if(inode_table[i].is_dir) print_tree_rec(i, depth+1);
        }
    }
}

void cmd_tree() 
{ 
    printf(".\n"); print_tree_rec(current_dir_id, 0); 
}

// chmod
void cmd_chmod(char *mode, char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx!=-1) 
    {
        inode_table[idx].permission=atoi(mode);
        printf("Changed permission of '%s' to %d\n", name, inode_table[idx].permission);
    } 
    else 
    {
        printf(C_ERR "File not found.\n" C_RESET);
    }
}

// pwd
void cmd_pwd() 
{ 
    printf("%s\n", current_path); 
}

// touch
void cmd_touch(char *name) 
{
    if(find_inode_by_name(name, current_dir_id)!=-1) return;
    int idx=find_free_inode(); if(idx==-1) return;
    inode_table[idx].is_used=1; inode_table[idx].is_dir=0; inode_table[idx].permission=7;
    strcpy(inode_table[idx].name, name); inode_table[idx].parent_id=current_dir_id;
    inode_table[idx].size=0; sb->used_inodes++;
    int bid=find_free_block(); inode_table[idx].blocks[0]=bid;
}

// rmdir
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

// append (含權限檢查)
void cmd_append(char *name, char *text) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1) 
    { 
        cmd_touch(name); idx=find_inode_by_name(name, current_dir_id); 
    }
    if(inode_table[idx].is_dir) return;

    // 檢查寫入權限 (Permission & 2)
    if ( !(inode_table[idx].permission & 2) ) {
        printf(C_ERR "Error: Permission denied (Write protected).\n" C_RESET);
        return;
    }

    int len=strlen(text);
    int offset = inode_table[idx].size;
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

// grep (含權限檢查)
void cmd_grep(char *key, char *fname) 
{
    int idx=find_inode_by_name(fname, current_dir_id);
    if(idx==-1 || inode_table[idx].is_dir) return;

    // 檢查讀取權限
    if ( !(inode_table[idx].permission & 4) ) { printf(C_ERR "Permission denied.\n" C_RESET); return; }

    char *buf=malloc(inode_table[idx].size+1);
    int rem=inode_table[idx].size; int p=0; int b=0;
    while(rem>0) 
    {
        int cp=(rem>BLOCK_SIZE)?BLOCK_SIZE:rem;
        memcpy(buf+p, data_blocks[inode_table[idx].blocks[b]].data, cp);
        p+=cp; rem-=cp; b++;
    }
    buf[p]=0;
    char *line=strtok(buf, "\n");
    while(line) { if(strstr(line, key)) printf("%s\n", line); line=strtok(NULL, "\n"); }
    free(buf);
}

// stat
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

// find
void recursive_find(int dir_id, char *target, char *path) 
{
    for(int i=0; i<MAX_FILES; i++) 
    {
        if(inode_table[i].is_used && inode_table[i].parent_id==dir_id) 
        {
            if(dir_id==0 && i==0) continue;
            char newp[256];
            if(strcmp(path,"/")==0) snprintf(newp, 256, "/%s", inode_table[i].name);
            else snprintf(newp, 256, "%s/%s", path, inode_table[i].name);
            if(strstr(inode_table[i].name, target)) printf("%s%s%s\n", inode_table[i].is_dir?C_DIR:C_FILE, newp, C_RESET);
            if(inode_table[i].is_dir) recursive_find(i, target, newp);
        }
    }
}

void cmd_find(char *name) 
{ 
    recursive_find(0, name, "/"); 
}

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

    // 檢查寫入權限
    if ( !(inode_table[idx].permission & 2) ) 
    {
        printf("Error: Permission denied (Write protected).\n");
        return;
    }

    for (int i = 0; i < MAX_BLOCKS_PER_FILE; i++) 
    {
        int bid = inode_table[idx].blocks[i];
        if (bid == 0) continue; // 跳過空的 Block

        xor_cipher(data_blocks[bid].data, BLOCK_SIZE, key);
    }

    printf("File '%s' encrypted/decrypted with key '%s'.\n", filename, key);
}

// Advanced Command

// Status Dashboard
void cmd_status() 
{
    printf("\n" "\033[7m" " SYSTEM STATUS " "\033[0m" "\n");
    printf("Total Size:   %d bytes\n", sb->total_size);
    printf("Blocks:       %d/%d used\n", sb->used_blocks, sb->total_blocks);
    float usage = (float)sb->used_blocks / sb->total_blocks * 100.0;
    printf("Usage: %.1f%%\n[", usage);
    int bar = 40; int fill = (int)((usage/100.0)*bar);
    for(int i=0; i<bar; i++) printf(i<fill?C_OK "#" C_RESET:".");
    printf("]\nInodes:       %d/%d used\n", sb->used_inodes, sb->total_inodes);
}

// Disk Heatmap
void cmd_diskmap() 
{
    printf("\n--- Disk Block Map ---\n");
    printf("Legend: " C_OK "[#]" C_RESET " Used  " "\033[1;30m" "[ ]" C_RESET " Free\n\n");
    int limit = sb->total_blocks > 400 ? 400 : sb->total_blocks;
    for(int i=0; i<limit; i++) 
    {
        if(get_bit(i)) printf(C_OK "[#]" C_RESET);
        else printf("\033[1;30m[ ]\033[0m");
        if((i+1)%32==0) printf("\n");
    }
    printf("\n");
}

// Hexdump (含權限檢查)
void cmd_hexdump(char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1 || inode_table[idx].is_dir) return;

    // 檢查讀取權限
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
        printf("%04x  ", i);
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

// Run Executable (含權限檢查)
void cmd_run(char *name) 
{
    int idx=find_inode_by_name(name, current_dir_id);
    if(idx==-1 || inode_table[idx].is_dir) 
    { 
        printf("Not executable.\n"); return; 
    }

    // 檢查執行權限 (Permission & 1)
    if ( !(inode_table[idx].permission & 1) ) 
    { 
        printf(C_ERR "Error: Permission denied (Not executable).\n" C_RESET);
        return; 
    }

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
    #ifndef _WIN32
    chmod(tpath, 0755);
    #endif
    printf(C_DIR "Running %s...\n" C_RESET, name);
    system(tpath);
    remove(tpath);
}

// cd
void cmd_cd(char *name) 
{
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

// help
void cmd_help() {
    printf("List of commands\n");
    printf("'ls'      list directory\n");
    printf("'cd'      change directory\n");
    printf("'mkdir'   make directory\n");
    printf("'rmdir'   remove directory\n");
    printf("'rm'      remove file (supports -r)\n");
    printf("'mv'      move or rename\n");
    printf("'cp'      copy file\n");
    printf("'put'     put file into the space\n");
    printf("'get'     get file from the space\n");
    printf("'cat'     show content\n");
    printf("'nano'    create or edit text file\n");
    printf("'tree'    show directory structure\n");
    printf("'grep'    search keyword in file\n");
    printf("'find'    search file by name\n");
    printf("'stat'    show detailed file info\n");
    printf("'chmod'   change permission\n");
    printf("'defrag'  defragment file system\n");
    printf("'status'  show status of the space\n");
    printf("'diskmap' visualize disk usage\n");
    printf("'hexdump' view file in hex mode\n");
    printf("'run'     execute binary file\n");
    printf("'help'    show this help message\n");
    printf("'exit'    exit and store img\n");

    printf("\n[Features]\n");
    printf(" * Use Up/Down arrow keys for Command History.\n");
    printf(" * Use '>' to redirect output (e.g., ls > list.txt).\n");
}