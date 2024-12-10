#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data *args = (struct thread_data *) thread_param;

    args->thread_complete_success = false;

    printf("[THREAD] Sleeping...\n");
    usleep(1000 * args->wait_to_obtain_ms);

    printf("[THREAD] Locking...\n");
    pthread_mutex_lock(args->mutex);

    printf("[THREAD] Waiting...\n");
    usleep(1000 * args->wait_to_release_ms);

    printf("[THREAD] Unlocking...\n");
    pthread_mutex_unlock(args->mutex);

    args->thread_complete_success = true;
    
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data *new_thread_args = malloc(sizeof(struct thread_data));

    printf("new_thread: %llx | args: %llx | mutex: %llx\n",
        (unsigned long long)thread, (unsigned long long)new_thread_args, (unsigned long long)mutex);

    if (new_thread_args == NULL)
        return false;

    new_thread_args->mutex = mutex;
    new_thread_args->wait_to_obtain_ms = wait_to_obtain_ms;
    new_thread_args->wait_to_release_ms = wait_to_release_ms;

    int ret;
    ret = pthread_create(thread, NULL, threadfunc, new_thread_args);
    if (ret != 0) { // thread creation failed
        printf("Thread creation failed with error code %d\n", ret);
        free(new_thread_args);
        return false;
    }
    return true;
}
