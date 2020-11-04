#include <stdlib.h>
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
	if (aeApiCreate(eventLoop) == -1) goto err;

	/* 初始化监听事件，当事件的掩码等于 AE_NONE 时表示没有被设置 */
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
