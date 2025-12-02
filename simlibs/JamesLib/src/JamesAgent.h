#ifndef __JAMES_AGENT_H_
#define __JAMES_AGENT_H_

#include <omnetpp.h>
#include <iostream>
#include <string>
#include <math.h>
#include <array>
#include <random>
#include <tuple>
#include "BrokerData.h"
#include "RLInterface.h"

using namespace omnetpp;
using namespace std;
using namespace learning;

/**
 * TODO - Generated class
 */
class JamesAgent : public cSimpleModule, public RLInterface
{
public:
    double a;
    double b;
    double gravity;
    double masscart;
    double masspole;
    double total_mass;
    double length; // actually half the pole's length
    double polemass_length;
    double force_mag;
    double tau; // seconds between state updates
    string kinematics_integrator;

    double theta_threshold_radians;
    double x_threshold;

    double high[4];

    ActionType action[2] = {0, 1};

    int steps_beyond_done;
    int steps;
    ObsType state; // array declared

    cMessage* initMsg; // Msg used to notify end of step

    bool isRegistered;


protected:
  virtual void initialize();

public:
  ~JamesAgent();
  ObsType random();
  void step(ActionType action);
    virtual void handleMessage(cMessage *msg);
    virtual void finish();

    virtual void cleanup();
    virtual void decisionMade(ActionType action); // defines what to do when decision is made
    virtual ObsType  getRLState();
    virtual RewardType getReward();
    virtual bool getDone();
    virtual void resetStepVariables();
    virtual ObsType computeObservation();
    virtual RewardType computeReward();
  
};
#endif