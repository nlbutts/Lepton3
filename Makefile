.PHONY: clean

# Headers
API_INCLUDES = -Iinclude

# Sources
API_SOURCES = $(wildcard src/*.c)

CC = gcc
CFLAGS = -g -DLOG_USE_COLOR=1 -Wall

main:
	$(CC) $(CFLAGS) -pthread -lzmq -lrt $(API_INCLUDES) src/lepton.c -o bin/lepton

clean:
	@rm -f *.o
	@rm lepton
