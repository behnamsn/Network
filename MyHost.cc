#include "MyHost.h"
#include "MyPacket_m.h"

void MyHost::initialize()
{
    packetSentSignal = registerSignal("packet_sent");///register the signal to record the number of packets which are sent
    seq=0; ///reset the sequence number of packets which are generated
    /* Set the name of the Finite State Machine */
    fsm.setName("fsm");

    /* Schedule the very first message that will trigger the FSM to move from the INIT state */
    scheduleAt(simTime(), new cMessage("init"));

    /* server shall send to host and viceversa */
    //// set the gate of module that the packet is going to be sent
        targetGate = getModuleByPath("server")->gate("in");
        /// generating a new packet with a name and sequence number
        char msgname[20];                                 ////defining the name of the packet
        sprintf(msgname, "host%d-%d",getIndex(), ++seq);
        Packet = new MyPacket(msgname); ///generating a new packet and saving it in a class variable
        Packet->setBitLength(1500); ///set the packet bit length

    /* Extract the values from the .ned file */
    txRate = 1000000; //// set the transmission rate
    propagationDelay = 0.000001; ////set the  propagation delay

};

void MyHost::handleMessage(cMessage *msg)
{
    /* cFSM are managed through "switch" statement. These are some rules to be kept in mind when using cFSM:
     * 1) Steady states have Enter and Exit routines
     * 2) Transient states have only Exit routines
     * 3) All Exit routines shall implement a Goto statement (otherwise the FSM doesn't know where to go next)
     * 4) Suggestion: use only one switch statement per FSM. Then, if it is needed to discriminate among self and outside messages,
     *    it can be done directly into each case.
     * 5) INIT transient state is mandatory
    */
    FSM_Switch(fsm)
    {
        case FSM_Exit(INIT): /// initial state
         {
             delete msg;
             FSM_Goto(fsm,SLEEP); ///change to sleep state
             break;
         }
        case FSM_Enter(SLEEP):
          { ///check if the packet has already been generated or not
            if(Packet==NULL){ ///genrate a new packet if the packet has not been generated so far
                char msgname[20];                  ////defining the name of the packet
                sprintf(msgname, "host%d-%d",getIndex(), ++seq);
                Packet = new MyPacket(msgname); ///generating a new packet and saving it in a class variable
                Packet->setBitLength(1500);   ///set the packet bit length
            }
            cMessage *msg = new cMessage("randomtime"); //// generate a self message for the sending time of a packet
            scheduleAt(simTime()+exponential(0.7),msg); ////scheduling a time which the packet must be sent
            break;
          }
        case FSM_Exit(SLEEP):
          {
            if(strcmp(msg->getName(),"randomtime")==0){ /// if the self messages arrives for sending the packet
               FSM_Goto(fsm,PKT_SEND);/// change state to sending packet
                }
            break;
          }
         case FSM_Exit(PKT_SEND):
         {
             EV << Packet << " is being sent" << "\n";
             /* In this case it is very important to set the packet duration. The packet duration is equivalent to
              * the transmission or reception time. This value depend directly on the length of the packet and inversely on the
              * transmission rate. */
             //// compute duration of the packet
             double packetDuration = Packet->getBitLength()/txRate; ////
             Packet->setReTx(Packet->getReTx()+1); ///increment the reTx in the Packet of MyPacket type
             MyPacket *copy=(MyPacket *)Packet->dup();////create a copy of the packet by dup()
             /* Prototype of sendDirect:
              * virtual void sendDirect(cMessage *msg, simtime_t propagationDelay, simtime_t duration, cGate *inputGate);*/
             sendDirect(copy, propagationDelay, packetDuration, targetGate);///sendDirect the Packet to the server gate
             emit(packetSentSignal, copy); //// record the number of packets which are transmitted
             FSM_Goto(fsm,ACK_WAIT);
             break;
         }


         case FSM_Enter(ACK_WAIT):
         {
             /* Wait for packet to be received */
             timeoutevent = new cMessage("timeoutEvent"); /// generate a message for timeout
             scheduleAt(simTime()+2,timeoutevent); ////schedule an event for the time that a timeout expires
             break;
         }

         case FSM_Exit(ACK_WAIT):
         {
             if(msg->isSelfMessage()){ /// if the timeout event is reached and the acknowledge hasn't arrived yet
                 Packet->setReTx(Packet->getReTx()+1);///increment the reTx variable every time timeout expires
                 delete msg; ///delete the self message
                 FSM_Goto(fsm,BACKOFF);//// change the state to back off
                 }
             else if(!msg->isSelfMessage()){ /// if acknowledge arrives before finishing the timeout
                 EV << msg << " acknowledge HAS RECEIVED!" << "\n";
                 cancelAndDelete(timeoutevent);////delete the timeout event
                 Packet=NULL; /// delete the packet when it is received
                 FSM_Goto(fsm,SLEEP); /// change state to sleep mode
                }
             break;
         }
         case FSM_Enter(BACKOFF):
         {
             /* Wait for packet to be received */
             cMessage *msg = new cMessage("retransmission"); /// generating a new self message for retransmission
             scheduleAt(simTime()+uniform(0,0.5*Packet->getReTx()),msg); /// scheduling a new time for retransmission
             break;
         }
         case FSM_Exit(BACKOFF):
         {
              //// waiting for the  retransmission event
             if(msg->isSelfMessage() && strcmp(msg->getName(),"retransmission")==0){
                 EV << Packet << " timeout reached! Resending "<<Packet << "\n";
                 FSM_Goto(fsm,PKT_SEND); ///goto pkt_send to resend a copy of the packet
                 delete msg;
             }else if(!msg->isSelfMessage()){ ///  when the acknowledgment arrives
                 EV << msg << " acknowledge HAS RECEIVED!" << "\n";
                 delete msg; ///deleting the self message
                 Packet=NULL; /// deleting the packet
                 FSM_Goto(fsm,SLEEP);/// goto sleep mode
             }

             break;
         }


    }
};

