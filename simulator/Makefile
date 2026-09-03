CC = gcc
CFLAGS = -Wall -O2 -Iinclude
LDFLAGS = -lws2_32

SRC = src/main.c src/pv_system.c src/ipc_socket.c
TARGET = simulator.exe

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	cmd /c del /f /q $(TARGET) 2>nul || exit 0
