#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>
#include "ae.h"

typedef struct aeApiState {
    int epfd; /* epoll 实例文件描述符。 */
    struct epoll_event *events; /* 用于存储需要监听的文件事件。 */
} aeApiState;

/*
 * 创建一个 epoll 实例。
 */
static int aeApiCreate(aeEventLoop *eventLoop) {
    aeApiState *state = malloc(sizeof(aeApiState));

    if (!state) return -1;
    state->events = malloc(sizeof(struct epoll_event) * eventLoop->setsize);
    if (!state->events) {
        free(state);
        return -1;
    }
    /* 1024只是对内核的一个提示。 */
    state->epfd = epoll_create(1024);
    if (state->epfd == -1) {
        free(state->events);
        free(state);
        return -1;
    }

    eventLoop->apidata = state;
    return 0;
}

/*
 * 调整事件槽大小。
 */
static int aeApiResize(aeEventLoop *eventLoop, int setsize) {
    aeApiState *state = eventLoop->apidata;

    state->events = realloc(state->events, sizeof(struct epoll_event) * setsize);
    return 0;
}

/*
 * 释放 epoll 实例和事件槽。
 */
static void aeApiFree(aeEventLoop *eventLoop) {
    aeApiState *state = eventLoop->apidata;

    close(state->epfd);
    free(state->events);
    free(state);
}

/*
 * 关联给定事件到 fd。
 */
static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask) {
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee = {0}; /* 避免 valgrind 警告。 */

    /* 如果 fd 没有关联任何事件，那么这是一个 ADD 操作，
     * 如果 fd 关联了某个/某些事件，那么这时一个 MOD 操作。 */
     int op = eventLoop->events[fd].mask == AE_NONE ?
        EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    ee.events = 0;
    mask |= eventLoop->events[fd].mask; /* 合并旧的事件。 */
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;

    if (epoll_ctl(state->epfd, op, fd, &ee) == -1) return -1;
    return 0;
}

/*
 * 从 fd 中删除给定的事件。
 */
static void aeApiDelEvent(aeEventLoop *eventLoop, int fd, int delmask) {
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee = {0}; /* 避免 valgrind 警告。 */
    int mask = eventLoop->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;

    if (mask != AE_NONE) {
        /* 如果 mask 不等于 AE_NONE，说明只是删除了某个/某些事件。 */
        epoll_ctl(state->epfd, EPOLL_CTL_MOD, fd, &ee);
    } else {
        /* 注意，Kernel<2.6.9要求一个非空的事件指针，即使对于EPOLL_CTL_DEL也是如此。 */
        epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, &ee);
    }
}

/*
 * 获取可执行事件。
 */
static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, numevents = 0;

    retval = epoll_wait(state->epfd,state->events,eventLoop->setsize,
            tvp ? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1);
    if (retval > 0) {
        int j;
        
        numevents = retval;
        for (j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events+j;

            if (e->events & EPOLLIN) mask |= AE_READABLE;
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if (e->events & EPOLLERR) mask |= AE_READABLE|AE_WRITABLE;
            if (e->events & EPOLLHUP) mask |= AE_READABLE|AE_WRITABLE;
            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }

    return numevents;
}

/*
 * 获取多路复用库的名称
 */
static char *aeApiName(void) {
    return "epoll";
}
