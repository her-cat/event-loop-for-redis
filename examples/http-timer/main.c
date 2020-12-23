#include <stdio.h>
#include "../../src/ae/ae.h"

#define URL_MAX_SIZE 1024

static aeEventLoop *eventLoop;

int main(void) {
    FILE *fp;
    char buf[URL_MAX_SIZE];

    eventLoop = aeCreateEventLoop(URL_MAX_SIZE);

    fp = fopen("./urls.txt", "r");
    if (fp == NULL) {
        perror("can't open url.txt");
        return 0;
    }

    while (fgets(buf, URL_MAX_SIZE, fp)) {
        puts(buf);
    }


    return 0;
}
