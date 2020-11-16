#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "../../src/ae.h"

void fileProc(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
	char buf[4096];

	if (fgets(buf, 4096, stdin) != NULL)
		buf[strlen(buf)-1] = '\0';

	printf("input: %s, file proc executed.\n", buf);
}

int timeProc(aeEventLoop *eventLoop, long long id, void *clientData) {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	printf("id: %lld, now: %ld %d, time proc executed.\n", id, tv.tv_sec, (int)tv.tv_usec/1000);

	return 10 * 1000;
}

int main() {
	aeEventLoop *eventLoop;

	eventLoop = aeCreateEventLoop(10);

	aeCreateFileEvent(eventLoop, STDIN_FILENO, AE_READABLE, fileProc, NULL);

	aeCreateTimeEvent(eventLoop, 10 * 1000, timeProc, NULL, NULL);
	aeCreateTimeEvent(eventLoop, 5 * 1000, timeProc, NULL, NULL);

	aeMain(eventLoop);

	return 0;
}
