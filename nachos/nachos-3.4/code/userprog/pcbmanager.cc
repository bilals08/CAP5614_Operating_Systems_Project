#include "pcbmanager.h"

PCBManager::PCBManager(int maxProcesses) {
    bitmap = new BitMap(maxProcesses);
    pcbs = new PCB*[maxProcesses];
    pcbManagerLock = new Lock("pcbManager");

    for(int i = 0; i < maxProcesses; i++) {
        pcbs[i] = NULL;
    }
}

PCBManager::~PCBManager() {
    delete bitmap;
    delete pcbs;
}

PCB* PCBManager::AllocatePCB() {
    pcbManagerLock->Acquire();
    int pid = bitmap->Find();
    pcbManagerLock->Release();
    ASSERT(pid != -1);
    pcbs[pid] = new PCB(pid);
    return pcbs[pid];
}

bool PCBManager::IsValidPID(int pid) {
    return (pid >= 0 && pcbs[pid] != nullptr);
}

int PCBManager::DeallocatePCB(PCB* pcb) {
    // Check of pcb is valid -- check pcbs for pcb->pid
    if (!IsValidPID(pcb->pid)) {
        return -1;
    }

    pcbManagerLock->Acquire();
    
    bitmap->Clear(pcb->pid);
    
    pcbManagerLock->Release();

    int pid = pcb->pid;
    
    delete pcbs[pcb->pid];

    pcbs[pid] = NULL;

    return 0;
}

PCB* PCBManager::GetPCB(int pid) {
    if (!IsValidPID(pid)) {
        return nullptr;
    }
    
    pcbManagerLock->Acquire();
    PCB* pcb = pcbs[pid];
    pcbManagerLock->Release();
    
    return pcb;
}
