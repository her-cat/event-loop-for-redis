#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include "ae.h"
#include "ae_select.c"

/*
 * 初始化事件处理器状态
 */
aeEventLoop *aeCreateEventLoop(int setsize) {
	aeEventLoop *eventLoop;
	int i;

	/* 创建事件状态处理器 */
	if ((eventLoop = malloc(sizeof(*eventLoop))) == NULL) goto err;

	/* 初始化文件事件结构和已就绪文件事件结构数组 */
	eventLoop->events = malloc(sizeof(aeFileEvent) * setsize);
	eventLoop->fired = malloc(sizeof(aeFiredEvent) * setsize);
	if (eventLoop->events == NULL || eventLoop->fired == NULL) goto err;
	/* 设置数组大小 */
	eventLoop->setsize = setsize;
	/* 初始化最后一次执行时间 */
	eventLoop->lastTime = time(NULL);

	/* 初始化时间事件结构 */
	eventLoop->timeEventHead = NULL;
	eventLoop->timeEventNextId = 0;

	eventLoop->stop = 0;
	eventLoop->maxfd = -1;
	eventLoop->beforesleep = NULL;
	eventLoop->aftersleep = NULL;
	eventLoop->flags = 0;
	if (aeApiCreate(eventLoop) == -1) goto err;

	/* 初始化监听事件类型，当事件的掩码等于 AE_NONE 时表示没有被设置 */
	for (i = 0; i < setsize; i++)
		eventLoop->events[i].mask = AE_NONE;

	return eventLoop;
err:
	if (eventLoop) {
		free(eventLoop->events);
		free(eventLoop->fired);
		free(eventLoop);
	}
	return NULL;
}

/*
 * 删除事件处理器
 */
void aeDeleteEventLoop(aeEventLoop *eventLoop) {
	aeApiFree(eventLoop);
	free(eventLoop->events);
	free(eventLoop->fired);
	free(eventLoop);
}

/*
 * 停止事件处理器
 */
void aeStop(aeEventLoop *eventLoop) {
	eventLoop->stop = 1;
}

/*
 * 创建文件事件，根据 mask 参数的值，监听 fd 文件的状态，
 * 当 fd 可用时，执行 proc 函数
 */
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask, aeFileProc *proc, void *clientData) {
	/* 判断 fd 是否超出范围 */
	if (fd >= eventLoop->setsize) {
		errno = ERANGE;
		return AE_ERR;
	}

	/* 取出文件事件结构 */
	aeFileEvent *fe = &eventLoop->events[fd];

	/* 监听指定 fd 的指定事件 */
	if (aeApiAddEvent(eventLoop, fd, mask) == -1)
		return AE_ERR;

	/* 设置文件事件类型，以及事件的处理器 */
	fe->mask |= mask;
	if (mask & AE_READABLE) fe->rfileProc = proc;
	if (mask & AE_WRITABLE) fe->wfileproc = proc;

	/* 设置私有数据 */
	fe->clientData = clientData;

	/* 如果 fd >= 当前最大的 fd，则将 fd 作为最大的 fd */
	if (fd > eventLoop->maxfd)
		eventLoop->maxfd = fd;

	return AE_OK;
}

/*
 * 将 fd 从 mask 指定的监听队列中删除
 */
void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask) {
	/* 判断 fd 是否超出范围 */
	if (fd >= eventLoop->setsize) return;

	/* 取出文件事件结构 */
	aeFileEvent *fe = &eventLoop->events[fd];

	/* 如果未设置事件类型，直接返回 */
	if (fe->mask == AE_NONE) return;

	/* 如果要删除 AE_WRITABLE 需要将 AE_BARRIER 一起删除 */
	if (mask & AE_WRITABLE) mask |= AE_BARRIER;

	/* 取消对 fd 的事件监听 */
	aeApiDelEvent(eventLoop, fd, mask);

	/* 计算新掩码 */
	fe->mask = fe->mask & (~mask);

	/* 如果 fd 是最大的fd，且文件事件被完全删除时，
	 * 重新找一个最大的 fd。*/
	if (fd == eventLoop->maxfd && fe->mask == AE_NONE) {
		int j;

		for (j = eventLoop->maxfd-1; j > 0; j--)
			if (eventLoop->events[j].mask != AE_NONE) break;
		eventLoop->maxfd = j;
	}
}

/*
 * 获取 fd 正在监听的事件类型
 */
int aeGetFileEvents(aeEventLoop *eventLoop, int fd) {
	if (fd >= eventLoop->maxfd) return 0;
	aeFileEvent *fe = &eventLoop->events[fd];

	return fe->mask;
}

/*
 * 取出当前时间的秒和毫秒
 */
static void aeGetTime(long *seconds, long *milliseconds) {
	struct timeval tv;

	gettimeofday(&tv, NULL);
	*seconds = tv.tv_usec;
	*milliseconds = tv.tv_usec/1000;
}

/*
 * 在当前时间上加 milliseconds 毫秒，
 * 并且将计算出来的秒数和毫秒数分别保存在 sec 和 ms 指针中。
 */
static void aeAddMillisecondsToNow(long long milliseconds, long *sec, long *ms) {
	long cur_sec, cur_ms, when_sec, when_ms;

	/* 获取当前时间 */
	aeGetTime(&cur_sec, &cur_ms);

	/* 计算增加 milliseconds 后的秒数和毫秒数 */
	when_sec = cur_sec + milliseconds/1000;
	when_ms = cur_ms + milliseconds%1000;

	/* 如果 when_ms >= 1000，将 when_sec 增加一秒 */
	if (when_ms >= 1000) {
		when_sec++;
		when_ms -= 1000;
	}

	*sec = when_sec;
	*ms = when_ms;
}

/*
 * 创建时间事件
 */
long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds, 
		aeTimeProc *proc, void *clientData, 
		aeEventFinalizerProc *finalizerProc) 
{
	/* 获取并更新时间事件 ID */
	long long id = eventLoop->timeEventNextId++;

	/* 时间事件指针 */
	aeTimeEvent *te;

	te = malloc(sizeof(*te));
	if (te == NULL) return AE_ERR;

	/* 设置时间事件 ID */
	te->id = id;

	/* 设置处理事件的时间 */
	aeAddMillisecondsToNow(milliseconds, &te->when_sec, &te->when_ms);
	/* 设置事件处理器 */
	te->timeProc = proc;
	te->finalizerProc = finalizerProc;
	/* 设置私有数据 */
	te->clientData = clientData;
	/* 将新事件放入表头 */
	te->prev = NULL;
	te->next = eventLoop->timeEventHead;
	if (te->next) {
		te->next->prev = te;
	}
	eventLoop->timeEventHead = te;

	return id;
}

/*
 * 删除时间事件
 */
int aeDeleteTimeEvent(aeEventLoop *eventLoop, long long id) {
	aeTimeEvent *te = eventLoop->timeEventHead;
	/* 遍历链表 */
	while (te) {
		if (te->id == id) {
			/* 将时间事件标记为已删除 */
			te->id = AE_DELETED_EVENT_ID;
			return AE_OK;
		}
		/* 将指针移到下一个事件 */
		te = te->next;
	}
	/* 没有找到指定 id 的时间事件 */
	return AE_ERR;
}
