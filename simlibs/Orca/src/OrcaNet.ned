package Orca;

@namespace(inet);

import inet.common.misc.ThruputMeteringChannel;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import RLComponents.Broker;
import RLComponents.Stepper;
import Orca.OrcaNet;
import Orca.Orca;
import Orca.OrcaTcp;
        
import RLComponents.Broker;
import RLComponents.Stepper;

network OrcaNet
{
    parameters:
        // @statistic[packetDropped] (source = packetDropped; record = count);
        int numberOfFlows = default(1);
        
    types:
        channel bottleneckLine extends ThruputMeteringChannel
        {
            delay = 10 ms; 
            datarate = 96 Mbps; 
        }
        
        channel standardLine extends ThruputMeteringChannel
        {
            delay = 0ms;
            datarate = 1000Gbps;
        }
    submodules:
    
        configurator: Ipv4NetworkConfigurator;
        
        client[numberOfFlows]: StandardHost;
        server[numberOfFlows]: StandardHost;
        router1: Router;
        router2: Router;
        broker: Broker;
		stepper: Stepper;
    connections:
        // Left
        for i=0..sizeof(client)-1 {
            client[i].pppg++ <--> standardLine <--> router1.pppg++;
        }

        // Right
        for n=0..sizeof(server)-1 {
            router2.pppg++ <--> standardLine <--> server[n].pppg++;
        }
        
        // Bottleneck
        router1.pppg++ <--> bottleneckLine <--> router2.pppg++;
}