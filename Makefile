CC = gcc
CFLAGS = -Wall -g -Iinclude

TARGET = myfs
# 自動搜尋 src 下所有的 .c 檔
SRCS = $(wildcard src/*.c)
# 將 .c 替換為 obj 資料夾下的 .o 檔
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))

# 主要編譯規則
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# 將每個 .c 編譯成 .o 的規則
obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

# 建立 obj 資料夾
obj:
	if not exist obj mkdir obj

# 清除規則
clean:
	-del /Q $(TARGET).exe my_fs.dump 2>NUL
	-rmdir /S /Q obj 2>NUL
	-rmdir /S /Q dump 2>NUL
	-if exist "-p" rmdir /S /Q "-p" 2>NUL

.PHONY: clean