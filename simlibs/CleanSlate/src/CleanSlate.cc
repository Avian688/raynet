#include "omnetpp/ccomponent.h"
#include "omnetpp/simtime_t.h"
#include "transportlayer/tcp/TcpPacedConnection.h"
#include "inet/transportlayer/tcp/flavours/TcpNoCongestionControl.h"
#include "transportlayer/tcp/flavours/TcpPacedFamily.h"
#include <numeric>
#ifdef CLEANSLATE
#include "CleanSlate.h"
#include "typedefs.h"
#include <inet/common/INETDefs.h>

using namespace inet::tcp;
using namespace inet;
using namespace learning;

Register_Class(CleanSlate); // Lets omnet see and use this class

CleanSlate::CleanSlate():
    TcpNoCongestionControl(), RLInterface() {
    if (debug) cout << "\tCleanSlate: Constructor called!";
}

CleanSlate::~CleanSlate() {
    if (debug) cout << "\tCleanSlate: Destructor method called. Goodbye.";
    getSimulation()->getSystemModule()->unsubscribe(stringId.c_str(), (cListener*) this);
    getSimulation()->getSystemModule()->unsubscribe("performAction", (cListener*) this);
    
}


// // RayNet: Called to initalize the agent
void CleanSlate::initialize() {
    if (debug) cout << "\tCleanSlate initialize()" << endl;
    this->rewardDelayForgiveness = this->conn->getTcpMain()->par("rewardDelayForgiveness");
    this->rewardLossMultiplier = this->conn->getTcpMain()->par("rewardLossMultiplier");
    this->maxRLSteps = this->conn->getTcpMain()->par("maxRLSteps");
    debug = this->conn->getTcpMain()->par("printDebugMessages");
    takeActions = this->conn->getTcpMain()->par("takeActions");

    // provide the RLInterface with a cComponent API (to use signaling functionality)
    setOwner((cComponent*) conn->getTcpMain());
    
    // Initalize parent classes
    // RLInterface::initialize(_stateSize, _maxObsCount); // Deprecated initialization function. Delete this later.
    RLInterface::initialise();
    TcpNoCongestionControl::initialize();

    // Set the RL ID of this component (for use by the training script). Ensure this is unique for multi-agent environments (perhaps use the IP of the host?)
    std::string s("CleanSlate");
    setStringId(s);
    
    // Register this agent with RayNet
    cObject* simtime = new cSimTime(this->conn->getTcpMain()->par("monitorIntervalDuration"));
    owner->emit(this->registerSig, stringId.c_str(), simtime); 
    scheduleNextStep(this->initialStepLength);
    // Schedule the first RL step
    // RLStep = new cMessage("RLSTEP");
    // conn->scheduleAt(simTime() + RLStepInterval, RLStep);
}

// OMNet Method? Called after component initialization is complete?
void CleanSlate::established(bool active) {
    state->snd_cwnd = 6000;
    if (debug) cout << "\tCleanSlate: established()" << endl;
    TcpNoCongestionControl::established(active);
    //dynamic_cast<TcpPacedConnection*>(conn)->subscribe(dynamic_cast<TcpPacedConnection*>(conn)->retransmissionRateSignal, (cListener*) this);
    if (active) {
        std::string s("CleanSlate");
        setStringId(s);
        this->isActive = active;
    }
}







