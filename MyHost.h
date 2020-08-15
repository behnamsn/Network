#include <omnetpp.h>
#include "MyPacket_m.h"

using namespace omnetpp;

class MyHost : public cSimpleModule
{

    protected:
        // FSM and its states
        cFSM fsm;
        enum
        {
             INIT = 0,
             SLEEP = FSM_Steady(1),
             ACK_WAIT = FSM_Steady(2),
             BACKOFF= FSM_Steady(3),
             PKT_SEND = FSM_Transient(1),
        };

        virtual void initialize();
        virtual void handleMessage(cMessage *msg);
    private:
        cGate *targetGate;
        double propagationDelay;
        double txRate;
        int seq;
        MyPacket *Packet;
        cMessage *timeoutevent;
        simsignal_t packetSentSignal;


};

Define_Module(MyHost); // MANDATORY to define the module
