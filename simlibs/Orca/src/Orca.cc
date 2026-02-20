#include "inet/transportlayer/tcp/flavours/TcpBaseAlg.h"
#include "inet/transportlayer/tcp/flavours/TcpNewReno.h"
#include "omnetpp/simtime_t.h"
#ifdef ORCA
#include "Orca.h"
#include "typedefs.h"
#include <inet/common/INETDefs.h>
#include "inet/common/InitStages.h"

using namespace inet::tcp;
using namespace inet;
using namespace learning;

Register_Class(Orca); // Lets omnet see and use this class

Orca::Orca():
    TcpNewReno(), RLInterface() {
    if (debug) cout << "\tOrca: Constructor called!";
}
Orca::~Orca() {
    if (debug) cout << "\tOrca: Destructor method called. Goodbye.";
    // if (RLStep) {
    //     delete cancelEvent(RLStep);
    // }
    getSimulation()->getSystemModule()->unsubscribe(stringId.c_str(), (cListener*) this);
    getSimulation()->getSystemModule()->unsubscribe("actionResponse", (cListener*) this);
}

// CWND is the current congestion window size, dictating how many packets are allowed in-flight
// ssthresh is the CWND value at which the window's growth switches from exponential (slow start) to linear (congestion avoidance)
    // Think of ssthresh as "remembering" the safe capacity of the network. It tells the connection when to slow down CWND growth as it is approaching previous levels of loss.


// ----- Slow Start (exponential) -----
// CWND increases exponentially with each ACK 
// This continues until CWND exceeds ssthresh (or loss is detected)
// At this point, slow start transitions to congesiton avoidance (growth slows from exponential to linear)

// ----- Congestion Avoidance (linear) -----
// CWND increases linearly with each ACK
// This continues until loss is detected 
// 

// ----- Timeout Event (reset) -----
// No ACKs for a given timeout period triggers this event
// CWND is reset to 1, and 

// (Generally) cuts ssthresh in half, making the slower, linear increase begin sooner
void Orca::recalculateSlowStartThreshold()
{
    //if (debug) cout << "\tOrca recalculateSlowStartThreshold()" << endl;
    uint32_t flight_size = std::min(state->snd_cwnd, state->snd_wnd); 
//    uint32_t flight_size = state->snd_max - state->snd_una;
    state->ssthresh = std::max((uint32_t) (flight_size / 2.0), 2 * state->snd_mss);
    //conn->emit(ssthreshSignal, state->ssthresh);
}

// Timeout - Reset cwnd, reduce ssthresh, enter slow start
void Orca::processRexmitTimer(TcpEventCode& event)
{
    TcpNewReno::processRexmitTimer(event);
}

// ACK received - increase cwnd and ssthresh
void Orca::receivedDataAck(uint32_t firstSeqAcked)
{
    TcpNewReno::receivedDataAck(firstSeqAcked);
    // // TODO: update the average RTT here
    // simtime_t segmentRTT = this->rtt;
    // this->orcaDelay = (this->orcaDelay * this->orcaACKTotal + segmentRTT.dbl()) / (this->orcaACKTotal + 1);
    // this->orcaACKTotal += 1;
}

// Duplicate ACK received - attempt a fast restransmit
void Orca::receivedDuplicateAck()
{
    TcpNewReno::receivedDuplicateAck();
}

// bool Orca::sendData(bool sendCommandInvoked) {
//     bool b;
//     uint32_t oldSndMax, newSndMax;

//     oldSndMax = state->snd_max;
//     b = TcpBaseAlg::sendData(sendCommandInvoked);
//     newSndMax = state->snd_max;
//     this->bytesSentTotal = newSndMax - oldSndMax;
//     EV_INFO << "Sent " << newSndMax - oldSndMax << " bytes" << std::endl;
//     return b;
// }

// Called upon a valid ACK received (?); Grab the RTT measured and use it to update the current interval's average (may be faster to store all values and average at the end of the interval)
void Orca::rttMeasurementComplete(simtime_t tSent, simtime_t tAcked) {
    TcpNewReno::rttMeasurementComplete(tSent, tAcked);
    double packetRTT = (tAcked-tSent).dbl();
    this->orcaDelay = (this->orcaDelay * (double) rttReportCount + packetRTT) / (rttReportCount + 1);
    this->rttReportCount += 1;
}




