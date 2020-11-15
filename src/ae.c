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
	*seconds = tv.tv_sec;
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
			/* 将时间事件标记为待删除 */
			te->id = AE_DELETED_EVENT_ID;
			return AE_OK;
		}
		/* 将指针移到下一个事件 */
		te = te->next;
	}
	/* 没有找到指定 id 的时间事件 */
	return AE_ERR;
}

/* 寻找离当前时间最近的时间事件，
 * 此操作有助于了解 select 可以在不延迟任何事件的情况下休眠多少时间。
 * Note: 因为链表是乱序的，所以查找复杂度为 O(N)
 */
static aeTimeEvent *aeSearchNearestTimer(aeEventLoop *eventLoop) {
	aeTimeEvent *te = eventLoop->timeEventHead;
	aeTimeEvent *nearest = NULL;

	while (te) {
		if (!nearest || te->when_sec < nearest->when_sec || 
				(te->when_sec == nearest->when_sec && 
				te->when_ms < nearest->when_ms))
			nearest = te;
		te = te->next;
	}

	return nearest;
}

/* 
 * 处理所有已到达的时间事件 
 */
static int processTimeEvents(aeEventLoop *eventLoop) {
	int processed = 0;
	aeTimeEvent *te;
	long long maxId;
	time_t now = time(NULL);

	/* 如果事件循环最后运行时间 > 当前时间，
	   说明当前时间被回拨到过去的时间点，需要通过重置事件执行时间，防止事件处理混乱。 */
	if (now < eventLoop->lastTime) {
		te = eventLoop->timeEventHead;
		while (te) {
			/* 将事件执行时间重置为 0，使事件稍后将被执行 */
			te->when_sec = 0;
			te = te->next;
		}
	}
	/* 重置事件循环最后运行时间 */
	eventLoop->lastTime = now;

	te = eventLoop->timeEventHead;
	maxId = eventLoop->timeEventNextId + 1;

	while (te) {
		long now_sec, now_ms;
		long long id;

		/* 删除被标记为待删除的事件 */
		if (te->id == AE_DELETED_EVENT_ID) {
			aeTimeEvent *next = te->next;
			/* 断开 te 跟上一个事件的联系  */
			if (te->prev)
				/* 将 te 的前驱节点的后继节点指向 te 的后继节点 */
				te->prev->next = te->next;
			else
				/* te->prev 为 NULL 表示 te 是第一个事件，直接将头节点指向 te->next 即可 */
				eventLoop->timeEventHead = te->next;
			/* 断开 te 跟下一个事件的联系 */
			if (te->next)
				/* 将 te 的后继节点的前驱节点指向 te 的前驱节点 */
				te->next->prev = te->prev;
			if (te->finalizerProc)
				/* 执行事件清理函数 */
				te->finalizerProc(eventLoop, te->clientData);
			/* 释放时间事件 */
			free(te);
			te = next;
			continue;
		}

		/* 跳过无效事件 */
		if (te->id > maxId) {
			te = te->next;
			continue;
		}

		/* 获取当前的秒数和毫秒数 */
		aeGetTime(&now_sec, &now_ms);
		/* 当前秒数 > 事件的秒数 */
		if (now_sec > te->when_sec || 
				/* 当前秒数 == 事件秒数 且 当前毫秒数 >= 事件的毫秒数 */
				(now_sec == te->when_sec && now_ms >= te->when_ms)) 
		{
			int retval;

			id = te->id;
			/* 执行事件回调函数，并获取返回值 */
			retval = te->timeProc(eventLoop, id, te->clientData);
			processed++;
			/* 时间事件是否需要继续执行 */
			if (retval != AE_NOMORE) {
				/* retval 毫秒后继续执行这个事件 */
				aeAddMillisecondsToNow(retval, &te->when_sec, &te->when_ms);
			} else {
				/* 如果不需要继续执行，则标记为待删除 */
				te->id = AE_DELETED_EVENT_ID;
			}
		}
		te = te->next;
	}

	return processed;
}

/**
 * 处理所有已到达的时间事件，以及所有已就绪的事件。
 *
 * 如果不传入特殊的 flags 的话，那么函数睡眠直到文件事件就绪，
 * 或者下一个时间事件到达（如果有的话）。
 *
 * 如果 flags 为 0，那么函数什么都不做，直接返回。
 * 如果 flags 包含 AE_ALL_EVENTS，所有类型的事件都会被处理。
 * 如果 flags 包含 AE_FILE_EVENTS，那么会处理文件事件。
 * 如果 flags 包含 AE_TIME_EVENTS，那么会处理时间事件。
 * 如果 flags 包含 AE_DONT_WAIT，那么函数会在处理完所有不许阻塞事件之后，立即返回。
 * 如果 flags 包含 AE_CALL_AFTER_SLEEP，那么会在休眠后调用 aftersleep 回调函数。
 *
 * 函数的返回值为已处理事件的数量。
 */
