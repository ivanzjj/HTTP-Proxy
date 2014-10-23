cc = gcc

default : remote_server local_listener

remote_server : remote_server.o
	cc -o remote_server remote_server.o
remote_server.o :	remote_server.c
	cc -c remote_server.c

local_listener : local_listener.o
	cc -o local_listener local_listener.o
local_listener.o : local_listener.c
	cc -c local_listener.c

.PHONY : clean default

clean :
	-rm local_listener.o remote_server.o

