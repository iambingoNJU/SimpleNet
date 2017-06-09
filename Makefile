CC = gcc
CFLAGS = -std=gnu99 -Wall -Werror -pthread -g

CLIENT_APP_SRC = client_app/app_simple_client.c
CLIENT_APP_OBJ = $(CLIENT_APP_SRC:.c=.o)
CLIENT_APP = client.elf

SERVER_APP_SRC = server_app/app_simple_server.c
SERVER_APP_OBJ = $(SERVER_APP_SRC:.c=.o)
SERVER_APP := server.elf

all: socketlib $(CLIENT_APP) $(SERVER_APP)

include socketlib/Makefile

$(CLIENT_APP): $(CLIENT_APP_OBJ) $(STCP_ARCH)
	$(CC) $(CFLAGS) -I$(STCP_ARCH) -L$(libpath) $^ -o $@

$(SERVER_APP): $(SERVER_APP_OBJ) $(STCP_ARCH)
	$(CC) $(CFLAGS) -I$(STCP_ARCH) -L$(libpath) $^ -o $@

.PHONY: all clean

update: script/*
	cd script && ./update-all.sh
clean:
	-rm -f $(COMMON_OBJ)
	-rm -f $(SON_OBJ) $(SON)
	-rm -f $(SIP_OBJ) $(SIP)
	-rm -f $(STCP_OBJ) $(STCP_ARCH)
	-rm -f $(SERVER_APP) $(SERVER_APP_OBJ)
	-rm -f $(CLIENT_APP) $(CLIENT_APP_OBJ) 
	
	
	
