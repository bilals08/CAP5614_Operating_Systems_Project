// exception.cc 
//  Entry point into the Nachos kernel from user programs.
//  There are two kinds of things that can cause control to
//  transfer back to here from user code:
//
//  syscall -- The user code explicitly requests to call a procedure
//  in the Nachos kernel.  Right now, the only function we support is
//  "Halt".
//
//  exceptions -- The user code does something that the CPU can't handle.
//  For instance, accessing memory that doesn't exist, arithmetic errors,
//  etc.  
//
//  Interrupts (which can also cause control to transfer from user
//  code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "addrspace.h"

//----------------------------------------------------------------------
// ExceptionHandler
//  Entry point into the Nachos kernel.  Called when a user program
//  is executing, and either does a syscall, or generates an addressing
//  or arithmetic exception.
//
//  For system calls, the following is the calling convention:
//
//  system call code -- r2
//      arg1 -- r4
//      arg2 -- r5
//      arg3 -- r6
//      arg4 -- r7
//
//  The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//  "which" is the kind of exception.  The list of possible exceptions 
//  are in machine.h.
//----------------------------------------------------------------------

void incrementPC() {
    int oldPCReg = machine->ReadRegister(PCReg);
    
    machine->WriteRegister(PrevPCReg, oldPCReg);
    machine->WriteRegister(PCReg, oldPCReg + 4);
    machine->WriteRegister(NextPCReg, oldPCReg + 8);
}

void childFunction(int pid) {

    currentThread->RestoreUserState();

    currentThread->space->RestoreState();
    
    int pcReg = machine->ReadRegister(34);
    printf("Process [%d] Fork: start at address [%d] with [%d] pages memory\n", 
           pid, pcReg, currentThread->space->GetNumPages());

    machine->Run();
           
}

void doExit(int status) {
    delete currentThread->space;
    currentThread->Finish();
    currentThread->space->pcb->exitStatus = status;
    PCB* pcb = currentThread->space->pcb;
    pcb->DeleteExitedChildrenSetParentNull();

    if (pcb->parent == NULL) delete pcb;
}

int doFork(int function) {
    // 1. Check if sufficient memory exists to create new process
    if (currentThread->space->GetNumPages() <= mm->GetFreePageCount()) {
        return -1;
    }

    // 2. SaveUserState for the parent thread
    currentThread->SaveUserState();

    // 3. Create  a new address space for child by copying parent address space
    AddrSpace* childAddrSpace = new AddrSpace(currentThread->space);
    
    // 4. Create a new thread for the child and set it's address space
    Thread* childThread = new Thread("childThread");
    childThread->space = childAddrSpace;

    // 5. Create a new PCB for the child and connect it all up
    PCB* childPCB = pcbManager->AllocatePCB();
    PCB* parentPCB = pcbManager->GetPCB(currentThread->pid);
    childPCB->thread = childThread;
    childPCB->parent = parentPCB;
    parentPCB->AddChild(childPCB);

    // 6. Setup machine registers for child and save it to child thread
    machine->WriteRegister(34, function); // PCReg
    machine->WriteRegister(35, function+4); // NextPCReg
    machine->WriteRegister(36, function-4); // PrevPCReg
    childThread->SaveUserState();

    // 7. Call thread->fork on child
    childThread->Fork(childFunction, childPCB->pid);

    // 8. Restore register state of parent user-level process
    currentThread->RestoreUserState();

    return childPCB->pid;

}

int doJoin(int pid) {

    // 1. Check if this is a valid pid and return -1 if not
    PCB* joinPCB = pcbManager->GetPCB(pid);
    if (joinPCB == NULL) return -1;

    // 2. Check if pid is a child of current process
    PCB* pcb = currentThread->space->pcb;
    if (pcb != joinPCB->parent) return -1;

    // 3. Yield until joinPCB has not exited
    while(!joinPCB->HasExited()) currentThread->Yield();

    // 4. Store status and delete joinPCB
    int status = joinPCB->exitStatus;
    delete joinPCB;

    // 5. return status;
    return status;

}

int doKill(int pid) 
{
    // 1. Check if the pid is valid and if not, return -1
    PCB* pcb = pcbManager->GetPCB(pid); // Retrieve the PCB for the given PID
    if (pcb == NULL) 
    {
        return -1; // Invalid PID
    }

    // 2. IF pid is self, then just exit the process
    if (pcb == currentThread->space->pcb) 
    {
        doExit(0); // Exit the current process
        return 0;
    }

    // 3. Valid kill, pid exists and not self, do cleanup similar to Exit
    // However, change references from currentThread to the target thread
    Thread* targetThread = pcb->thread; // Access the thread associated with the target PCB

    // Perform cleanup actions for the target thread
    targetThread->Finish(); // Mark the thread for destruction

    // 4. return 0 for success!
    return 0;
}


void doYield() {
    currentThread->Yield();
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
        DEBUG('a', "Shutdown, initiated by user program.\n");
        interrupt->Halt();
    } else if ((which == SyscallException) && (type == SC_Yield)) {
         DEBUG('a', "Yield system call initiated by user program.\n");
        doYield();
        incrementPC();
    } else if ((which == SyscallException) && (type == SC_Exit)) {
        DEBUG('a', "Exit system call initiated by user program with status %d.\n", machine->ReadRegister(4));
        doExit(machine->ReadRegister(4));
    } else if ((which == SyscallException) && (type == SC_Fork)) {
        DEBUG('a', "Fork system call initiated by user program.\n");
        int ret = doFork(machine->ReadRegister(4));
        machine->WriteRegister(2, ret);
        incrementPC();
    } else if ((which == SyscallException) && (type == SC_Join)) {
        DEBUG('a', "Fork system call initiated by user program.\n");
        int ret = doJoin(machine->ReadRegister(4));
        machine->WriteRegister(2, ret);
        incrementPC();
    } else if ((which == SyscallException) && (type == SC_Kill)) {
        DEBUG('a', "Fork system call initiated by user program.\n");
        int ret = doKill(machine->ReadRegister(4));
        machine->WriteRegister(2, ret);
        incrementPC();
    } else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}