// Perform and observation and store the result into the provided vector (or append to it, if you're keeping history)
ObsType CleanSlate::computeObservation(){
    if (debug) cout << "\tCleanSlate: computeObservation()" << endl; 
    if (debug) cout << "\tCleanSlate: cwnd=" << state->snd_cwnd << endl;
    
    //dynamic_cast<TcpPacedConnection*>(conn)->computeRetransmissionRate(); // Updates this->retransmissionBytes via TcpPaced Connection
    double delta_snd_max = state->snd_max - this->last_snd_max;
    double delta_snd_una = state->snd_una - this->last_snd_una;
    this->cleanslateIntervalDuration = (simTime() - this->lastIntervalTime).dbl();

    // Throughput: How many bytes were DELIVERED this interval (basically goodput?)
    this->cleanslateThroughput = delta_snd_una / this->cleanslateIntervalDuration;
    this->cleanslateMaxThroughput = std::max(this->cleanslateMaxThroughput, this->cleanslateThroughput);

    // Lossrate: What percentage of bytes sent this interval were retransmissions
    // this->cleanslateLossRate = 0.0;
    // if (this->retransmissionRate > 0.0) {  // Avoid division by 0
    //     double transmissionRate = delta_snd_max/this->cleanslateIntervalDuration; // How many non-retransmits occurred this interval
    //     this->cleanslateLossRate = this->retransmissionRate / (this->retransmissionRate + transmissionRate);
    // }

    // ACKed: How many bytes were ACKed this interval (basically raw goodput?)
    this->cleanslateACKTotal= delta_snd_una;
    this->maxACKTotal = std::max(this->maxACKTotal, this->cleanslateACKTotal);

    // SRTT: Smoothed round trip time. Already tracked by TCP.
    this->cleanslateSRTT = state->srtt.dbl();
    if (this->cleanslateSRTT > 0.0) {
        this->cleanslateMinDelay = std::min(this->cleanslateMinDelay, this->cleanslateSRTT);
    }

    // CWND: Size of the congestion window. Already tracked by TCP.
    this->cleanslateCwnd = (double) state->snd_cwnd;
    this->maxCwnd = std::max(this->maxCwnd, this->cleanslateCwnd);

    // Delay Metric: The delay metric is treated as optimal if within the forgiveness window. Otherwise, have it slowly decrease as delay inflates.
    this->cleanslateDelayMetric = 1.0; 
    if (this->cleanslateSRTT > this->cleanslateMinDelay * this->rewardDelayForgiveness) {                                   
        this->cleanslateDelayMetric = this->cleanslateMinDelay * this->rewardDelayForgiveness / this->cleanslateSRTT;
    }

    if(debug) {
        cout << "-" << endl;
        cout << "-" << endl;
        cout << "\t\tState:" << endl;
            cout << "\t\t\tsnd_una: " << state->snd_una << endl;
            cout << "\t\t\tdelta_snd_una: " << delta_snd_una << endl;
            cout << "\t\t\tcwnd: " << state->snd_cwnd << endl;
            cout << "\t\t\tsnd_max: " << state->snd_max << endl;
        cout << "\t\tObservations:" << endl;
            cout << "\t\t\tThroughput: " << this->cleanslateThroughput / this->cleanslateMaxThroughput << endl;
            cout << "\t\t\tPacerate: " << this->cleanslatePaceRate / this->cleanslateMaxThroughput << endl;
            cout << "\t\t\tlossrate: " << -9 << endl;
            cout << "\t\t\tACKs: " << this->cleanslateACKTotal /  state->snd_cwnd << endl;
            cout << "\t\t\tMTP Duration: " << this->cleanslateIntervalDuration << endl;
            cout << "\t\t\tSRTT: " << this->cleanslateMinDelay / this->cleanslateSRTT << endl;
            cout << "\t\t\tDelay Metric: " << this->cleanslateDelayMetric  << endl;
        
    } 

    if (state->srtt.dbl() == 0) {
        return {0,0,0,0,0,0,0}; // Schedule the next RLStep
    }
    return {this->cleanslateThroughput / this->cleanslateMaxThroughput,     // Normalized throughput
            this->cleanslatePaceRate / this->cleanslateMaxThroughput,       // Normalized pacerate
            -9, // Normalized lossrate
            this->cleanslateACKTotal /  state->snd_cwnd,              // Normalized ACKs count (maybe use tcp_cwnd? ask aiden)     
            this->cleanslateIntervalDuration,                         // Monitor interval duration
            this->cleanslateMinDelay / this->cleanslateSRTT,                // Normalized SRTT (delay)
            this->cleanslateDelayMetric                               // Normalized SRTT (possibly forgiven, if within the forgiveness window)
        };

    // return {delta_snd_una,                      // Throughput (number of bytes acked)
    //         this->cleanslateMaxThroughput,      // Max observed throughtput (number of bytes acked)   
    //         state->snd_cwnd,                    // Current cwnd
    //         this->maxCwnd,                      // Max observed cwnd
    //         state->srtt.dbl(),                  // current srtt
    //         this->cleanslateMinDelay,           // Min SRTT observed 
    //         this->cleanslateIntervalDuration,   // Monitor interval duration
    //     };
}

RewardType CleanSlate::computeReward(){
    if (debug) cout << "\tCleanSlate: computeReward()" << endl;
    if (state->srtt.dbl() == 0) {
        return 0; // Schedule the next RLStep
    }
    // Do not compute a reward if no ACKs were received. No ACKs means no throughput, no valid RTT measurement, etc.
    // Currently this just returns a 0 reward. TODO: Find a way to skip the RLStep altogether.
    // Note to self - maybe just don't return reward/obs, and instead schedule a new event? Something the upper layers won't see.
    // if (this->rttReportCount == 0 || done || !this->first_slowstart_complete) {
    //     return RewardType(0.0);
    // }
    // // Reward calculation: Reward the agent based on their proximity to the optimal throughput/delay ratio. (power)
    //     // Delay: If the measured delay is within some forgiveness window, then it does not negatively impact reward. Forgiveness window determined by rewardDelayForgiveness.
    //     // Loss: Loss directly subtracts from the rewards gained from thoughput. Strength of effect determined by rewardLossMultiplier.
    // double optimalPower = (this->cleanslateMaxThroughput/this->cleanslateMinDelay);         // Max possible reward based on observed max/min throughput/delay so far.
    // double currentPower;                                                        // Our actual measured reward for this interval
    // if (this->cleanslateDelay <= this->cleanslateMinDelay *this->rewardDelayForgiveness) {
    //     currentPower = (this->cleanslateThroughput - this->cleanslateLossRate*this->rewardLossMultiplier) / this->cleanslateMinDelay;   // Delay forgiven
    // } else {                                                                    
    //     currentPower = (this->cleanslateThroughput - this->cleanslateLossRate*this->rewardLossMultiplier) / this->cleanslateDelay;      // Delay NOT forgiven
    // }
    // double normalizedPower = currentPower / optimalPower; // How close this reward is to optimal. (0 is worst, 1 is optimal)
    // return RewardType(normalizedPower);

    return(this->cleanslateThroughput/this->cleanslateMaxThroughput*this->cleanslateDelayMetric);
    //return(this->cleanslateThroughput/state->srtt);
}

