// scheduler.h 
//	Data structures for the thread dispatcher and scheduler.
//	Primarily, the list of threads that are ready to run.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "copyright.h"
#include "list.h"
#include "thread.h"

// The following class defines the scheduler/dispatcher abstraction -- 
// the data structures and operations needed to keep track of which 
// thread is running, and which threads are ready but not running.

class NachOSscheduler {
  public:
    NachOSscheduler();			// Initialize list of ready threads 
    ~NachOSscheduler();			// De-allocate ready list

    void ThreadIsReadyToRun(NachOSThread* thread);	// Thread can be dispatched.
    void ThreadSleep(NachOSThread* thread,int waketime);
    int ThreadWake (int time);

    NachOSThread* FindNextThreadToRun();		// Dequeue first thread on the ready 
					// list, if any, and return thread.
    void Schedule(NachOSThread* nextThread);	// Cause nextThread to start running
    void Print();			// Print contents of ready list
    List *ThreadSleeping; 
    int exitCode[1000];
    List *WaitingForChild;

  private:
    List *readyThreadList;  		// queue of threads that are ready to run,
				// but not running
    
};

#endif // SCHEDULER_H