// // RayNet: Called to initalize the agent
void Orca::initialize() {
    if (debug) cout << "\tOrca initialize()" << endl;
    int _stateSize = this->conn->getTcpMain()->par("stateSize");;
    int _maxObsCount = this->conn->getTcpMain()->par("maxObsCount");
    
    this->maxRLSteps = this->conn->getTcpMain()->par("maxRLSteps");
    debug = this->conn->getTcpMain()->par("printDebugMessages");

    // provide the RLInterface with a cComponent API (to use signaling functionality)
    setOwner((cComponent*) conn->getTcpMain());
    
    // Initalize parent classes
    // RLInterface::initialize(_stateSize, _maxObsCount); // Deprecated initialization function. Delete this later.
    RLInterface::initialise();
    TcpNewReno::initialize();

    // Set the RL ID of this component (for use by the training script). Ensure this is unique for multi-agent environments (perhaps use the IP of the host?)
    std::string s("Orca");
    setStringId(s);
    
    // Register this agent with RayNet
    cObject* simtime = new cSimTime(this->conn->getTcpMain()->par("monitorIntervalDuration"));
    owner->emit(this->registerSig, stringId.c_str(), simtime); 

    // Schedule the first RL step
    // RLStep = new cMessage("RLSTEP");
    // conn->scheduleAt(simTime() + RLStepInterval, RLStep);
}

// OMNeT method that catches timers set by scheduleAt() and similar. Necessary for self-scheduling events.
// void Orca::processTimer(cMessage *timer, TcpEventCode &event) {
//     if (timer == RLStep) {
//         if (debug) cout << "\tOrca: Performing an RLStep!" << endl;
//         owner->emit(senderToStepper, this, new cString(stringId)); // Request the action! Maybe pass self?

//         // Schedule another RL step and increment the RLStep counter
//         conn->scheduleAt(simTime() + RLStepInterval, RLStep);
//         RLStepsTaken++;
//         if (RLStepsTaken > 100) {
//             if (debug) cout << "\t\tWE ARE DONE! " << RLStepsTaken << " STEPS TAKEN!" << endl;
//             done = true;
//         }
//     } else {
//         TcpNewReno::processTimer(timer, event);
//     }
// }


// OMNet Method? Called after component initialization is complete?
void Orca::established(bool active) {
    if (debug) cout << "\tOrca: established()" << endl;
    TcpNewReno::established(active);

    if (active) {
        std::string s("Orca");
        setStringId(s);
        this->isActive = active;
    }
}







// Perform and observation and store the result into the provided vector (or append to it, if you're keeping history)
ObsType Orca::computeObservation(){
    if (debug) cout << "\tOrca: computeObservation()" << endl; 
    
    this->orcaIntervalDuration = (simTime() - this->lastIntervalTime).dbl();
    this->orcaThroughput = (state->snd_max - this->lastIntervalSentBytes) / this->orcaIntervalDuration;
    this->orcaLossRate=0.0;             // Track total sent and total lost. Perform final division here.
    this->orcaACKTotal= state->snd_una - this->lastIntervalSndUna;  // Check how many ACK's occured this interval (see how many packets snd_una has increased by)
    this->orcaSRTT = state->srtt.dbl();
    this->orcaCwnd = (double) state->snd_cwnd;
    this->orcaMaxThroughput = std::max(this->orcaMaxThroughput, this->orcaThroughput);
    if (this->rttReportCount > 0) {
        // Only update the minDelay if an ACK has been receieved this interval.
        // This is done to prevent division by zero.
        // At some point I need to implement skipping if this happens.
        this->orcaMinDelay = std::min(this->orcaMinDelay, this->orcaDelay);
    }
    this->maxCwnd = std::max(this->maxCwnd, this->orcaCwnd);
    this->maxACKTotal = std::max(this->maxACKTotal, this->orcaACKTotal);

    // Should I update these in resetStepVariables? How much later is that called?
    this->lastIntervalSndUna = state->snd_una;
    this->lastIntervalSentBytes = state->snd_max;
    this->lastIntervalTime = simTime();
    return {this->orcaThroughput / this->orcaMaxThroughput,
            this->orcaLossRate, // not implemented yet
            this->orcaDelay / this->orcaMinDelay,
            this->orcaACKTotal / this->maxACKTotal, 
            this->orcaIntervalDuration, 
            this->orcaSRTT / this->orcaMinDelay, 
            this->orcaCwnd / this->maxCwnd,
            this->orcaMaxThroughput, 
            this->orcaMinDelay
        };
}

