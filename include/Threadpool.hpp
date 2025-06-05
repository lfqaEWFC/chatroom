#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <pthread.h>
#include <unistd.h>

typedef void* (*typ_function)(void*);
typedef struct task
{   
    typ_function taskfunction;
    void* arg;
    task* next;
}task; 

class pool
{
    public:

    pool(size_t pthread_num);

    //function: typedef void* (*typ_function)(void*)
    void addtask(typ_function taskfunction,void* arg);

    void wait();

    ~pool();

    private:

    ssize_t threads_num;
    pthread_t *threads = nullptr;
    ssize_t quenesize = 0;
    task* quenehead = nullptr;
    task* quenetail = nullptr;
    ssize_t endflag = 0;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    ssize_t active_task = 0;

    static void* workpthread(void* args);

};

#endif
