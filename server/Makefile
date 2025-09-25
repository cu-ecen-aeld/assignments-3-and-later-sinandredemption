default: aesdsocket.c
	${CC} -g -pthread aesdsocket.c -o aesdsocket ${LDFLAGS}

all: default

clean:
	rm -f aesdsocket
	rm -f /var/tmp/aesdsocketdata
