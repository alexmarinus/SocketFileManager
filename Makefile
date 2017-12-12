CC=g++
LIBSOCKET=-lnsl
CCFLAGS=-Wall -g
SRV=server
CLT=client

build: $(SRV) $(CLT)

$(SRV):$(SRV).cpp
	$(CC) -o $(SRV) $(LIBSOCKET) $(SRV).cpp

$(CLT):	$(CLT).cpp
	$(CC) -o $(CLT) $(LIBSOCKET) $(CLT).cpp

clean:
	rm -f *.o *~ *.log logfile
	rm -f $(SRV) $(CLT)

