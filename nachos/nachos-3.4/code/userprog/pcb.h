#ifndef PCB_H
#define PCB_H

#include "list.h"
class Thread;

class PCB {
    public: 
        PCB(int id);
        ~PCB();
        int pid;
        PCB* parent;
        List* children;
        Thread* thread;
        int exitStatus;

        void AddChild(PCB* child);
        int RemoveChild(PCB* child);
        bool HasExited();
        void DeleteExitedChildrenSetParentNull();

};

#endif