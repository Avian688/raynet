package CleanSlate;
import inet.transportlayer.tcp.Tcp;

simple CleanSlateTcp extends Tcp
{
    parameters:
        @class(CleanSlateTcp);

        int maxWindow; 					                    // maximum window size
        int stateSize; 					                    // dimension of the state space
        int maxObsCount; 				                    // max number of observations for a single state
        double ewmaWeight; 				                    // weight for the moving average of state variables (as in Tcp-Ex Machina)
        double rewardDelayForgiveness; 		                    // Reward only suffers if (delay > minDelay * rewardDelayForgiveness). (higher values = more forgiveness, less impact of delay increases on reward)
        double rewardLossMultiplier;                        // How strongly loss impacts reward (higher = stronger impact)

        bool printDebugMessages;                            // used to print debug messages
        bool takeActions;                                   // Skips actions if set to false. Useful for comparing against the base case without altering code.
        int maxRLSteps;                                     // how many training steps should take place before the agent reports as done
        double monitorIntervalDuration = default(1.0);      // How many seconds a monitor interval should last
        @signal[stepData];				                    // step data signal
        @signal[actionQuery];			                    // action query signal
}