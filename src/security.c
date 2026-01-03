#include <stdio.h>
#include <string.h>
#include "security.h"
#include "utils.h"

// XOR Function
void xor_cipher(void *data, int size, const char *key) 
{
    if (!key || strlen(key) == 0) return; // 無密碼不處理
    char *ptr = (char *)data;
    int klen = strlen(key);
    for(int i=0; i<size; i++) 
    {
        // 對每個 byte 做 XOR
        ptr[i] ^= key[i % klen];
    }
}

// Check Password
int check_password(const char *stored_pwd) 
{
    // if no password,pass
    if (strlen(stored_pwd) == 0) return 1;
    
    char input[32];
    printf("Enter password: ");
    scanf("%31s", input);
    
    // clear input buffer
    int c; while((c=getchar())!='\n' && c!=EOF); 

    if(strcmp(input, stored_pwd)==0) 
    {
        printf(C_OK "Password accepted.\n" C_RESET);
        return 1;
    }
    printf(C_ERR "Wrong password!\n" C_RESET);
    return 0;
}

// Set Password
void set_new_password(char *buffer, int max_len) 
{
    char pwd[32];
    printf("Set password (Enter for none): ");
    if(fgets(pwd, sizeof(pwd), stdin)) 
    {
        pwd[strcspn(pwd, "\n")] = 0; // 移除 fgets 讀入的換行符號
        if(strlen(pwd) >= max_len) pwd[max_len-1]=0; // prevent overflow
        strcpy(buffer, pwd);
    }
}