RewardType Orca::computeReward(){
    if (debug) cout << "\tOrca: computeReward()" << endl;

    // Do not compute a reward if no ACKs were received. No ACKs means no throughput, no valid RTT measurement, etc.
    if (this->rttReportCount == 0) {
        return RewardType(0.0);
    }
    
    double maxPossiblePower = (this->orcaMaxThroughput/this->orcaMinDelay);
    double currentPower = this->orcaThroughput / this->orcaDelay; // TODO: clamp the delay to minDelay if it is within some threshold (minDelay*beta)
    double normalizedPower = currentPower / maxPossiblePower;

    return RewardType(normalizedPower);
}

// RayNet method: Make a decision based on the policy (alter snd_cwnd)
void Orca::decisionMade(ActionType action) {
    if (debug) cout << "\tOrca: decisionMade()" << endl;

    if (!isnan(action) && isActive) {
        if (debug) cout << "\t\tAction received: " << action << endl;

        if (isReset) {
            if (debug) cout << "\t\tOrca currently resetting, will not take action" << endl;
        } else {
            // Change the current cwnd based on the action. Do not let it drop below the maximum segment size.
            if (this->orcaACKTotal == 0) {
                if (debug) cout << "No packets ACK'd this interval. Skipping action, cwnd staying at " << state->snd_cwnd << endl;
            } else {
                double fakeAction = action;
                uint32_t newCwnd = ceil(std::pow(2.0, fakeAction) * (double) state->snd_cwnd);
                newCwnd =  max(state->snd_mss, newCwnd);
                if (debug) cout << "\tAction:" << endl;
                if (debug) cout << "\t\tcwnd changing from " << state->snd_cwnd << " to " << newCwnd << endl;
                state->snd_cwnd = newCwnd;

                // Change the stepSize to be 1 RTT (based on srtt)
                // cObject* newStepSizeObj = new cSimTime(state->srtt.dbl());
                // cout << "\t\tChanging step size to " << newStepSizeObj << endl;
                
                // owner->emit(this->modifyStepSizeSig, stringId.c_str(), newStepSizeObj); 

                this->modifyStepSize(state->srtt.dbl());
            }
        }

        RLStepsTaken++;
        if (debug) cout << "\t\tRLSteps taken: " << RLStepsTaken << endl;
        if (RLStepsTaken >= this->maxRLSteps) {
            if (debug) cout << "\t\tWE ARE DONE! " << RLStepsTaken << " STEPS TAKEN!" << endl;
            done = true; // Don't set done yourself. Unsure of the correct way to handle this, but this isn't it.
        }
    }
    else {
        EV_ERROR << action << " value in decisionMade() function" << std::endl;
    }
}


void Orca::resetStepVariables()
{
    if (debug) cout << "\t\tOrca: resetStepVariables()" << endl;
    this->orcaThroughput=0.0;    // The average delivery rate (throughput) over the last interval
    this->orcaLossRate=0.0;      // The average loss rate of packets over the last interval
    this->orcaDelay=0.0;         // The average delay of packets over the last interval
    this->orcaACKTotal=0.0;      // The number of valid acknowledgements over the last interval
    this->orcaIntervalDuration=0.0;  // The simtime elapsed over the last interval

    this->rttReportCount=0; // The number of RTT values we have measured over the last interval
}

// Returns true if the agent is reporting this episode as complete. (Pretty sure this is never called. Just set done to true directly during an RLStep.)
bool Orca::getDone() {
    if (debug) cout << "Orca getDone(): If you're seeing this, getDone() probably isn't deprecated.";
    bool done = RLStepsTaken > 1000;
    if (debug) cout << "\tOrca: " << RLStepsTaken << " steps completed. Returning " << done << endl;
    return done;
}

// RayNet method: Called after simulation completion? Unsure how this differs from reset()
void Orca::cleanup()
{
    if (debug) cout << "\tOrca: cleanUp()" << endl;
}

ObsType Orca::getRLState(){
    if (debug) cout << "\tOrca: getRLState()" << endl;
    // Deprecated, remove this later
}

RewardType Orca::getReward(){
    if (debug) cout << "\tOrca: getReward()" << endl;
    // Deprecated, remove this later
}


#endif