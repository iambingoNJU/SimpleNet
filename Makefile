CC = gcc
CFLAGS = -std=gnu99 -Wall -Werror -pthread -g

SIMPLE_CLIENT_APP_SRC = client_app/app_simple_client.c
SIMPLE_CLIENT_APP_OBJ = $(SIMPLE_CLIENT_APP_SRC:.c=.o)
SIMPLE_CLIENT_APP = simple-client.elf

STRESS_CLIENT_APP_SRC = client_app/app_stress_client.c
STRESS_CLIENT_APP_OBJ = $(STRESS_CLIENT_APP_SRC:.c=.o)
STRESS_CLIENT_APP = stress-client.elf

SIMPLE_SERVER_APP_SRC = server_app/app_simple_server.c
SIMPLE_SERVER_APP_OBJ = $(SIMPLE_SERVER_APP_SRC:.c=.o)
SIMPLE_SERVER_APP := simple-server.elf

STRESS_SERVER_APP_SRC = server_app/app_stress_server.c
STRESS_SERVER_APP_OBJ = $(STRESS_SERVER_APP_SRC:.c=.o)
STRESS_SERVER_APP := stress-server.elf

all: socketlib $(SIMPLE_CLIENT_APP) $(STRESS_CLIENT_APP) $(SIMPLE_SERVER_APP) $(STRESS_SERVER_APP)

include socketlib/Makefile

$(SIMPLE_CLIENT_APP): $(SIMPLE_CLIENT_APP_OBJ) $(STCP_ARCH)
	$(CC) $(CFLAGS) -I$(STCP_ARCH) -L$(libpath) $^ -o $@

$(STRESS_CLIENT_APP): $(STRESS_CLIENT_APP_OBJ) $(STCP_ARCH)
	$(CC) $(CFLAGS) -I$(STCP_ARCH) -L$(libpath) $^ -o $@

$(SIMPLE_SERVER_APP): $(SIMPLE_SERVER_APP_OBJ) $(STCP_ARCH)
	$(CC) $(CFLAGS) -I$(STCP_ARCH) -L$(libpath) $^ -o $@

$(STRESS_SERVER_APP): $(STRESS_SERVER_APP_OBJ) $(STCP_ARCH)
	$(CC) $(CFLAGS) -I$(STCP_ARCH) -L$(libpath) $^ -o $@

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@ 

.PHONY: all clean update

update:
	cd script && ./update-all.sh

clean:
	-rm -f $(COMMON_OBJ)
	-rm -f $(SON_OBJ) $(SON)
	-rm -f $(SIP_OBJ) $(SIP)
	-rm -f $(STCP_OBJ) $(STCP_ARCH)
	-rm -f $(SIMPLE_SERVER_APP) $(SIMPLE_SERVER_APP_OBJ)
	-rm -f $(STRESS_SERVER_APP) $(STRESS_SERVER_APP_OBJ)
	-rm -f $(SIMPLE_CLIENT_APP) $(SIMPLE_CLIENT_APP_OBJ) 
	-rm -f $(STRESS_CLIENT_APP) $(STRESS_CLIENT_APP_OBJ) 
	
	
	
