#ifndef __JAMES_CC_H_
#define __JAMES_CC_H_

#include <omnetpp.h>
#include <iostream>
#include <string>
#include <math.h>
#include <array>
#include <random>
#include <tuple>
#include "BrokerData.h"
#include "RLInterface.h"

#include "MonitorInterval.h"
#include "inet/transportlayer/tcp/flavours/TcpNewReno.h"
#include <inet/transportlayer/tcp/Tcp.h>
#include <inet/transportlayer/tcp/TcpConnection.h>

using namespace omnetpp;
using namespace inet::tcp;
using namespace inet;
using namespace learning;
/**
 * TODO - Generated class
 */
 // Used to import cSimpleModule, now only imports TcpReno
class JamesCC : public TcpNewReno
{
  };
#endif




//
// Copyright (C) 2009 Thomas Reschka
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TCPNEWRENO_H
#define __INET_TCPNEWRENO_H

#include "inet/transportlayer/tcp/flavours/TcpTahoeRenoFamily.h"

namespace inet {
namespace tcp {

/**
 * State variables for TcpNewReno.
 */
typedef TcpTahoeRenoFamilyStateVariables TcpNewRenoStateVariables;

/**
 * Implements TCP NewReno.
 */
class INET_API TcpNewReno : public TcpTahoeRenoFamily
{
  protected:
    TcpNewRenoStateVariables *& state; // alias to TcpAlgorithm's 'state'

    /** Create and return a TcpNewRenoStateVariables object. */
    virtual TcpStateVariables *createStateVariables() override
    {
        return new TcpNewRenoStateVariables();
    }

    /** Utility function to recalculate ssthresh */
    virtual void recalculateSlowStartThreshold();

    /** Redefine what should happen on retransmission */
    virtual void processRexmitTimer(TcpEventCode& event) override;

  public:
    /** Ctor */
    TcpNewReno();

    /** Redefine what should happen when data got acked, to add congestion window management */
    virtual void receivedDataAck(uint32_t firstSeqAcked) override;

    /** Redefine what should happen when dupAck was received, to add congestion window management */
    virtual void receivedDuplicateAck() override;
};

} // namespace tcp
} // namespace inet

#endif

