USE_AESD_CHAR_DEVICE ?= 1
default: aesdsocket.c
	${CC} -g -Wall -pthread aesdsocket.c -o aesdsocket ${LDFLAGS} -DUSE_AESD_CHAR_DEVICE=${USE_AESD_CHAR_DEVICE}

all: default

clean:
	rm -f aesdsocket
	rm -f /var/tmp/aesdsocketdata
