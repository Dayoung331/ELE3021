#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_ITERS 30000
#define NUM_THREADS 30000

int shared_resource = 0;
volatile int lock_var = 0; // 락 변수

void lock();
void unlock();

// Compare-and-swap 함수
int compare_and_swap(volatile int *ptr, int old, int new) {
    int ret;
    __asm__ __volatile__(
        "lock cmpxchgl %2, %1"
        : "=a"(ret), "+m"(*ptr)
        : "r"(new), "a"(old)
        : "memory");
    return ret;
}

void lock(int tid)
{
    int backoff = 1;  // 초기 백오프 시간
    while (compare_and_swap(&lock_var, 0, 1) != 0) {
        // busy-wait를 피하기 위해 백오프
        usleep(backoff);
        backoff = backoff * 2;  // 백오프 시간 증가
        if (backoff > 1000) {
            backoff = 1000;  // 최대 백오프 시간 제한
        }
    }
}

void unlock(int tid)
{
    lock_var = 0;
}

void* thread_func(void* arg) {
    int tid = *(int*)arg;
    
    lock(tid);
    
        for(int i = 0; i < NUM_ITERS; i++)    shared_resource++;
    
    unlock(tid);
    
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];
    int tids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        tids[i] = i;
        pthread_create(&threads[i], NULL, thread_func, &tids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("shared: %d\n", shared_resource);
    
    return 0;
}