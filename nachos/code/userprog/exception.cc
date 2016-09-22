// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
#include "scheduler.h"
#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"
#include "machine.h"
#include "translate.h"
#include "utility.h"
#include "addrspace.h"
#include "stats.h"
#include "thread.h"
#include "switch.h"
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

static void ConvertIntToHex (unsigned v, Console *console)
{
   unsigned x;
   if (v == 0) return;
   ConvertIntToHex (v/16, console);
   x = v % 16;
   if (x < 10) {
      writeDone->P() ;
      console->PutChar('0'+x);
   }
   else {
      writeDone->P() ;
      console->PutChar('a'+x-10);
   }
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
	 int memval, vaddr, printval, tempval, exp;
    unsigned printvalus;        // Used for printing in hex
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);;

    if ((which == SyscallException) && (type == SYScall_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == SYScall_PrintInt)) {
       printval = machine->ReadRegister(4);
       if (printval == 0) {
	  writeDone->P() ;
          console->PutChar('0');
       }
       else {
          if (printval < 0) {
	     writeDone->P() ;
             console->PutChar('-');
             printval = -printval;
          }
          tempval = printval;
          exp=1;
          while (tempval != 0) {
             tempval = tempval/10;
             exp = exp*10;
          }
          exp = exp/10;
          while (exp > 0) {
	     writeDone->P() ;
             console->PutChar('0'+(printval/exp));
             printval = printval % exp;
             exp = exp/10;
          }
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_PrintChar)) {
	writeDone->P() ;
        console->PutChar(machine->ReadRegister(4));   // echo it!
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_GetReg)) {
	machine->WriteRegister(2,machine->ReadRegister(machine->ReadRegister(4)));	
       // Advance program counter
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_Fork)){
       nachOSThread(child);	
       // Allocate space to child
	for ( int i = numPagesinVM; i < 2*numPagesinVM-1; i++) {
		NachOSpageTable[i].virtualPage = i-numPagesinVM;
		NachOSpageTable[i].physicalPage = i;
		NachOSpageTable[i].use = FALSE;
		NachOSpageTable[i].dirty= FALSE;
		NachOSpageTable[i].readOnly = FALSE;
		NachOSpageTable[i].valid = TRUE;
	}
	//copy pocess address space to child space
       // Advance program counter
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_GetPA)) {
	int virtAddr = machine->ReadRegister(4);
	unsigned int vpn,offset,pageFrame;
	TranslationEntry *entry;
	vpn = (unsigned)virtAddr/PageSize;
	offset = (unsigned) virtAddr%PageSize;
	
	if(vpn>machine->pageTableSize) {
		machine->WriteRegister(2,-1);
		machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       		machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       		machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	}
	else if (!machine->NachOSpageTable[vpn].valid){
		machine->WriteRegister(2,-1);
		machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       		machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       		machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	}
	entry = &(machine->NachOSpageTable[vpn]);
	pageFrame = entry->physicalPage;
	if(pageFrame>=NumPhysPages){
		machine->WriteRegister(2,-1);
		 machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       		machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       		machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	}    
	machine->WriteRegister(2,(pageFrame*PageSize + offset));
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       	machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
   }
    else if((which == SyscallException) && (type == SYScall_GetPID)){
	//ASSERT(0);
	machine->WriteRegister(2, currentThread->GetPID());
	// Advance program counters.
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
   }
    else if((which == SyscallException) && (type == SYScall_GetPPID)){
	machine->WriteRegister(2, currentThread->GetPPID());
	// Advance program counters.
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
   }
    else if ((which == SyscallException) && (type == SYScall_Time)) {
	machine->WriteRegister(2,stats->totalTicks);        
						// Advance program counter
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_Sleep)) {
       int WakeUpTime = (machine->ReadRegister(4)+(stats->totalTicks);
       if(WakeUpTime == 0) currentThread->YieldCPU();       
	else{
		scheduler->ThreadSleep(currentThread,WakeUpTime);
		IntStatus oldLevel = interrupt->SetLevel(IntOff);
		currentThread->PutThreadToSleep();
		(void)interrupt->SetLevel(oldLevel);
	}
						// Advance program counter
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_Yield)) {
	NachOSThread *nextThread;
	nextThread = scheduler->FindNextThreadToRun();
       if(nextThread != NULL){
		scheduler->Schedule(nextThread);
	}
						// Advance program counter
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_PrintString)) {
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
	  writeDone->P() ;
          console->PutChar(*(char*)&memval);
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
	
    else if ((which == SyscallException) && (type == SYScall_PrintIntHex)) {
       printvalus = (unsigned)machine->ReadRegister(4);
       writeDone->P() ;
       console->PutChar('0');
       writeDone->P() ;
       console->PutChar('x');
       if (printvalus == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          ConvertIntToHex (printvalus, console);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    } else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}