int aeProcessEvents(aeEventLoop *eventLoop, int flags) {
	int processed = 0, numevents;

	/* 没有需要处理的事件，尽快返回 */
	if (!(flags & AE_TIME_EVENTS) && !(flags & AE_FILE_EVENTS)) return 0;

	/* 如果已经注册了文件事件，或者需要处理时间事件且没有设置 AE_DONT_WAIT 时，
	 * 我们需要获取下一个时间事件执行时间，以便于在下一个时间事件准备好触发之前休眠。*/
	if (eventLoop->maxfd != -1 ||
		((flags & AE_TIME_EVENTS) && !(flags & AE_DONT_WAIT))) {
		int j;
		aeTimeEvent *shortest = NULL;
		struct timeval tv, *tvp;

		/* 如果处理时间事件，需要将进程阻塞进行休眠，所以设置了 AE_DONT_WAIT 就不能处理时间事件了。*/
		if (flags & AE_TIME_EVENTS && !(flags & AE_DONT_WAIT))
			/* 获取最近的时间事件 */
			shortest = aeSearchNearestTimer(eventLoop);

		if (shortest) {
			/* 如果存在时间事件，
			 * 根据最近可执行时间事件的执行时间和现在的事件计算出来的时间差，来决定文件事件阻塞时间。*/
			long now_sec, now_ms;

			aeGetTime(&now_sec, &now_ms);
			tvp = &tv;

			/* 计算出需要等待多少毫秒才能触发下一个时间事件 */
			long long ms = (shortest->when_sec - now_sec) * 1000 + shortest->when_ms - now_ms;

			if (ms > 0) {
				tvp->tv_sec = ms / 1000;
				tvp->tv_usec = (ms % 1000) * 1000;
			} else {
				tvp->tv_sec = 0;
				tvp->tv_usec = 0;
			}
		} else {
			/* 执行到了这一步，说明没有时间事件。 */
			/* 如果设置了 AE_DONT_WAIT 需要尽快返回。 */
			if (flags & AE_DONT_WAIT) {
				/* 将超时时间设置为 0，表示非阻塞，不管有没有文件事件到达都立即返回。 */
				tv.tv_sec = tv.tv_usec = 0;
				tvp = &tv;
			} else {
				/* 其他情况可以一直阻塞，直到有文件事件到达。 */
				tvp = NULL;
			}
		}

		/* 调用多路复用API，将仅在超时或某些事件激发时返回，超时时间由 tvp 决定。
		 * tvp 为 NULL：表示如果没有 I/O 事件发生，则一直等待下去。
		 * tvp->tv_sec 或 tvp->tv_usec 为 0：表示阻塞指定的时间。
		 * tvp->tv_sec 且 tvp->tv_usec 为 0：表示不等待，检测完立即返回。 */
		numevents = aeApiPoll(eventLoop, tvp);

		/* 如果设置了回调函数且 flags 设置了 AE_CALL_AFTER_SLEEP */
		if (eventLoop->aftersleep != NULL && flags & AE_CALL_AFTER_SLEEP)
			/* 执行休眠后的回调函数 */
			eventLoop->aftersleep(eventLoop);

		/* 处理已就绪的文件事件 */
		for (j = 0; j < numevents; j++) {
			aeFileEvent *fe = &eventLoop->events[eventLoop->fired[j].fd];
			int mask = eventLoop->fired[j].mask;
			int fd = eventLoop->fired[j].fd;
			int fired = 0; /* 用来表示该 fd 是否被处理过 */

			/*
			 * 通常我们先执行可读事件，然后再执行可写事件。
			 * 因为有时我们可以在处理查询之后立即提供查询的答复。
			 *
			 * 但是，如果在掩码中设置了AE_BARRIER，
			 * 我们的应用程序会要求我们做相反的操作：在readable之后永远不要触发可写事件。
			 * 在这种情况下，我们反转调用。
			 * 例如，当我们想在 beforeSleep() 钩子中执行某些操作时，这非常有用，比如在回复客户端之前将文件同步到磁盘。*/
			int invert = fe->mask & AE_BARRIER;

			/*
			 * 注意“fe->mask & mask & ...”代码：可能一个已经处理的事件删除了一个已就绪的元素，
			 * 而我们仍然没有处理，所以我们检查事件是否仍然有效。
			 *
			 * 如果调用顺序没有颠倒，则处理可读事件。*/
			if (!invert && fe->mask & mask & AE_READABLE) {
				fe->rfileProc(eventLoop, fd, fe->clientData, mask);
				/* fired 确保读/写事件只能执行其中一个。 */
				fired++;
				/* 重新获取已就绪文件事件，因为调用 rfileProc 处理可读事件时，可能改变了文件事件的状态。 */
				fe = &eventLoop->events[fd];
			}

			/* 如果事件类型为可写，处理可写事件。 */
			if (fe->mask & mask & AE_WRITABLE) {
				/* 文件事件没有被处理过或读/写事件不是同一个回调函数。 */
				if (!fired || fe->rfileProc != fe->wfileproc) {
					fe->wfileproc(eventLoop, fd, fe->clientData, mask);
					fired++;
				}
			}

			/* 如果调用顺序被颠倒了。 */
			if (invert) {
				fe = &eventLoop->events[fd];
				/* 如果事件类型为可读， */
				if ((fe->mask & mask & AE_READABLE) &&
					/* 并且文件事件没有被处理过或读/写事件不是同一个回调函数。 */
					(!fired || fe->rfileProc != fe->wfileproc)) {
					fe->rfileProc(eventLoop, fd, fe->clientData, mask);
					fired++;
				}
			}
			/* 已处理事件数量+1 */
			processed++;
		}
	}

	/* 检查时间事件。 */
	if (flags & AE_TIME_EVENTS)
		processed += processTimeEvents(eventLoop);

	/* 返回已处理的文件/时间事件数。*/
	return processed;
}
