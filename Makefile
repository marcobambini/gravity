PKG_CONFIG = PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1 pkg-config
COMPILER_DIR = src/compiler/
RUNTIME_DIR = src/runtime/
SHARED_DIR = src/shared/
UTILS_DIR = src/utils/
OPT_DIR = src/optionals/
GRAVITY_SRC = src/cli/gravity.c

DOCKER_DIR = docker
DOCKERFILE_DIRS = $(notdir $(wildcard $(DOCKER_DIR)/*))

CC ?= gcc
SRC = $(wildcard $(COMPILER_DIR)*.c) \
      $(wildcard $(RUNTIME_DIR)/*.c) \
      $(wildcard $(SHARED_DIR)/*.c) \
      $(wildcard $(UTILS_DIR)/*.c) \
      $(wildcard $(OPT_DIR)/*.c)

OPENSSL_ENABLED := true
OPENSSL_INSTALLED = $(shell $(PKG_CONFIG) --exists openssl && echo true || echo false)
$(info OPENSSL_ENABLED: $(OPENSSL_ENABLED))
$(info OPENSSL_INSTALLED: $(OPENSSL_INSTALLED))
ifeq ($(OPENSSL_ENABLED),true)
	ifeq ($(OPENSSL_INSTALLED),false)
		$(error OpenSSL is enabled, but not installed)
	endif
endif
INCLUDE = -I$(COMPILER_DIR) -I$(RUNTIME_DIR) -I$(SHARED_DIR) -I$(UTILS_DIR) -I$(OPT_DIR)
CFLAGS = $(INCLUDE) -std=gnu99 -fgnu89-inline -fPIC -DBUILD_GRAVITY_API
OBJ = $(SRC:.c=.o)

ifeq ($(OS),Windows_NT)
	# Windows
	LIBTARGET = gravity.dll
	LDFLAGS = -lm -lShlwapi
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		# MacOS
		LIBTARGET = libgravity.dylib
		LDFLAGS = -lm
		ifeq ($(OPENSSL_ENABLED),true)
			CFLAGS += $(shell $(PKG_CONFIG) --cflags openssl) -DGRAVITY_OPENSSL_ENABLED
			LDFLAGS += $(shell $(PKG_CONFIG) --libs openssl)
		endif
	else ifeq ($(UNAME_S),OpenBSD)
		# OpenBSD
		CFLAGS += -D_WITH_GETLINE
		LDFLAGS = -lm
	else ifeq ($(UNAME_S),FreeBSD)
		# FreeBSD
		CFLAGS += -D_WITH_GETLINE
		LDFLAGS = -lm
	else ifeq ($(UNAME_S),NetBSD)
		# NetBSD
		CFLAGS += -D_WITH_GETLINE
		LDFLAGS = -lm
	else ifeq ($(UNAME_S),DragonFly)
		# DragonFly
		CFLAGS += -D_WITH_GETLINE
		LDFLAGS = -lm
	else
		# Linux
		LIBTARGET = libgravity.so
		LDFLAGS = -lm -lrt
		ifeq ($(OPENSSL_ENABLED),true)
			CFLAGS += $(shell $(PKG_CONFIG) --cflags openssl) -DGRAVITY_OPENSSL_ENABLED
			LDFLAGS += $(shell $(PKG_CONFIG) --libs openssl)
		endif
	endif
endif

ifeq ($(mode),debug)
	CFLAGS += -g -O0 -DDEBUG
else
	CFLAGS += -O2
endif

all: gravity

gravity:	$(OBJ) $(GRAVITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: all clean gravity

lib: gravity
	$(CC) -shared -o $(LIBTARGET) $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ) gravity libgravity.so gravity.dll

docker.%.base:
	docker build --no-cache -t gravity-$*-base -f $(DOCKER_DIR)/$*/base/Dockerfile .

docker.build:
	docker-compose build

docker.up: docker.$(DOCKERFILE_DIRS).base docker.build
	$(info building gravity on: $(DOCKERFILE_DIRS))
	docker-compose up

docker.down:
	docker-compose down
	docker-compose stop
	docker-compose rm