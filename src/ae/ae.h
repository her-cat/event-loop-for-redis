#ifndef __AE_H__
#define __AE_H__

#include <time.h>

#define AE_OK 0
#define AE_ERR -1

#define AE_NONE 0 /* 没有注册事件。 */
#define AE_READABLE 1 /* 当描述符可读时触发。 */
#define AE_WRITABLE 2 /* 当描述符可写时触发。 */
#define AE_BARRIER 4  /* 对于 WRITABLE，当 READABLE 事件已在同一事件循环迭代中被触发，则永远不要触发该事件。 */

#define AE_FILE_EVENTS 1 /* 文件事件 */
#define AE_TIME_EVENTS 2 /* 时间事件 */
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS) /* 所有事件 */
#define AE_DONT_WAIT 4 /* 不阻塞，也不进行等待 */
#define AE_CALL_AFTER_SLEEP 8 /* 表示是否执行休眠后的回调函数 */

#define AE_NOMORE -1 /* 表示时间事件不需要继续执行 */
#define AE_DELETED_EVENT_ID -1 /* 表示时间事件已删除 */

/* 宏定义 */
#define AE_NOTUSED(v) ((void)v)

/* 前置声明 */
struct aeEventLoop;

/* 回调函数原型 */
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);
typedef int aeTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData);
typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData);
typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);

/* 文件事件结构体 */
typedef struct aeFileEvent {
    int mask; /* 事件掩码，one of AE_(READABLE|WRITABLE|BARRIER)。 */
    aeFileProc *rfileProc; /* 可读回调函数，当事件可读时调用该函数。 */
    aeFileProc *wfileproc; /* 可写回调函数，当事件可写时调用该函数。 */
    void *clientData; /* 多路复用库的私有数据，一般是指向 redisClient 的指针。 */
} aeFileEvent;

/* 时间事件结构体 */
typedef struct aeTimeEvent {
    long long id; /* 时间事件 ID */
    long when_sec; /* 秒数 */
    long when_ms; /* 毫秒数 */
    aeTimeProc *timeProc; /* 时间事件回调函数，当时间事件被触发时调用该函数。 */
    aeEventFinalizerProc *finalizerProc; /* 时间事件清理函数，当删除时间事件的时候会被调用。 */
    void *clientData; /* 多路复用库的私有数据，一般是指向 redisClient 的指针。 */
    struct aeTimeEvent *prev; /* 上一个时间事件 */
    struct aeTimeEvent *next; /* 下一个时间事件 */
} aeTimeEvent;

/* 已就绪事件结构体 */
typedef struct aeFiredEvent {
    int fd;
    int mask;
} aeFiredEvent;

/* 事件循环结构体 */
typedef struct aeEventLoop {
    int maxfd; /* 当前已注册最大的文件描述符。 */
    int setsize; /* 目前已追踪的最大描述符。 */
    long long timeEventNextId; /* 用于生成时间事件 id。 */
    time_t lastTime; /* 最后一次运行时间，用于检测系统时钟偏差，处理时间回拨的情况。 */
    aeFileEvent *events; /* 已注册的文件事件。 */
    aeFiredEvent *fired; /* 已就绪的文件事件。 */
    aeTimeEvent *timeEventHead; /* 时间事件链表头节点。 */
    int stop; /* 事件循环结束标识。 */
    void *apidata; /* 用于存储轮询API特定的数据，不同的 I/O 多路复用技术有不同的数据结构。 */
    aeBeforeSleepProc *beforesleep; /* 进入轮询API前需要执行的操作 */
    aeBeforeSleepProc *aftersleep; /* 轮询API结束后需要执行的操作 */
    int flags; /* 事件循环状态 flags */
} aeEventLoop;

/* 函数原型 */
aeEventLoop *aeCreateEventLoop(int setsize);
void aeDeleteEventLoop(aeEventLoop *eventLoop);
void aeStop(aeEventLoop *eventLoop);
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask, aeFileProc *proc, void *clientData);
void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask);
int aeGetFileEvents(aeEventLoop *eventLoop, int fd);
long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds, 
        aeTimeProc *proc, void *clientData, 
        aeEventFinalizerProc *finalizerProc);
int aeDeleteTimeEvent(aeEventLoop *eventLoop, long long id);
int aeProcessEvents(aeEventLoop *eventLoop, int flags);
int aeWait(int fd, int mask, long long milliseconds);
void aeMain(aeEventLoop *eventLoop);
char *aeGetApiName(void);
void aeSetBeforeSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *beforesleep);
void aeSetAfterSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *aftersleep);
int aeGetSetSize(aeEventLoop *eventLoop);
int aeResizeSetSize(aeEventLoop *eventLoop, int setsize);
void aeSetDontWait(aeEventLoop *eventLoop, int noWait);
#endif
