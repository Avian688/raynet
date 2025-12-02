package JamesLib;

import RLComponents.Broker;
import RLComponents.Stepper;
import JamesLib.JamesAgent;

network JamesNet
{
     submodules:
        broker: Broker;
        james_agent: JamesAgent;
        stepper: Stepper;
}