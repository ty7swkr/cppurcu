# ====================================================================
# 1. Common settings
# ====================================================================

CC_DEFAULT = clang++
#CC_DEFAULT = g++

INCLUDE += -I../
CPPFLAGS += -D_REENTRANT -g -m64 -O2 -std=c++17 -Wall -Wextra -Wdeprecated-declarations -Wfloat-equal -fno-omit-frame-pointer
LDFLAGS = -lrt -lpthread

.PHONY: all build clean test tsan lsan guard_pack

# ====================================================================
# 2. Target definitions
# ====================================================================

TARGET_TEST = unit_test
TARGET_TSAN = unit_test_tsan
TARGET_LAUSAN = unit_test_lausan
TARGET_GUARD_PACK = unit_test_guard_pack

# ====================================================================
# 3. Main build targets
# ====================================================================

all: build

build: $(TARGET_TEST) $(TARGET_TSAN) $(TARGET_LAUSAN) $(TARGET_GUARD_PACK)

test: $(TARGET_TEST)

tsan: $(TARGET_TSAN)

lsan: $(TARGET_LAUSAN)

guard_pack: $(TARGET_GUARD_PACK)

# ====================================================================
# 4. Individual target rules
# ====================================================================

# ----------------------------------
# 4.1. unit_test
# ----------------------------------
SOURCES_TEST = unit_test.cpp
OBJECTS_TEST = $(SOURCES_TEST:.cpp=.o)
CPPFLAGS_TEST = $(CPPFLAGS)

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
CPPFLAGS_TSAN = $(CPPFLAGS) -fsanitize=thread

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
CPPFLAGS_LAUSAN = $(CPPFLAGS) -fsanitize=leak -fsanitize=address -fsanitize=undefined

$(TARGET_LAUSAN): $(OBJECTS_LAUSAN)
	@echo "--- Linking $(TARGET_LAUSAN) ---"
	$(CC_DEFAULT) -o $(TARGET_LAUSAN) $(OBJECTS_LAUSAN) $(CPPFLAGS_LAUSAN) $(LDFLAGS)

unit_test_lausan.o: unit_test_lausan.cpp
	@echo "--- Compiling unit_test_lausan.cpp ---"
	$(CC_DEFAULT) $(INCLUDE) $(CPPFLAGS_LAUSAN) -MMD -MP -c $< -o $@

# ----------------------------------
# 4.4. guard_pack
# ----------------------------------
SOURCES_GUARD_PACK = unit_test_guard_pack.cpp
OBJECTS_GUARD_PACK = $(SOURCES_GUARD_PACK:.cpp=.o)
CPPFLAGS_GUARD_PACK = $(CPPFLAGS) -fsanitize=leak -fsanitize=address -fsanitize=undefined

$(TARGET_GUARD_PACK): $(OBJECTS_GUARD_PACK)
	@echo "--- Linking $(TARGET_GUARD_PACK) ---"
	$(CC_DEFAULT) -o $(TARGET_GUARD_PACK) $(OBJECTS_GUARD_PACK) $(CPPFLAGS_GUARD_PACK) $(LDFLAGS)

unit_test_guard_pack.o: unit_test_guard_pack.cpp
	@echo "--- Compiling unit_test_guard_pack.cpp ---"
	$(CC_DEFAULT) $(INCLUDE) $(CPPFLAGS_GUARD_PACK) -MMD -MP -c $< -o $@

# ====================================================================
# 5. Clean target
# ====================================================================

clean:
	@echo "--- Cleaning up build files ---"
	rm -rf $(TARGET_TEST) $(TARGET_TSAN) $(TARGET_LAUSAN) $(TARGET_GUARD_PACK) *.o *.d core.*

# ====================================================================
# 6. Include dependencies
# ====================================================================

-include $(SOURCES_TEST:.cpp=.d)
-include $(SOURCES_TSAN:.cpp=.d)
-include $(SOURCES_LAUSAN:.cpp=.d)
