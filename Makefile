#CC      = gcc
CFLAGS += -std=c99 # Define which version of the C standard to use
CFLAGS += -Wall # Enable the 'all' set of warnings
CFLAGS += -Werror # Treat all warnings as error
CFLAGS += -Wshadow # Warn when shadowing variables
CFLAGS += -Wextra # Enable additional warnings
CFLAGS += -O2 -D_FORTIFY_SOURCE=2 # Add canary code, i.e. detect buffer overflows
CFLAGS += -fstack-protector-all # Add canary code to detect stack smashing

LDFLAGS+= -lz

# We have no libraries to link against except libc, but we want to keep
# the symbols for debugging
LDFLAGS+= -rdynamic

all: clean sender receiver

#SEND_OBJS= src/sender.c src/packet_implem.c src/create_socket.c src/read_write_loop.c src/real_address.c src/wait_for_client.c
#REC_OBJS = src/receiver.c src/packet_implem.c src/create_socket.c src/read_write_loop.c src/real_address.c src/wait_for_client.c


sender: sender.o packet_implem.o create_socket.o read_write_loop.o read_write_loop.o real_address.o wait_for_client.o

receiver: receiver.o packet_implem.o create_socket.o read_write_loop.o read_write_loop.o real_address.o wait_for_client.o

sender.o: src/sender.c src/packet_implem.c src/create_socket.c src/read_write_loop.c src/real_address.c src/wait_for_client.c
			gcc -c src/sender.c -o sender.o

receiver.o: src/receiver.c src/packet_implem.c src/create_socket.c src/read_write_loop.c src/real_address.c src/wait_for_client.c
			gcc -c src/receiver.c -o receiver.o

packet_implem.o: src/packet_implem.c
			gcc -c src/packet_implem.c -o packet_implem.o

create_socket.o: src/create_socket.c
			gcc -c src/create_socket.c -o create_socket.o

read_write_loop.o: src/read_write_loop.c
			gcc -c src/read_write_loop.c -o read_write_loop.o

real_address.o: src/real_address.c
			gcc -c src/real_address.c -o real_address.o

wait_for_client.o: src/wait_for_client.c
			gcc -c src/wait_for_client.c -o wait_for_client.o


#sender:
#	${CC} ${CFLAGS} ${SEND_OBJS} -o sender ${LDFLAGS}

#receiver:
#	${CC} ${CFLAGS} ${REC_OBJS} -o receiver ${LDFLAGS}



clean:
	@rm -f receiver sender *.o
