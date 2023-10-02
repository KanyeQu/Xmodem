PORT=53862
CFLAGS= -DPORT=\$(PORT) -g -Wall
DEPENDENCIES = xmodemserver.h crc16.h 

all : xmodemserver client

xmodemserver : crc16.o xmodemserver.o 
	gcc ${CFLAGS} -o $@ $^

client : crc16.o client1.o 
	gcc ${CFLAGS} -o $@ $^

%.o : %.c ${DEPENDENCIES}
	gcc ${CFLAGS} -c $<

clean : 
	rm -f *.o xmodemserver client