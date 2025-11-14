# ====================================================================
# 1. Common settings
# ====================================================================

CC_DEFAULT = clang++
#CC_DEFAULT = g++

INCLUDE += -I../
CPPFLAGS += -D_REENTRANT -g -m64
LDFLAGS = -lrt -lpthread

.PHONY: all build clean test tsan lsan

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

test: $(TARGET_TEST)

tsan: $(TARGET_TSAN)

lsan: $(TARGET_LAUSAN)

# ====================================================================
# 4. Individual target rules
# ====================================================================

# ----------------------------------
# 4.1. unit_test
# ----------------------------------
SOURCES_TEST = unit_test.cpp
OBJECTS_TEST = $(SOURCES_TEST:.cpp=.o)
CPPFLAGS_TEST = $(CPPFLAGS) -O2 -std=c++17 -Wall -Wextra -Wfloat-equal

$(TARGET_TEST): $(OBJECTS_TEST)
	@echo "--- Linking $(TARGET_TEST) ---"
	$(CC_DEFAULT) -o $(TARGET_TEST) $(OBJECTS_TEST) $(CPPFLAGS_TEST) $(LDFLAGS)

unit_test.o: unit_test.cpp
	@echo "--- Compiling unit_test.cpp ---"
	$(CC_DEFAULT) $(INCLUDE) $(CPPFLAGS_TEST) -MMD -MP -c $< -o $@

# ----------------------------------
# 4.2. unit_test_tsan
# ----------------------------------
SOURCES_TSAN = unit_test_tsan.cpp
OBJECTS_TSAN = $(SOURCES_TSAN:.cpp=.o)
CPPFLAGS_TSAN = $(CPPFLAGS) -fno-omit-frame-pointer
CPPFLAGS_TSAN += -fsanitize=thread
CPPFLAGS_TSAN += -O1 -std=c++17 -Wall -Wextra -Wfloat-equal

$(TARGET_TSAN): $(OBJECTS_TSAN)
	@echo "--- Linking $(TARGET_TSAN) ---"
	$(CC_DEFAULT) -o $(TARGET_TSAN) $(OBJECTS_TSAN) $(CPPFLAGS_TSAN) $(LDFLAGS)

unit_test_tsan.o: unit_test_tsan.cpp
	@echo "--- Compiling unit_test_tsan.cpp ---"
	$(CC_DEFAULT) $(INCLUDE) $(CPPFLAGS_TSAN) -MMD -MP -c $< -o $@

# ----------------------------------
# 4.3. unit_test_lausan
# ----------------------------------
SOURCES_LAUSAN = unit_test_lausan.cpp
OBJECTS_LAUSAN = $(SOURCES_LAUSAN:.cpp=.o)
CPPFLAGS_LAUSAN = $(CPPFLAGS) -fno-omit-frame-pointer
CPPFLAGS_LAUSAN += -fsanitize=leak -fsanitize=address -fsanitize=undefined
CPPFLAGS_LAUSAN += -O2 -std=c++17 -Wall -Wextra -Wfloat-equal

$(TARGET_LAUSAN): $(OBJECTS_LAUSAN)
	@echo "--- Linking $(TARGET_LAUSAN) ---"
	$(CC_DEFAULT) -o $(TARGET_LAUSAN) $(OBJECTS_LAUSAN) $(CPPFLAGS_LAUSAN) $(LDFLAGS)

unit_test_lausan.o: unit_test_lausan.cpp
	@echo "--- Compiling unit_test_lausan.cpp ---"
	$(CC_DEFAULT) $(INCLUDE) $(CPPFLAGS_LAUSAN) -MMD -MP -c $< -o $@

# ====================================================================
# 5. Clean target
# ====================================================================

clean:
	@echo "--- Cleaning up build files ---"
	rm -rf $(TARGET_TEST) $(TARGET_TSAN) $(TARGET_LAUSAN) *.o *.d core.*

# ====================================================================
# 6. Include dependencies
# ====================================================================

-include $(SOURCES_TEST:.cpp=.d)
-include $(SOURCES_TSAN:.cpp=.d)
-include $(SOURCES_LAUSAN:.cpp=.d)
