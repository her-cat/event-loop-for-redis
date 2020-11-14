#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "ae.h"

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

void finalizerProc(aeEventLoop *eventLoop, void *clientData) {
	printf("finalizer proc executed.\n");
}

int main() {
	int processed;
	aeEventLoop *eventLoop;

	eventLoop = aeCreateEventLoop(10);

	aeCreateFileEvent(eventLoop, STDIN_FILENO, AE_READABLE, fileProc, NULL);

	aeCreateTimeEvent(eventLoop, 10 * 1000, timeProc, NULL, finalizerProc);
	aeCreateTimeEvent(eventLoop, 5 * 1000, timeProc, NULL, finalizerProc);

	while (!eventLoop->stop) {
		processed = aeProcessEvents(eventLoop, AE_ALL_EVENTS);

		printf("processed: %d\n", processed);
	}
}
