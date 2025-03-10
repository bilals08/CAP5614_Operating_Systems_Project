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
    
    int pcReg = machine->ReadRegister(PCReg);
    printf("Process [%d] Fork: start at address [%d] with [%d] pages memory\n", 
           pid, pcReg, currentThread->space->GetNumPages());

    machine->Run();
           
}

void doExit(int status) {
     printf("Process [%d] exits with [%d]\n", currentThread->pid, status);
    delete currentThread->space;
    currentThread->Finish();
    currentThread->space->pcb->exitStatus = status;
    PCB* pcb = currentThread->space->pcb;
    pcb->DeleteExitedChildrenSetParentNull();

    if (pcb->parent == NULL) delete pcb;
}

int doFork(int function) {
    // 1. Check if sufficient memory exists to create new process
    if (currentThread->space->GetNumPages() > mm->GetFreePageCount()) {
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
    // childThread->space->pcb = childPCB;
    childPCB->parent = parentPCB;
    childThread->pid = childPCB->pid;
    parentPCB->AddChild(childPCB);

    // machine->ReadRegister(PCReg)
    // 6. Setup machine registers for child and save it to child thread
    machine->WriteRegister(PCReg, function); // PCReg
    machine->WriteRegister(NextPCReg, function+4); // NextPCReg
    machine->WriteRegister(PrevPCReg, function-4); // PrevPCReg
    childThread->SaveUserState();

     // 8. Restore register state of parent user-level process
    currentThread->RestoreUserState();
    // 7. Call thread->fork on child
    childThread->Fork(childFunction, childPCB->pid);
    // printf("[%d] [%d]", childPCB->pid, childThread->space->pcb->pid);

    // Read the program counter register
    // int pcreg = machine->ReadRegister(PCReg);

    // Log child creation information

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
        printf("Process [%d] cannot kill process [%d]: doesn't exist\n", currentThread->pid, pid);
        return -1; // Invalid PID
    }

    // 2. IF pid is self, then just exit the process
    if (pcb == currentThread->space->pcb) 
    {
        printf("Process [%d] killed process [%d]\n", currentThread->pid, pid);
        doExit(0); // Exit the current process
        return 0;
    }

    // 3. Valid kill, pid exists and not self, do cleanup similar to Exit
    // However, change references from currentThread to the target thread
    printf("Process [%d] killed process [%d]\n", currentThread->pid, pid);
    Thread* targetThread = pcb->thread; // Access the thread associated with the target PCB
    scheduler->RemoveThread(targetThread);

    // Perform cleanup actions for the target thread
    // targetThread->Finish(); // Mark the thread for destruction

    // 4. return 0 for success!
    return 0;
}


void doYield() {
    currentThread->Yield();
}

int doExec(char* filename) {
    // 1. Open the file and check validity
    OpenFile *executable = fileSystem->Open(filename);
    if (executable == NULL) {
        printf("Unable to open file %s\n", filename);
        return -1;
    }

    printf("Exec Program: [%d] loading %s\n", currentThread->pid, filename);
    // 2. Delete current address space but store current PCB first if using in Step 5.
    PCB* pcb = currentThread->space->pcb;

    delete currentThread->space;

    // 3. Create new address space
    AddrSpace* space = new AddrSpace(executable);

    delete executable;			// close file

    // 5. Check if Addrspace creation was successful
    if(space->valid != true) {
        printf("Could not create AddrSpace \n");
        return -1;
    }
    // 6. Set the PCB for the new addrspace - reused from deleted address space
    space->pcb = pcb;


    // 7. Set the addrspace for currentThread
    currentThread->space = space;
    // 8. Initialize registers for new addrspace
    space->InitRegisters();		// set the initial register values

    // currentThread->RestoreUserState();

    space->RestoreState();
    

    // 9. Initialize the page table
    // space->RestoreState();		// load page table register

    // 10. Run the machine now that all is set up
    machine->Run();			// jump to the user progam
    ASSERT(FALSE); // Execution nevere reaches here
    

    return 0;
}


char* translate(int virtualAddr) {
    int i = 0;
    char* str = new char[256];
    unsigned int physicalAddr = currentThread->space->Translate(virtualAddr);

    // Need to get one byte at a time since the string may straddle multiple pages that are not guaranteed to be contiguous in the physicalAddr space
    bcopy(&(machine->mainMemory[physicalAddr]),&str[i],1);
    while(str[i] != '\0' && i != 256-1)
    {
        virtualAddr++;
        i++;
        physicalAddr = currentThread->space->Translate(virtualAddr);
        bcopy(&(machine->mainMemory[physicalAddr]),&str[i],1);
    }
    if(i == 256-1 && str[i] != '\0')
    {
        str[i] = '\0';
    }

    return str;
}

void
ExceptionHandler(ExceptionType which)
{   
    
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
        printf("System Call: [%d] invoked Halt.\n", currentThread->pid);
        DEBUG('a', "Shutdown, initiated by user program.\n");
        interrupt->Halt();
    } else if ((which == SyscallException) && (type == SC_Yield)) {
        printf("System Call: [%d] invoked Yield.\n", currentThread->pid);
        DEBUG('a', "System Call: invoked Yield.\n");
        doYield();
        incrementPC();
    } else if ((which == SyscallException) && (type == SC_Exit)) {
        printf("System Call: [%d] invoked Exit.\n", currentThread->pid);
        DEBUG('a', "Exit system call initiated by user program with status %d.\n", machine->ReadRegister(4));
        doExit(machine->ReadRegister(4));
    } else if ((which == SyscallException) && (type == SC_Fork)) {
        printf("System Call: [%d] invoked Fork.\n", currentThread->pid);
        DEBUG('a', "Fork system call initiated by user program.\n");
        int ret = doFork(machine->ReadRegister(4));
        machine->WriteRegister(2, ret);
        incrementPC();
    }  else if ((which == SyscallException) && (type == SC_Exec)) {
        printf("System Call: [%d] invoked Exec.\n", currentThread->pid);
         DEBUG('a', "Exec system call initiated by user program.\n");
        int virtAddr = machine->ReadRegister(4);
        char* fileName = translate(virtAddr);
        int ret = doExec(fileName);
        machine->WriteRegister(2, ret);
        incrementPC();
    }else if ((which == SyscallException) && (type == SC_Join)) {
        printf("System Call: [%d] invoked Join.\n", currentThread->pid);
        DEBUG('a', "Join system call initiated by user program.\n");
        int ret = doJoin(machine->ReadRegister(4));
        machine->WriteRegister(2, ret);
        incrementPC();
    } else if ((which == SyscallException) && (type == SC_Kill)) {
        printf("System Call: [%d] invoked Kill.\n", currentThread->pid);
        DEBUG('a', "Kill system call initiated by user program.\n");
        int ret = doKill(machine->ReadRegister(4));
        machine->WriteRegister(2, ret);
        incrementPC();
    } else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}