// RayNet method: Make a decision based on the policy (alter snd_cwnd)
void CleanSlate::decisionMade(ActionType action) {
    if (debug) cout << "\tCleanSlate: decisionMade()" << endl;
    
    RLStepsTaken++;
    if (debug) cout << "\t\tRLSteps taken: " << RLStepsTaken << endl;
    if (RLStepsTaken >= this->maxRLSteps) {
            if (debug) cout << "\t\tWE ARE DONE! " << RLStepsTaken << " STEPS TAKEN!" << endl;
            done = true; // Don't set done yourself. Unsure of the correct way to handle this, but this isn't it.
    }

    if (state->srtt.dbl() == 0) {
        scheduleNextStep(this->initialStepLength); // Schedule the next RLStep
    } else {
        scheduleNextStep(this->initialStepLength);
        //scheduleNextStep(state->srtt.dbl()); // Schedule the next RLStep
    }
    // if (this->cleanslateACKTotal == 0) {
    //     if (debug) cout << "No packets ACK'd this interval. Skipping action, cwnd staying at " << state->snd_cwnd << endl;
    //     return;
    // } 

    // Avoid taking actions until initial slowstart is complete
    // if (this->first_slowstart_complete == false) {
    //     if (debug) cout << "Currently in slow start. CleanSlate will not apply any action.";
    //     return;
    // }
        double fakeAction = action;
        uint32_t newCwnd = ceil(std::pow(2.0, fakeAction) * (double) state->snd_cwnd);
        newCwnd =  max(state->snd_mss, newCwnd); // cwnd should not deflate below 1mss
        newCwnd = max(state->snd_max - state->snd_una, newCwnd); // cwnd should not deflate below in flight bytes
        // dont let cwnd inflate to ridiculous values. Learning will take care of this eventually, but large values eventually kill simulations.
        if (newCwnd < 1000000) {
            if (debug) cout << "\t\tChanging cwnd from " << state->snd_cwnd << " to " << newCwnd << "(" << (double)newCwnd/(double)state->snd_cwnd << "x)" << endl;
            if (takeActions) state->snd_cwnd = newCwnd;
        }
        

        double newIntersendingTime = state->srtt.dbl() / (double) state->snd_cwnd;  // Pace rate expressed as seconds between packets (cwnd/srtt per second)
        
        // cout << "srtt: " << state->srtt.dbl() << endl;
        // cout << "interSendTime: " << newIntersendingTime << endl;
        
        //cleanslatePaceRate = (double) state->snd_cwnd / state->srtt.dbl();  // Bytes/s
        //if (takeActions) dynamic_cast<TcpPacedConnection*>(conn)->changeIntersendingTime(1/cleanslatePaceRate); // Time between bytes

        // Change the stepSize to be 1 RTT (based on srtt)
        // cObject* newStepSizeObj = new cSimTime(state->srtt.dbl());
        // cout << "\t\tChanging step size to " << newStepSizeObj << endl;
        
        // owner->emit(this->modifyStepSizeSig, stringId.c_str(), newStepSizeObj); 
}


void CleanSlate::resetStepVariables()
{
    if (debug) cout << "\tCleanSlate: resetStepVariables()" << endl;
    if (state->srtt.dbl() == 0) {
        return; // Skipping a step, dont reset step variables
    }
    this->last_snd_max = state->snd_max;
    this->last_snd_una = state->snd_una;
    this->lastIntervalTime = simTime();
}

// Returns true if the agent is reporting this episode as complete. (Pretty sure this is never called. Just set done to true directly during an RLStep.)
bool CleanSlate::getDone() {
    if (debug) cout << "CleanSlate getDone(): If you're seeing this, getDone() probably isn't deprecated.";
    bool done = RLStepsTaken > 1000;
    if (debug) cout << "\tCleanSlate: " << RLStepsTaken << " steps completed. Returning " << done << endl;
    return done;
}

// RayNet method: Called after simulation completion? Unsure how this differs from reset()
void CleanSlate::cleanup()
{
    if (debug) cout << "\tCleanSlate: cleanUp()" << endl;
}

ObsType CleanSlate::getRLState(){
    if (debug) cout << "\tCleanSlate: getRLState()" << endl;
    // Deprecated, remove this later
}

RewardType CleanSlate::getReward(){
    if (debug) cout << "\tCleanSlate: getReward()" << endl;
    // Deprecated, remove this later
}


#endif