#include <stdlib.h>
#include <stdio.h>
#include "blocking_queue.h"

/* 初始化队列 */
void blocking_queue_init(blocking_queue *queue, int size) {
    queue->size = size;
    queue->fd = calloc(size, sizeof(int));
    queue->used = queue->front = queue->rear = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->free, NULL);
    pthread_cond_init(&queue->nonempty, NULL);
}

/* 将连接字放入队列 */
void blocking_queue_push(blocking_queue *queue, int fd) {
    /* 先加锁，防止其它线程读写队列 */
    pthread_mutex_lock(&queue->mutex);
    /* 队列已满则等待空闲信号 */
    while (queue->used == queue->size)
        pthread_cond_wait(&queue->free, &queue->mutex);
    /* 将新的 fd 放在队尾 */
    queue->fd[queue->rear] = fd;
    /* 如果已经到了最后，重置队尾位置 */
    if (++queue->rear == queue->size)
        queue->rear = 0;

    queue->used++;
    printf("[queue] push fd: %d \n", fd);

    /* 通知其他线程有新的连接字等待处理 */
    pthread_cond_signal(&queue->nonempty);
    /* 释放锁 */
    pthread_mutex_unlock(&queue->mutex);
}

/* 从队列取出连接字 */
int blocking_queue_pop(blocking_queue *queue) {
    /* 加锁 */
    pthread_mutex_lock(&queue->mutex);
    /* 如果队列为空就一直条件等待，直到有新连接入队列 */
    while (queue->used == 0) {
//        pthread_cond_wait(&queue->nonempty, &queue->mutex);
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    /* 取出队头的连接字 */
    int fd = queue->fd[queue->front];
    /* 如果已经到最后，重置队头位置 */
    if (++queue->front == queue->size)
        queue->front = 0;

    queue->used--;
    printf("[queue] pop fd: %d \n", fd);

    /* 通知其他线程当前队列有空闲 */
    pthread_cond_signal(&queue->free);
    /* 释放锁 */
    pthread_mutex_unlock(&queue->mutex);

    return fd;
}
