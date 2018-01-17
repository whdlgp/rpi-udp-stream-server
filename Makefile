BIN = rpi_stream_server

CC = gcc
CFLAGS = -DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS \
		-DTARGET_POSIX -D_LINUX -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE \
		-D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -DHAVE_LIBOPENMAX=2 -DOMX \
		-DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX \
		-DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -fPIC \
		-ftree-vectorize -pipe -Werror -g -Wall
LDFLAGS = -L/opt/vc/lib -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread
INCLUDES = -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads \
		-I/opt/vc/include/interface/vmcs_host/linux

VPATH = ./openmax ./app ./udp_setup ./common_util

SRC = $(OPENMAX_SRC) $(APP_SRC) $(UDP_SRC) $(COMMON_UTIL_SRC)

OPENMAX_DIR = ./openmax
OPENMAX_SRC = $(notdir $(wildcard $(OPENMAX_DIR)/*.cpp))

APP_DIR = ./app
APP_SRC = $(notdir $(wildcard $(APP_DIR)/*.cpp))

UDP_DIR = ./udp_setup
UDP_SRC = $(notdir $(wildcard $(UDP_DIR)/*.cpp))

COMMON_UTIL_DIR = ./common_util
COMMON_UTIL_SRC = $(notdir $(wildcard $(COMMON_UTIL_DIR)/*.cpp))

OBJ_DIR = ./objs
OBJS = $(addprefix $(OBJ_DIR)/,$(SRC:.cpp=.o))

all: $(BIN) $(SRC)

$(OBJ_DIR)/%.o: %.cpp
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ -Wno-deprecated-declarations
	@echo "make obj file $@ with $<"

$(BIN): $(OBJS)
	@$(CC) -o $@ -Wl,--whole-archive $(OBJS) $(LDFLAGS) -Wl,--no-whole-archive -rdynamic
	@echo "make excutable file $@"

.PHONY: clean rebuild

clean:
	rm -f $(BIN) $(OBJ_DIR)/*.o

rebuild:
	make clean && make
