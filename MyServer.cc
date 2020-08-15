#include "MyServer.h"
#include "MyPacket_m.h"




void MyServer::initialize()
{
    packetSentSignal = registerSignal("packet_sent"); ///register the signal to record the number of packets which are sent
    /* Set the name of the Finite State Machine */
    fsm.setName("fsm"); ///define the finite state machine

    /* Schedule the very first message that will trigger the FSM to move from the INIT state */
    scheduleAt(simTime(), new cMessage("init"));
     /* By setting setDeliverOneReceptionStart to true, the receiving module will receive the packet as soon as the first bit is received.
     * In this way it is possible to extract the packet duration (thus, the time that it takes to receive the entire packet)
     * and model possible collisions */
    collision=false; ///set the initial collision variable to false///
    gate("in")->setDeliverOnReceptionStart(true); //to model collisions we need to find out as soon as the first bit arrives

    /* Extract the values from the .ned file */
    txRate = 1000000; ///set the transmission rate
    propagationDelay = 0.000001; ////set the propagation delay
    ///////Throughput configurations///////
    countBits=0; /// reset the counter bits for throughput
    instThroughput = registerSignal("instantaneous_throughput"); ///register the signal to record instantaneous throughput
    cMessage *msg = new cMessage("selfThroughput"); ///generate a self message for timer
    scheduleAt(simTime()+1, msg); /////schedule the self message for next second

};

void MyServer::handleMessage(cMessage *msg)
{
    /* cFSM are managed through "switch" statement. These are some rules to be kept in mind when using cFSM:
     * 1) Steady states have Enter and Exit routines
     * 2) Transient states have only Exit routines
     * 3) All Exit routines shall implement a Goto statement (otherwise the FSM doesn't know where to go next)
     * 4) Suggestion: use only one switch statement per FSM. Then, if it is needed to discriminate among self and outside messages,
     *    it can be done directly into each case.
     * 5) INIT transient state is mandatory
    */
    /////recognize if the receiving message belongs to throughput timer////
    if(msg->isSelfMessage() && strcmp(msg->getName(),"selfThroughput")==0){
          emit(instThroughput, countBits/1000); /// count the instantaneous throughput in Kbps and record it by signal
          countBits=0; ///reset the counter to for next second
    }
    else{ //// triggers the finite sate machine if the message doesn't belong to throughput

    FSM_Switch(fsm)
    {
        case FSM_Exit(INIT): /// initial state
         {
            delete msg;
            FSM_Goto(fsm,SLEEP); ///change to sleep state
            break;
         }
        case FSM_Enter(SLEEP):
         {
           collision=false; ////reset the collision variable to false every time the server is waiting for messages
           break;
         }
        case FSM_Exit(SLEEP):
         {
            ////if a packet arrives at server
          if(!msg->isSelfMessage()){
            Packet = check_and_cast<cPacket *>(msg); //casting the packet to a cPacket type and store it in a class variable
            senderModule = Packet->getSenderModule();//saving the packet sender module in a class variable of cModule type
            duration=new cMessage("Duration"); ///generate a message for the time a packet fully arrives
            scheduleAt(simTime()+Packet->getDuration(),duration);// schedule a self message for the time that Last bit arrives
             EV << Packet << " is receiving" <<"\n";
             FSM_Goto(fsm,RECEIVE_PKT); //// change the state to receiving packet
            }
            break;
         }
        case FSM_Enter(RECEIVE_PKT): ///must be defined because of rules of defining finite state machine but no commands
        {
            break;
        }
        case FSM_Exit(RECEIVE_PKT):
        {
            /* This case will always be triggered by a self message (the one informing about the end of packet reception).
             * In case it is not a self message, we made a mistake, and ASSERT can be used to track these mistakes.
             * */
            ///// checking if collision occurs during receiving a packet ////
              if(!msg->isSelfMessage()){ /// if during receiving a packet another packet arrives in server
                    EV << getName() << " Packet Collision Occured! " << "\n";
                    collision=true; ////set the class variable to true if the collision happens
                    cPacket *pkt = check_and_cast<cPacket *>(msg); /// cast the collided packet to a cPacket to get the duration time
                    cancelEvent(duration); /// cancel the previous finish duration event
                    scheduleAt(simTime()+pkt->getDuration(),duration); //reschedule again the finish duration time
                    EV << pkt << " is deleted!" << "\n";
                    EV << Packet->getName() << " is deleted!" << "\n";
                    /// deleting the collided packets////
                    delete pkt;
                    Packet=NULL;
            }
            else if(msg->isSelfMessage()){ ////when the duration time finishes
                    if(collision){ ///if collision happened
                    FSM_Goto(fsm,SLEEP); ///change to sleep mode
                    }else { ////if collision hasn't happened
                     EV << Packet << " received completely" << "\n";
                     countBits+=Packet->getBitLength(); ////count the number of received bits if the packet is
                                                      /////completely received without collision
                     FSM_Goto(fsm,ACK_SEND); //// change the state to ack send
                     }
               cancelAndDelete(duration); ////delete the finish duration event after at the end of the receive packet state
            }
            break;
        }
         case FSM_Exit(ACK_SEND):
         {
             /* This case will always be triggered by a self message (the one informing about the end of packet reception).
              * In case it is not a self message, we made a mistake, and ASSERT can be used to track these mistakes.*/
             cPacket *ack=new cPacket(Packet->getName()); ////create a new packet to send acknowledgment
             ack->setBitLength(200); ///set the acknowledgment packet size
             double packetDuration = ack->getBitLength()/txRate; ///calculate packet duration
             sendDirect(ack, propagationDelay, packetDuration, senderModule->gate("in")); ///sendDirect the acknowledgment to the sender module
             Packet=NULL; ///delete the received  packet
             FSM_Goto(fsm,SLEEP); ///change the state to sleep mode
             break;
         }

    }
    }
};

