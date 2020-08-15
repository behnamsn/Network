#include <omnetpp.h>
#include "MyPacket_m.h"

using namespace omnetpp;

class MyServer : public cSimpleModule
{

    protected:
        // FSM and its states
        cFSM fsm;
        enum
        {
             INIT = 0,
             SLEEP= FSM_Steady(1),
             RECEIVE_PKT = FSM_Steady(2),
             ACK_SEND = FSM_Transient(1),
        };

        virtual void initialize();
        virtual void handleMessage(cMessage *msg);
    private:
        cGate *targetGate;
        int countBits;
        double propagationDelay;
        double txRate;
        bool collision;
        cPacket *Packet;
        cMessage *duration;
        simsignal_t packetSentSignal;
        simsignal_t instThroughput;
        cModule *senderModule;


};

Define_Module(MyServer); // MANDATORY to define the module
