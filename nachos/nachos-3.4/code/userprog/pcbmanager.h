#ifndef PCBMANAGER_H
#define PCBMANAGER_H

#include "bitmap.h"
#include "pcb.h"
#include "synch.h"
#include "bitmap.h"

class PCBManager {
    public:
        PCBManager(int maxProcesses);
        ~PCBManager();
        PCB* AllocatePCB();
        bool IsValidPID(int pid);
        int DeallocatePCB(PCB* pcb);
        PCB* GetPCB(int pid);

    private:
        BitMap* bitmap;
        PCB** pcbs;
        Lock* pcbManagerLock;


};

#endif