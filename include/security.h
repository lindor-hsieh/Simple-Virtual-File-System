#ifndef SECURITY_H
#define SECURITY_H

// 加密/解密 (Use XOR)
void xor_cipher(void *data, int size, const char *key);
// Check Password
int check_password(const char *stored_pwd);
// Set Password
void set_new_password(char *buffer, int max_len);

#endif