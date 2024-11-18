// threadtest.cc
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield,
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "synch.h"

// testnum is set in main.cc
int testnum = 1;

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

#ifdef HW1_SEMAPHORES

int numThreadsActive;
int SharedVariable;
Semaphore *mutex = new Semaphore("mutex", 1);
Semaphore *barrier = new Semaphore("barrier", 0);
int counter = 0;

void SimpleThread(int which)
{
    int num, val;

    for (num = 0; num < 5; num++)
    {
        mutex->P();

        val = SharedVariable;
        printf("*** thread %d sees value %d\n", which, val);
        currentThread->Yield();
        SharedVariable = val + 1;

        mutex->V();

        currentThread->Yield();
    }

    // Barrier
    mutex->P();
    counter++;
    if (counter == numThreadsActive)
    {
        for (int i = 0; i < numThreadsActive - 1; i++)
        {
            barrier->V();
        }
        mutex->V();
    }
    else
    {
        mutex->V();
        barrier->P();
    }

    val = SharedVariable;
    printf("Thread %d sees final value %d\n", which, val);
}

#elif defined(HW1_LOCKS) || defined(HW1_ELEVATOR)

int numThreadsActive;
int SharedVariable;
Lock *mutex = new Lock("mutex");
Semaphore *barrier = new Semaphore("barrier", 0);
int counter = 0;

void SimpleThread(int which)
{
    int num, val;

    for (num = 0; num < 5; num++)
    {
        mutex->Acquire();

        val = SharedVariable;
        printf("*** thread %d sees value %d\n", which, val);
        currentThread->Yield();
        SharedVariable = val + 1;

        mutex->Release();

        currentThread->Yield();
    }

    // Barrier
    mutex->Acquire();
    counter++;
    if (counter == numThreadsActive)
    {
        for (int i = 0; i < numThreadsActive - 1; i++)
        {
            barrier->V();
        }
        mutex->Release();
    }
    else
    {
        mutex->Release();
        barrier->P();
    }

    val = SharedVariable;
    printf("Thread %d sees final value %d\n", which, val);
}

#else

void SimpleThread(int which)
{
    int num;

    for (num = 0; num < 5; num++)
    {
        printf("*** thread %d looped %d times\n", which, num);
        currentThread->Yield();
    }
}

#endif

//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, 1);
    SimpleThread(0);
}

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

#ifdef HW1_SEMAPHORES

void ThreadTest(int n)
{
    DEBUG('t', "Entering SimpleTest");
    Thread *t;
    numThreadsActive = n;
    printf("NumthreadsActive = %d\n", numThreadsActive);

    for (int i = 1; i < n; i++)
    {
        t = new Thread("forked thread");
        t->Fork(SimpleThread, i);
    }
    SimpleThread(0);
}

#elif defined(HW1_LOCKS) || defined(HW1_ELEVATOR)

void ThreadTest(int n)
{
    DEBUG('t', "Entering SimpleTest");
    Thread *t;
    numThreadsActive = n;
    printf("NumthreadsActive = %d\n", numThreadsActive);

    for (int i = 1; i < n; i++)
    {
        t = new Thread("forked thread");
        t->Fork(SimpleThread, i);
    }
    SimpleThread(0);
}

#else

void ThreadTest()
{
    switch (testnum)
    {
    case 1:
        ThreadTest1();
        break;
    default:
        printf("No test specified.\n");
        break;
    }
}

#endif