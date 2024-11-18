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
        void AddChild(PCB* child);
        int RemoveChild(PCB* child);

};

#endif