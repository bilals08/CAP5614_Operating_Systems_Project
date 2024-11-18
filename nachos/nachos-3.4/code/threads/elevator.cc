#include "copyright.h"
#include "system.h"
#include "synch.h"
#include "elevator.h"
#include "unistd.h"
int nextPersonID = 1;
Lock *personIDLock = new Lock("PersonIDLock");
ELEVATOR *e;

ELEVATOR::ELEVATOR(int numFloors) {
    this->numFloors = numFloors;
    currentFloor = 1;
    nextFloorDirection = 1;
    occupancy = 0;
    maxOccupancy = 5; // Max 5 people in the elevator at a time
    entering = new Condition*[numFloors];
    leaving = new Condition*[numFloors];
    personsWaiting = new int[numFloors];
    occupancyForFloor = new int[numFloors];
    elevatorLock = new Lock("ElevatorLock");

    // Initialize entering and leaving conditions
    for (int i = 0; i < numFloors; i++) {
        entering[i] = new Condition("Entering " + i);
        leaving[i] = new Condition("Leaving " + i);
        personsWaiting[i] = 0;
        occupancyForFloor[i] = 0;
    }
}

ELEVATOR::~ELEVATOR() {
    delete[] entering;
    delete[] leaving;
    delete[] personsWaiting;
    delete elevatorLock;
    delete []occupancyForFloor;
}

void ELEVATOR::start() {
    while (true) {
        elevatorLock->Acquire();


        // Signal people to leave the elevator if they are on the current floor
        leaving[currentFloor - 1]->Broadcast(elevatorLock);

        // Wait for people to leave
        while (occupancyForFloor[currentFloor - 1] > 0) {
            leaving[currentFloor - 1]->Signal(elevatorLock);
            occupancyForFloor[currentFloor - 1]--;
            occupancy--;
            currentThread->Yield();
        }

        // Signal people waiting on the current floor to enter the elevator
        while (occupancy < maxOccupancy && personsWaiting[currentFloor - 1] > 0) {
            entering[currentFloor - 1]->Signal(elevatorLock);
            occupancy++;
            occupancyForFloor[currentFloor - 1]++;
            personsWaiting[currentFloor - 1]--;
            currentThread->Yield();

        }

        elevatorLock->Release();

        // Simulate travel to the next floor
        printf("Elevator arrives on floor %d\n", currentFloor);
        for (int i = 0; i < 50; i++) {
            currentThread->Yield(); // Simulate 50-tick travel time
        }

        sleep(1);
            
        // Move to the next floor
        if (currentFloor == numFloors){
            nextFloorDirection = -1;
        }
        else if (currentFloor == 1){
            nextFloorDirection = 1;
        }
        currentFloor = currentFloor  + (nextFloorDirection);
    }
}

void ElevatorThread(int numFloors) {
    printf("Elevator with %d floors was created!\n", numFloors);

    e = new ELEVATOR(numFloors);
    e->start();
}

void Elevator(int numFloors) {
    Thread *t = new Thread("Elevator");
    t->Fork(ElevatorThread, numFloors);
}

void ELEVATOR::hailElevator(Person *p) {
    elevatorLock->Acquire();
    
    printf("Person %d wants to go from floor %d to floor %d\n", p->id, p->atFloor, p->toFloor);

    personsWaiting[p->atFloor - 1]++;
    // Wait for the elevator to arrive at the requested floor
    entering[p->atFloor - 1]->Wait(elevatorLock);

    // Person enters the elevator
    printf("Person %d got into the elevator.\n", p->id);
    // occupancy++;
    // personsWaiting[p->atFloor - 1]--;

    // Wait until the elevator reaches the destination floor
    leaving[p->toFloor - 1]->Wait(elevatorLock);

    // Person leaves the elevator
    printf("Person %d got out of the elevator.\n", p->id);
    occupancy--;

    elevatorLock->Release();
}

int getNextPersonID() {
    personIDLock->Acquire();
    int personID = nextPersonID++;
    personIDLock->Release();
    return personID;
}

void PersonThread(int personArg) {
    Person *p = (Person *)personArg;

    e->hailElevator(p);
}

void ArrivingGoingFromTo(int atFloor, int toFloor) {
    Person *p = new Person;
    p->id = getNextPersonID();
    p->atFloor = atFloor;
    p->toFloor = toFloor;

    // Create person thread
    Thread *t = new Thread("Person " + p->id);
    t->Fork(PersonThread, (int)p);
}
