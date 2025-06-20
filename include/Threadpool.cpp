#include "Threadpool.hpp"

pool::pool(size_t pthread_num):
threads_num(pthread_num)
{
    threads = new pthread_t[pthread_num];
    pthread_mutex_init(&lock,nullptr);
    pthread_cond_init(&cond,nullptr);        
    for(int i = 0;i < pthread_num;i++){
        pthread_create(&threads[i],nullptr,workpthread,this);
    }
}


void pool::addtask(typ_function taskfunction,void* arg){
    task* newtask = new task;
    newtask->arg = arg;
    newtask->taskfunction = taskfunction;   
    newtask->next = nullptr; 

    pthread_mutex_lock(&lock);
    if(quenehead == nullptr){
        quenehead = newtask;
        quenetail = quenehead;
    }
    else{
        quenetail->next = newtask;
        quenetail = newtask;
    }  
    quenesize++;
    active_task++;
    pthread_mutex_unlock(&lock);
    pthread_cond_signal(&cond);
}

void pool::wait(){   
    while(active_task > 0){
        usleep(1000);
    }
    endflag = 1;        
}

pool::~pool(){   
    wait();
    pthread_cond_broadcast(&cond);        
    for(int i = 0;i < threads_num;i++){
        pthread_join(threads[i],nullptr);
    }              
    delete(threads);            
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
}

void* pool::workpthread(void *args){
    pool* workpool = (pool*)args;
    while(1)
    {
        pthread_mutex_lock(&workpool->lock);
        while(workpool->quenesize == 0 && !workpool->endflag){
            pthread_cond_wait(&workpool->cond,&workpool->lock);
        }
        if(workpool->endflag && workpool->quenesize == 0){
            pthread_mutex_unlock(&workpool->lock);
            break;
        }
        task* cuttask = workpool->quenehead;
        workpool->quenehead = cuttask->next;
        workpool->quenesize--;
        pthread_mutex_unlock(&workpool->lock);

        cuttask->taskfunction(cuttask->arg);

        pthread_mutex_lock(&workpool->lock);
        workpool->active_task--;
        pthread_mutex_unlock(&workpool->lock);
        delete(cuttask);
    }
    return nullptr;
}
