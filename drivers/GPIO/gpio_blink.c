#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(const char* prog)
{
    printf("Usage %s <gpiochip> <line> <delay_ms>",prog);
}
int main