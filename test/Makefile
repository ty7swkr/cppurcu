# ====================================================================
# 1. Common settings
# ====================================================================

CC_DEFAULT = clang++
#CC_DEFAULT = g++

INCLUDE += -I../
LDFLAGS = -lrt -lpthread
CPPFLAGS += -D_REENTRANT 
CPPFLAGS += -g -m64

.PHONY: all build clean

# ====================================================================
# 2. Target definitions
# ====================================================================

TARGET_TEST = unit_test
TARGET_TSAN = unit_test_tsan
TARGET_LAUSAN = unit_test_lausan

# ====================================================================
# 3. Main build targets
# ====================================================================

all: build

build: $(TARGET_TEST) $(TARGET_TSAN) $(TARGET_LAUSAN)

# ====================================================================
# 4. Individual target rules
# ====================================================================

# ----------------------------------
# 4.1. unit_test
# ----------------------------------
SOURCES_TEST = unit_test.cpp
CPPFLAGS_TEST = $(CPPFLAGS) -O2 -std=c++17 -Wall -Wextra -Wfloat-equal

$(TARGET_TEST): $(SOURCES_TEST)
	@echo "--- Building $(TARGET_TEST) ---"
	$(eval OBJECTS_TEST := $(SOURCES_TEST:.cpp=.o))
	$(CC_DEFAULT) $(INCLUDE) $(CPPFLAGS_TEST) -c $(SOURCES_TEST) -o $(OBJECTS_TEST)
	$(CC_DEFAULT) -o $(TARGET_TEST) $(OBJECTS_TEST) $(CPPFLAGS_TEST) $(LDFLAGS)

# ----------------------------------
# 4.2. unit_test_tsan
# ----------------------------------
SOURCES_TSAN = unit_test_tsan.cpp
CPPFLAGS_TSAN = $(CPPFLAGS) -fno-omit-frame-pointer
CPPFLAGS_TSAN += -fsanitize=thread
CPPFLAGS_TSAN += -O1 -std=c++17 -Wall -Wextra -Wfloat-equal

$(TARGET_TSAN): $(SOURCES_TSAN)
	@echo "--- Building $(TARGET_TSAN) ---"
	$(eval OBJECTS_TSAN := $(SOURCES_TSAN:.cpp=.o))
	$(CC_DEFAULT) $(INCLUDE) $(CPPFLAGS_TSAN) -c $(SOURCES_TSAN) -o $(OBJECTS_TSAN)
	$(CC_DEFAULT) -o $(TARGET_TSAN) $(OBJECTS_TSAN) $(CPPFLAGS_TSAN) $(LDFLAGS)

# ----------------------------------
# 4.3. unit_test_lausan
# ----------------------------------
SOURCES_LAUSAN = unit_test_lausan.cpp
CPPFLAGS_LAUSAN = $(CPPFLAGS) -fno-omit-frame-pointer
CPPFLAGS_LAUSAN += -fsanitize=leak -fsanitize=address -fsanitize=undefined
CPPFLAGS_LAUSAN += -O2 -std=c++17 -Wall -Wextra -Wfloat-equal

$(TARGET_LAUSAN): $(SOURCES_LAUSAN)
	@echo "--- Building $(TARGET_LAUSAN) ---"
	$(eval OBJECTS_LAUSAN := $(SOURCES_LAUSAN:.cpp=.o))
	$(CC_DEFAULT) $(INCLUDE) $(CPPFLAGS_LAUSAN) -c $(SOURCES_LAUSAN) -o $(OBJECTS_LAUSAN)
	$(CC_DEFAULT) -o $(TARGET_LAUSAN) $(OBJECTS_LAUSAN) $(CPPFLAGS_LAUSAN) $(LDFLAGS)

# ====================================================================
# 5. Clean target
# ====================================================================

clean:
	@echo "--- Cleaning up build files ---"
	rm -rf $(TARGET_TEST) $(TARGET_TSAN) $(TARGET_LAUSAN) *.o core.*