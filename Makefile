CXXSOURCES = \
./ClassPath/Externals/ebsc/EBSC.cpp \
./ClassPath/src/__version__.cpp \
./ClassPath/src/Compress/BSCLib.cpp \
./ClassPath/src/Compress/ZLib.cpp \
./ClassPath/src/Crypto/AES.cpp \
./ClassPath/src/Crypto/OpenSSLWrap.cpp \
./ClassPath/src/Crypto/Random.cpp \
./ClassPath/src/Crypto/SafeMemclean.cpp \
./ClassPath/src/Database/Database.cpp \
./ClassPath/src/Database/SQLite.cpp \
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
./Frost.cpp \

# find path -name "*.cpp"
CSOURCES = \
ClassPath/src/Strings/BString/bstrlib.c \
ClassPath/src/Compress/easyzlib.c

OUTPUT = build/Frost


ifeq ($(CONFIG),Release)
CXXFLAGS += -O2
CFLAGS += -O2
endif

CXXFLAGS += -pthread -Wno-multichar -std=c++11
CFLAGS += -pthread -Wno-multichar


# Don't touch anything below this line
BUILD_NUMBER_FILE=build/build-number.txt
SRC = $(CXXSOURCES) $(CSOURCES)
OBJ = $(CXXSOURCES:.cpp=.o) $(CSOURCES:.c=.o)
OBJ_CXX_BUILD = $(foreach object, $(CXXSOURCES:.cpp=.o),$(addprefix build/, $(object)))
OBJ_C_BUILD = $(foreach object, $(CSOURCES:.c=.o),$(addprefix build/, $(object)))
OBJ_BUILD = $(OBJ_CXX_BUILD) $(OBJ_C_BUILD)
SYSLD := $(shell echo `whereis g++ | cut -d\  -f 2`)
REALLD ?= $(SYSLD)
SHAREDLIBS = -lssl -lcrypto -lsqlite3
# On Linux, old Glib requires -lrt for aio and clock_gettime
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
   SHAREDLIBS += -lrt
endif
LDXX = g++
Q = @

all: $(OUTPUT)


$(OUTPUT): $(OBJ_BUILD) 
	@echo Linking $@
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


clean:
	-rm $(OBJ_BUILD)
	-rm -r build/ClassPath
	-rm $(OUTPUT)
