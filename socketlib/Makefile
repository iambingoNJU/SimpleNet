
CC := gcc
CFLAGS := -std=gnu99 -Wall -Werror -pthread -g
DST_PATH = ./
libpath := $(shell pwd)
libpath := $(addsuffix /socketlib, $(libpath))

COMMON_SRC := $(wildcard $(libpath)/topology/*.c)
COMMON_SRC += $(wildcard $(libpath)/common/*.c)
COMMON_OBJ := $(COMMON_SRC:.c=.o)

SON_SRC := $(wildcard $(libpath)/son/*.c)
SON_OBJ := $(SON_SRC:.c=.o)
SON := $(DST_PATH)son.elf

SIP_SRC := $(wildcard $(libpath)/sip/*.c)
SIP_OBJ := $(SIP_SRC:.c=.o)
SIP := $(DST_PATH)sip.elf

STCP_SRC := $(wildcard $(libpath)/stcp/*.c)
STCP_OBJ := $(STCP_SRC:.c=.o)
STCP_ARCH := $(libpath)/libstcp.a

.PHONY: socketlib

socketlib: $(SON) $(SIP) $(STCP_ARCH)

$(SON): $(SON_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(SIP): $(SIP_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(STCP_ARCH): $(STCP_OBJ) $(COMMON_OBJ)
	ar -r $@ $^
 	

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@ 

