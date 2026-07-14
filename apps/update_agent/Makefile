CC := ~/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-gcc
CFLAGS := -Wall -O2 -Wextra

all: update_agent

update_agent: main.c
	$(CC) $(CFLAGS) main.c -o update_agent

clean:
	rm -f update_agent

	