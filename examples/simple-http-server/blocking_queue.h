#ifndef __BLOCKING_QUEUE_H__
#define __BLOCKING_QUEUE_H__

#include <pthread.h>

#define BLOCKING_QUEUE_SIZE 1024

typedef struct {
    int size; /* 当前队列中描述符最大个数 */
    int used;  /* 当前队列中描述符个数 */
    int *fd;    /* 描述符数组 */
    int front;  /* 当前队列的头位置 */
    int rear;   /* 当前队列的尾位置 */
    pthread_mutex_t mutex; /* 锁 */
    pthread_cond_t free;   /* 队列空闲条件变量 */
    pthread_cond_t nonempty;   /* 队列非空条件变量 */
} blocking_queue;

void blocking_queue_init(blocking_queue *queue, int size);
void blocking_queue_push(blocking_queue *queue, int fd);
int blocking_queue_pop(blocking_queue *queue);

#endif
