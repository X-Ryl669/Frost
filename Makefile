CXXSOURCES = \
./ClassPath/Externals/ebsc/EBSC.cpp \
./ClassPath/src/__version__.cpp \
./ClassPath/src/Compress/BSCLib.cpp \
./ClassPath/src/Compress/ZLib.cpp \
./ClassPath/src/Crypto/AES.cpp \
./ClassPath/src/Crypto/OpenSSLWrap.cpp \
./ClassPath/src/Crypto/Random.cpp \
./ClassPath/src/Crypto/SafeMemclean.cpp \
./ClassPath/src/Encoding/Encode.cpp \
./ClassPath/src/File/BaseChunker.cpp \
./ClassPath/src/File/File.cpp \
./ClassPath/src/File/TTTDChunker.cpp \
./ClassPath/src/Hash/HashKey.cpp \
./ClassPath/src/Hashing/Adler32.cpp \
./ClassPath/src/Hashing/SHA1.cpp \
./ClassPath/src/Hashing/SHA256.cpp \
./ClassPath/src/Logger/Logger.cpp \
./ClassPath/src/Platform/Linux.cpp \
./ClassPath/src/Streams/Streams.cpp \
./ClassPath/src/Strings/BString/bstrwrap.cpp \
./ClassPath/src/Strings/Strings.cpp \
./ClassPath/src/Threading/Lock.cpp \
./ClassPath/src/Threading/Threads.cpp \
./ClassPath/src/Time/Time.cpp \
./ClassPath/src/Utils/Dump.cpp \
./ClassPath/src/Utils/MemoryBlock.cpp \
./ClassPath/src/Variant/UTIImpl.cpp \


# find path -name "*.cpp"
MAINCXX = \
./Frost.cpp \

CSOURCES = \
ClassPath/src/Strings/BString/bstrlib.c \
ClassPath/src/Compress/easyzlib.c

OUTPUT = build/Frost


ifeq ($(CONFIG),Release)
CXXFLAGS += -O2
CFLAGS += -O2
endif

ifeq ($(CONFIG),Debug)
CXXFLAGS += -g -O0
CFLAGS += -g -O0
endif


CXXFLAGS += -pthread -Wno-multichar -std=c++11 -DCONSOLE=1 -D_FILE_OFFSET_BITS=64 -DDEBUG=1 -DHasClassPathConfig=1 -DWantSSLCode=1 -DWantAES=1 -DWantMD5Hashing=1 -DWantThreadLocalStorage=1 -DWantBaseEncoding=1 -DWantFloatParsing=1 -DWantRegularExpressions=1 -DWantTimedProfiling=1 -DWantAtomicClass=1 -DWantExtendedLock=1 -DWantCompression=1 -DWantBSCCompression=1 -DDontWantUPNPC=1
CFLAGS += -pthread -Wno-multichar


# Don't touch anything below this line
BUILD_NUMBER_FILE=build/build-number.txt
SRC = $(CXXSOURCES) $(CSOURCES)
OBJ = $(CXXSOURCES:.cpp=.o) $(CSOURCES:.c=.o)
OBJ_CXX_BUILD = $(foreach object, $(CXXSOURCES:.cpp=.o),$(addprefix build/, $(object)))
OBJ_C_BUILD = $(foreach object, $(CSOURCES:.c=.o),$(addprefix build/, $(object)))
MAIN_OBJ_BUILD = $(foreach object, $(MAINCXX:.cpp=.o),$(addprefix build/, $(object)))
OBJ_BUILD = $(OBJ_CXX_BUILD) $(OBJ_C_BUILD) $(MAIN_OBJ_BUILD)
SYSLD := $(shell echo `whereis g++ | cut -d\  -f 2`)
REALLD ?= $(SYSLD)
SHAREDLIBS = -lssl -lcrypto
# On Linux, old Glib requires -lrt for aio and clock_gettime, and -ldl for dlclose
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
   SHAREDLIBS += -lrt -ldl
endif
ifeq ($(UNAME_S),Darwin)
   CXXFLAGS += -I/opt/local/include
endif
LDXX = g++
Q = @

all: $(OUTPUT)


$(OUTPUT): $(OBJ_BUILD)
	@echo "Linking $@"
	$(Q)$(LDXX) -o $@ -lpthread $(LIBPATH) $(OBJ_BUILD) $(LIBS) $(SHAREDLIBS)
	@echo Built Frost version: $$(cat $(BUILD_NUMBER_FILE))

$(BUILD_NUMBER_FILE): $(OBJ_BUILD)
	@if ! test -f $(BUILD_NUMBER_FILE); then echo 0 > $(BUILD_NUMBER_FILE); fi
	@echo $$(($$(cat $(BUILD_NUMBER_FILE)) + 1)) > $(BUILD_NUMBER_FILE)

$(OBJ_C_BUILD): $(CSOURCES)
	@echo ">  Compiling $(notdir $*.c)"
	$(Q)-mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) $(DFLAGS) -c $(subst build/,,$(patsubst %.o,%.c,$@)) -o $@

$(OBJ_CXX_BUILD): $(CXXSOURCES)
	@echo ">  Compiling $(notdir $*.cpp)"
	$(Q)-mkdir -p $(dir $@)
	$(Q)$(CXX) $(CXXFLAGS) $(DFLAGS) $(INCPATH) -c $(subst build/,,$(patsubst %.o,%.cpp,$@)) -o $@

$(MAIN_OBJ_BUILD): $(CXXSOURCES) $(MAINCXX)
	@echo ">  Compiling $(notdir $*.cpp)"
	$(Q)-mkdir -p $(dir $@)
	$(Q)$(CXX) $(CXXFLAGS) $(DFLAGS) $(INCPATH) -c $(subst build/,,$(patsubst %.o,%.cpp,$@)) -o $@

Frostfuse: build/Frostfuse


build/Frostfuse: build/Frostfuse.o
	@echo "Linking $@"
	$(Q)$(LDXX) -o $@ -lpthread $(LIBPATH) $(OBJ_CXX_BUILD) $(OBJ_C_BUILD) build/Frostfuse.o $(LIBS) $(SHAREDLIBS) `pkg-config fuse --libs`

build/Frostfuse.o: ./Frost.cpp ./Frost.hpp $(CXXSOURCES)
	@echo ">  Compiling $(notdir $*.cpp)"
	$(Q)-mkdir -p $(dir $@)
	$(Q)$(CXX) $(CXXFLAGS) $(DFLAGS) -DWithFUSE=1 `pkg-config fuse --cflags` $(INCPATH) -c ./Frost.cpp -o $@

clean:
	-rm $(OBJ_BUILD)
	-rm -r build/ClassPath
	-rm $(OUTPUT)
	-rm build/Frostfuse.o
	-rm build/Frostfuse
