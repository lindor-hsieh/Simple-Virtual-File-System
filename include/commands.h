#ifndef COMMANDS_H
#define COMMANDS_H

// Basic Command
void cmd_ls();
void cmd_ll();
void cmd_mkdir(char *name);
void cmd_cd(char *name);
void cmd_touch(char *name);
void cmd_rm(char *name);
void cmd_rm_r(char *name);
void cmd_rmdir(char *name);
void cmd_mv(char *src, char *dest);
void cmd_cp(char *src, char *dest);
void cmd_cat(char *name);
void cmd_pwd();

// Exchange with host
void cmd_put(char *host_filename);
void cmd_get(char *fs_filename);

// Extend Command
void cmd_append(char *name, char *text);
void cmd_nano(char *name);
void cmd_grep(char *keyword, char *filename);

// Advanced Command
void cmd_tree();
void cmd_stat(char *name);
void cmd_find(char *name);
void cmd_encrypt(char *filename, char *key);
void cmd_chmod(char *mode, char *name);
void cmd_status();
void cmd_defrag();
void cmd_help();

// Visualization
void cmd_diskmap();
void cmd_hexdump(char *name);
void cmd_run(char *name);

void import_host_file(char *host_path, char *vfs_name);

#endif