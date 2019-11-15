CC=gcc
CFLAGS=-W -Wall -g
LDFLAGS=-lmysqlclient -lbcm2835
EXEC=weather

all: $(EXEC)

weather: weather.o TX20.o
	$(CC) -o weather weather.o TX20.o $(LDFLAGS)

weather.o: weather.c TX20.h
	$(CC) -o weather.o -c weather.c $(CFLAGS)

TX20.o: TX20.c TX20.h
	$(CC) -o TX20.o -c TX20.c $(CFLAGS)

clean:
	rm -rf *.o

mrproper: clean
	rm -rf $(EXEC)
