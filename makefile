CC = gcc
CFLAGS = -Wall
TARGET = main
SRCS = $(wildcard src/*.c)

# Определение платформы
UNAME_S := $(shell uname -s)

# Настойки для каждой платформы
ifeq ($(UNAME_S), Linux)
	CFLAGS += -I /usr/include
	LDFLAGS = -L /usr/lib -lOpenCL
endif

ifeq ($(UNAME_S), Darwin)
# CFLAGS += -I /System/Library/Frameworks/OpenCL.framework/Headers
	LDFLAGS = -framework OpenCL
endif

all: $(TARGET)

$(TARGET): $(SRCS)
# $(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGE)




