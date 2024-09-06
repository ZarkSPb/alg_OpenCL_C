CC = gcc
CFLAGS = -Wall -O3
TARGET = main
SRCS = $(wildcard src/*.c)

# Определение платформы и архиектуры процессора
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Настойки для каждой платформы
ifeq ($(UNAME_S), Linux)
	ifeq ($(UNAME_M), x86_64)
		CFLAGS += -march=alderlake -mtune=alderlake
	endif
	CFLAGS += -I /usr/include
	LDFLAGS = -L /usr/lib -lOpenCL
endif

ifeq ($(UNAME_S), Darwin)
	ifeq ($(UNAME_M), arm64)
		CFLAGS += -mcpu=apple-m1
	endif
	LDFLAGS = -framework OpenCL
endif

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)