# ============================================
# 知识点标记：序号1 - 基础要求
# UNIX系统下可编译运行
# ============================================
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread
TARGET = chat_server
CLIENT_TARGET = test_client
SOURCES = server.c thread_pool.c connection_pool.c
CLIENT_SOURCES = client_test.c
OBJECTS = $(SOURCES:.c=.o)
CLIENT_OBJECTS = $(CLIENT_SOURCES:.c=.o)

.PHONY: all clean client

all: $(TARGET)

client: $(CLIENT_TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) -pthread

$(CLIENT_TARGET): $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) $(CLIENT_OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(CLIENT_TARGET) $(OBJECTS) $(CLIENT_OBJECTS)

