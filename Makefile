
CC := gcc
CFLAGS := -std=gnu99 -Wall -Werror -pthread -g

COMMON_SRC := $(wildcard topology/*.c)
COMMON_SRC += $(wildcard common/*.c)
COMMON_OBJ := $(COMMON_SRC:.c=.o)

SON_SRC := $(wildcard son/*.c)
SON_OBJ := $(SON_SRC:.c=.o)
SON := son.elf

SIP_SRC := $(wildcard sip/*.c)
SIP_OBJ := $(SIP_SRC:.c=.o)
SIP := sip.elf

CLIENT_SRC := client/stcp_client.c
CLIENT_SRC += client/app_simple_client.c
CLIENT_OBJ := $(CLIENT_SRC:.c=.o)
CLIENT := client.elf

SERVER_SRC := server/stcp_server.c
SERVER_SRC += server/app_simple_server.c
SERVER_OBJ := $(SERVER_SRC:.c=.o)
SERVER := server.elf

.PHONY: all clean

all: $(SON) $(SIP) $(STCP)

$(SON): $(SON_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(SIP): $(SIP_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(CLIENT): $(CLIENT_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(SERVER): $(SERVER_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@ 

clean:
	-rm -f $(COMMON_OBJ)
	-rm -f $(SON_OBJ) $(SON)
	-rm -f $(SIP_OBJ) $(SIP)
	-rm -f $(CLIENT_OBJ) $(CLIENT)
	-rm -f $(SERVER_OBJ) $(SERVER)

