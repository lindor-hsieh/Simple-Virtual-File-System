#ifndef UTILS_H
#define UTILS_H

// ANSI 顏色代碼 (讓terminal輸出有顏色)
#define C_RESET   "\033[0m"    // 重置顏色
#define C_DIR     "\033[1;34m" // 藍色 (目錄)
#define C_FILE    "\033[0m"    // 白色 (檔案)
#define C_ERR     "\033[1;31m" // 紅色 (錯誤訊息)
#define C_OK      "\033[1;32m" // 綠色 (成功訊息)
#define C_WARN    "\033[1;33m" // 黃色 (警告)

// 跨平台
void create_host_dir(const char *path);

#endif