# The recommended compiler flags for the Raspberry Pi
CCFLAGS=

all: gettingstarted pingpair_dyn scanner \
	gettingstarted_call_response rpi-hub transfer

gettingstarted: RF24.o bcm2835.o gettingstarted.o	
	g++ ${CCFLAGS} -o $@ $^

pingpair_dyn: RF24.o bcm2835.o pingpair_dyn.o
	g++ ${CCFLAGS} -o $@ $^

scanner: RF24.o bcm2835.o scanner.o
	g++ ${CCFLAGS} -o $@ $^

gettingstarted_call_response: \
	RF24.o bcm2835.o gettingstarted_call_response.o	
	g++ ${CCFLAGS} -o $@ $^

rpi-hub: RF24.o bcm2835.o rpi-hub.o
	g++ ${CCFLAGS} -o $@ $^

transfer: RF24.o bcm2835.o transfer.o
	g++ ${CCFLAGS} -o $@ $^

# Library parts
RF24.o: RF24.cpp
	g++ -Wall ${CCFLAGS} -c $^

bcm2835.o: bcm2835.c
	gcc -Wall ${CCFLAGS} -c $^

gettingstarted.o: gettingstarted.cpp
	gcc -Wall ${CCFLAGS} -c $^

pingpair_dyn.o: pingpair_dyn.cpp
	gcc -Wall ${CCFLAGS} -c $^

scanner.o: scanner.cpp
	gcc -Wall ${CCFLAGS} -c $^

gettingstarted_call_response.o: \
	gettingstarted_call_response.cpp
	gcc -Wall ${CCFLAGS} -c $^

rpi-hub.o: rpi-hub.cpp
	g++ -Wall ${CCFLAGS} -c $^

transfer.o: transfer.cpp
	g++ -Wall ${CCFLAGS} -c $^

# clear build files
clean:
	rm -rf *.o gettingstarted pingpair_dyn rpi-hub \
		gettingstarted_call_response scanner transfer
