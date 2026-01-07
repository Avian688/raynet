
import sys, os
from ray.runtime_env import RuntimeEnv

# Add ~/raynet/build to the PATH, for access to omnetbind
# sys.path.append("/home/cjuknowles/raynet/build")

#import the simulation model with cart-pole
from build.omnetbind import OmnetGymApi
import gymnasium as gym
from gymnasium import spaces, logger
import numpy as np
import math
from ray.tune.registry import register_env
import ray
from ray import tune
import random

import math
from ray.rllib.algorithms.dqn.dqn import DQNConfig

from ray.rllib.algorithms.dqn.dqn import AlgorithmConfig
# from ns3gym import ns3env
import time

class OmnetGymApiEnv(gym.Env):
    def __init__(self, env_config):
        self.env_config = env_config
        self.action_space = spaces.Box(-2,2,shape=(1,), dtype=np.float32)
        self.obs_min = np.array([0,   0,   0,   0,   0,   -3, 0,    -2],dtype=np.float32)
        self.obs_max = np.array([0.1, 0.1, 0.1, 0.1, 1000, 1, 500000,2],dtype=np.float32)
        self._agent_ids = set([str(x) for x in range(20)])

        self.observation_space = spaces.Box(low=self.obs_min, high=self.obs_max, dtype=np.float32)
        self.runner = OmnetGymApi()

    def reset(self):
       
        # if not isinstance(self.runner, type(None)):
        #     del self.runner
        

        self.runner.initialise(self.env_config["iniPath"])
        obs = self.runner.reset()

        for key, value in obs.items():
            obs[key] = np.asarray(value, dtype=np.float32)

        return obs

    def step(self, actions):
        obs, rewards, dones = self.runner.step(actions)
        if dones['__all__']:
             self.runner.shutdown()
             self.runner.cleanup()
        
        for key, value in obs.items():
            obs[key] = np.asarray(value, dtype=np.float32)

        return  obs, rewards, dones, {}

def omnetgymapienv_creator(env_config):
    return OmnetGymApiEnv(env_config)  # return an env instance

register_env("OmnetGymApiEnv", omnetgymapienv_creator)

if __name__ == '__main__':

    env = sys.argv[1]               #CartPole-v1, OmnetGymApiEnv
    num_workers = int(sys.argv[2])  # 1
    seed = int(sys.argv[3])         # 99
    random.seed(seed)
    np.random.seed(seed)

    ray.init(num_cpus=64)

    env_config = {"iniPath": os.getenv('HOME') + "/raynet/configs/james/james.ini"}
    # env_config = {"iniPath": os.getenv('HOME') + "/raynet/configs/james/james_simple.ini"}
    # env_config = {"iniPath": os.getenv('HOME') + "/omnetpp-6.2.0/samples/james_testbed/JamesDumbbell.ini"}
    
    #env_config={}
    # This should supposedly be replaced with AlgorithmConfig, but doesn't work
    algo = (
        DQNConfig()
        .env_runners(num_env_runners=num_workers)
        .resources(num_gpus=0)
        .environment(env, env_config=env_config) # "OmnetGymApiEnv
        .build_algo()
    # Deprecated DQNConfig for reference
        # DQNConfig()
        # .rollouts(num_rollout_workers=num_workers)
        # .resources(num_gpus=0)
        # .environment(env, env_config=env_config) # "ns3-v0"
        # .build()
    )
    
    # Run experiments and log progress
    t_start = time.time()
    now = time.time()
    while True:
        print(f"Total elapsed: {(now - t_start)}")
        result = algo.train()
        # for i in range(0, 20):
        #     print("Result object:")
        # for i in result:
        #     print(i, result[i])
        #     print()
        print(result['num_env_steps_sampled_lifetime'])
        if result['num_env_steps_sampled_lifetime'] >= 2000:
            break
        now = time.time()
    ray.shutdown()
    print("Finished!")