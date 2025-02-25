#include "opt-synchprobs.h"
#include "math_tester.h"
#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>



enum {
        NADDERS = 10,        /* the number of adder threads */
        NADDS   = 10000, /* the number of overall increments to perform */
};

/*
 * Declare the counter variable that all the adder() threads increment
 *
 * Declaring it "volatile" instructs the compiler to always (re)read the
 * variable from memory and not optimise by removing memory references
 * and re-using the content of a register.
 */
volatile unsigned long int counter;

/*
 * Declare an array of adder counters to count per-thread
 * increments. These are used for printing statistics.
 */
unsigned long int adder_counters[NADDERS];


/*
 * We use a semaphore to wait for adder() threads to finish
 */
struct semaphore *finished;
// use a lock to set the critical section


/*
 * **********************************************************************
 * ADD YOUR OWN VARIABLES HERE AS NEEDED
 * **********************************************************************
 */
struct lock *lockmath;


/*
 * adder()
 *
 *  Each adder thread simply keeps incrementing the counter until we
 *  hit the max value.
 *
 * **********************************************************************
 * YOU NEED TO INSERT SYNCHRONISATION PRIMITIVES APPROPRIATELY
 * TO ENSURE COUNTING IS CORRECTLY PERFORMED.
 * **********************************************************************
 *
 * You should not re-write the existing code.
 *
 * + Only the correct number of increments are performed
 * + Ensure x+1 == x+1
 * + Ensure that the statistics kept match the number of increments
 *   performed.
 * + only synchronise the critical section, no more.
 *
 */

static void adder(void * unusedpointer, unsigned long addernumber)
{
        unsigned long int a, b;
        int flag = 1;

        /*
         * Avoid unused variable warnings.
         */
        (void) unusedpointer; /* remove this line if variable is used */



        while (flag) {
                /* loop doing increments until we achieve the overall number
                   of increments */
                lock_acquire(lockmath);
                // check the status of the lock
                a = counter;
                if (a < NADDS) {

                        counter = counter + 1;

                        math_test_1(addernumber); /* We use this for testing, please leave this here. */

                        b = counter;

                        math_test_2(addernumber); /* We use this for testing, please leave this here.
                                                   * Also note it should NOT execute mutually exclusively
                                                   */

                        /*
                         * now count the number of increments we perform  for statistics.
                         * addernumber is effectively the thread number of this thread.
                         * Note: An individual array location is only accessed by a single
                         * thread and has no concurrency issues.
                         */
                        adder_counters[addernumber]++;

                        /* check we are getting sane results. This should not print in a correct
                         * solution.
                         */
                        if (a + 1 != b) {
                                kprintf("In thread %ld, %ld + 1 == %ld?\n",
                                        addernumber, a, b) ;
                        }
                } else {
                        flag = 0;
                }
                lock_release(lockmath);
                //release the lock
        }

        /* signal the main thread we have finished and then exit */
        V(finished);

        thread_exit();
}

/*
 * maths()
 *
 * This function:
 *
 * + Initialises the counter variables
 * + Creates a semaphore to wait for adder threads to complete
 * + Starts the define number of adder threads
 * + waits, prints statistics, cleans up, and exits
 */

int maths (int data1, char **data2)
{
        int index, error;
        unsigned long int sum;

        /*
         * Avoid unused variable warnings from the compiler.
         */
        (void) data1;
        (void) data2;

        /* initialise the counter before the threads start */

        counter = 0;
        for (index = 0; index < NADDERS; index++) {
                adder_counters[index] = 0;
        }

        /* create a semaphore to allow main thread to wait on workers */

        finished = sem_create("finished", 0);

        if (finished == NULL) {
                panic("maths: sem create failed");
        }

        /*
         * ********************************************************************
         * INSERT ANY INITIALISATION CODE YOU REQUIRE HERE
         * ********************************************************************
         */
        lockmath = lock_create("lockmath");
        // create a lock called lockmath
        

        /*
         * Start NADDERS adder() threads.
         */

        kprintf("Starting %d adder threads\n", NADDERS);

        for (index = 0; index < NADDERS; index++) {

                error = thread_fork("adder thread", NULL, &adder, NULL, index);

                /*
                 * panic() on error as we can't progress if we can't create threads.
                 */

                if (error) {
                        panic("adder: thread_fork failed: %s\n",
                              strerror(error));
                }
        }


        /*
         * Wait until the adder threads complete by waiting on
         * the semaphore NADDER times.
         */

        for (index = 0; index < NADDERS; index++) {
                P(finished);
        }

        kprintf("Adder threads performed %ld adds\n", counter);

        /* Print out some statistics, they should add up */
        sum = 0;
        for (index = 0; index < NADDERS; index++) {
                sum += adder_counters[index];
                kprintf("Adder %d performed %ld increments.\n", index,
                        adder_counters[index]);
        }
        kprintf("The adders performed %ld increments overall (expected %d)\n", sum, NADDS);

        /*
         * **********************************************************************
         * INSERT ANY CLEANUP CODE YOU REQUIRE HERE
         * **********************************************************************
         */
        lock_destroy(lockmath);
        /* clean up the lock we created earlier */

        /* clean up the semaphore we allocated earlier */
        sem_destroy(finished);
        return 0;
}
