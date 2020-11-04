
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include "ae.h"

typedef struct aeApiState {
	fd_set rfds, wfds; /* 存放需要记录的读写文件描述符集合。 */
	fd_set _rfds, _wfds; /* 上面文件描述符集合的副本，用于传给 select() 函数。 */
} aeApiState;

static int aeApiCreate(aeEventLoop *eventLoop) {
	aeApiState *state = malloc(sizeof(aeApiState));

	if (!state) return -1;
	/* 初始化文件描述符集合 */
	FD_ZERO(&state->rfds);
	FD_ZERO(&state->wfds);
	eventLoop->apidata = state;
	return 0;
}

static int aeApiResize(aeEventLoop *eventLoop, int setsize) {
	/* 确保有足够的空间 */
	if (setsize >= FD_SETSIZE) return -1;
	return 0;
}

static void aeApiFree(aeEventLoop *eventLoop) {
	free(eventLoop->apidata);
}

static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask) {
	aeApiState *state = eventLoop->apidata;

	if (mask & AE_READABLE) FD_SET(fd, &state->rfds);
	if (mask & AE_WRITABLE) FD_SET(fd, &state->wfds);
	return 0;
}

static void aeApiDelEvent(aeEventLoop *eventLoop, int fd, int mask) {
	aeApiState *state = eventLoop->apidata;

	if (mask & AE_READABLE) FD_CLR(fd, &state->rfds);
	if (mask & AE_WRITABLE) FD_CLR(fd, &state->wfds);
}

static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
	aeApiState *state = eventLoop->apidata;
	int retval, j, numevents = 0;

	/* 当有事件发生时，select 会将发生事件的描述符拷贝到 _rfds/_wfds 上覆盖掉传入的值，
	 * 所以这里将读写集合副本传给 select */
	memcpy(&state->_rfds, &state->rfds, sizeof(fd_set));
	memcpy(&state->_wfds, &state->wfds, sizeof(fd_set));

	retval = select(eventLoop->maxfd+1, &state->_rfds, &state->_wfds, NULL, tvp);

	if (retval > 0) {
		for (int j = 0; j <= eventLoop->maxfd; j++) {
			int mask = 0;
			aeFileEvent *fe = &eventLoop->events[j];

			if (fe->mask == AE_NONE) continue;
			/* 当文件事件设置了 AE_READABLE/AE_WRITABLE 时，并且对应的fd集合也有该文件描述符 */
			if (fe->mask & AE_READABLE && FD_ISSET(j, &state->_rfds))
				mask |= AE_READABLE;
			if (fe->mask & AE_WRITABLE && FD_ISSET(j, &state->_wfds))
				mask |= AE_WRITABLE;
			/* 将文件描述符设置到被触发的事件集合中，后续进行处理 */
			eventLoop->fired[numevents].fd = j;
			eventLoop->fired[numevents].mask = mask;
			numevents++;
		}
	}

	return numevents;
}

static char *aeApiName(void) {
	return "select";
}

