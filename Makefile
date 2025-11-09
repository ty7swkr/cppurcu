CC	= g++

TARGET  = rcu_bench

INCLUDE += -I./

########################################
LDFLAGS = -lrt -lpthread

CPPFLAGS += -g -D_REENTRANT
CPPFLAGS += -O2 -std=c++17 -Wall -Wextra -Wfloat-equal -m64

# bench1 (without liburcu)
all: SOURCES = rcu_bench1.cpp
all: build

# bench2 (with liburcu)
liburcu: SOURCES = rcu_bench2.cpp
#liburcu: LDFLAGS += -lurcu-memb
liburcu: LDFLAGS += -lurcu-mb -lurcu
liburcu: build

build:
	$(eval OBJECTS := $(SOURCES:.cpp=.o))
	$(CC) $(INCLUDE) $(CPPFLAGS) -c $(SOURCES) -o $(OBJECTS)
	rm -rf core.*
	$(CC) -o $(TARGET) $(OBJECTS) $(CPPFLAGS) $(LDFLAGS)

clean:
	rm -rf $(TARGET) *.o

.PHONY: all liburcu build clean
