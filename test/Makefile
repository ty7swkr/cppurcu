CC	= g++

TARGET  = test

INCLUDE += -I../

########################################
# 공통 플래그
LDFLAGS = -lrt -lpthread

CPPFLAGS += -g -D_REENTRANT
CPPFLAGS += -O2 -std=c++17 -Wall -Wextra -Wfloat-equal -m64

all: SOURCES = test.cpp
#all: SOURCES = main.cpp
all: build

build:
	$(eval OBJECTS := $(SOURCES:.cpp=.o))
	$(CC) $(INCLUDE) $(CPPFLAGS) -c $(SOURCES) -o $(OBJECTS)
	rm -rf core.*
	$(CC) -o $(TARGET) $(OBJECTS) $(CPPFLAGS) $(LDFLAGS)

clean:
	rm -rf $(TARGET) *.o

.PHONY: all liburcu build clean
