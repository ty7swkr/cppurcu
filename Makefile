CC      = g++
#CC     = clang++-10

TARGET		= rcu_bench
SOURCES	= rcu_bench1.cpp

ifeq ($(MAKECMDGOALS),liburcu)
	SOURCES = rcu_bench2.cpp
endif

INCLUDE =  -I./

CPPFLAGS += -g -D_REENTRANT
CPPFLAGS  +=  -O2 -std=c++17 -Wall -Wextra -Wfloat-equal -m64

OBJECTS :=  $(SOURCES:.cpp=.o)
DEPENDS :=  $(SOURCES:.cpp=.d)

ifeq ($(MAKECMDGOALS),liburcu)
	LDFLAGS += -lurcu-mb -lurcu
endif
LDFLAGS += -lrt -lpthread

all: $(DEPENDS) $(OBJECTS)
	rm -rf core.*
	$(CC) -o $(TARGET) $(OBJECTS) $(CPPFLAGS) $(LDFLAGS)

clean:
	rm -rf $(TARGET) rcu_bench?.o

.c.o: $(.cpp.o)
.cpp.o:
	$(CC) $(INCLUDE) $(CPPFLAGS) -c $< -o $@
  
%d:%cpp
	$(CC) $(INCLUDE) $(CPPFLAGS) -MM -MP -MT "$(@:.d=.o) $@" -MF $@ $<

.PHONY: all

liburcu: all

-include $(DEPENDS)
