/* This file will contain your solution. Modify it as you wish. */
#include <types.h>
#include <lib.h>
#include <synch.h>
#include "producerconsumer_driver.h"

/* Declare any variables you need here to keep track of and
   synchronise your bounded. A sample declaration of a buffer is shown
   below. It is an array of pointers to items.

   You can change this if you choose another implementation.
   However, you should not have a buffer bigger than BUFFER_SIZE
*/



struct semaphore *mutex;
struct semaphore *empty;
struct semaphore *full;

struct circular_buffer{
    data_item_t * item_buffer[BUFFER_SIZE];
    int front;
    int rear;
};

struct circular_buffer BUF;

/* consumer_receive() is called by a consumer to request more data. It
   should block on a sync primitive if no data is available in your
   buffer. It should not busy wait! */

data_item_t * consumer_receive(void)
{
        data_item_t * item;
        P(full);
        P(mutex);
        item = BUF.item_buffer[BUF.front];
        BUF.front = (BUF.front + 1)%BUFFER_SIZE;        
        V(mutex);
        V(empty);

        return item;
}

/* procucer_send() is called by a producer to store data in your
   bounded buffer.  It should block on a sync primitive if no space is
   available in your buffer. It should not busy wait!*/

void producer_send(data_item_t *item)
{
        P(empty);
        P(mutex);
        BUF.item_buffer[BUF.rear] = item;
        BUF.rear = (BUF.rear + 1)%BUFFER_SIZE;   
        V(mutex);
        V(full);
}

/* Perform any initialisation (e.g. of global data) you need
   here. Note: You can panic if any allocation fails during setup */

void producerconsumer_startup(void)
{
    BUF.front = 0;
    BUF.rear = 0;
    mutex = sem_create("mutex",1);
    /* initialise mutex as 1 */
    empty = sem_create("empty",BUFFER_SIZE);
    /*count empty slots*/
    full = sem_create("full",0);
    if (full == NULL)
        panic("sem_create failed");
    /* count full slots*/
}

/* Perform any clean-up you need here */
void producerconsumer_shutdown(void)
{
    sem_destroy(mutex);
    sem_destroy(empty);
    sem_destroy(full);
    
}
