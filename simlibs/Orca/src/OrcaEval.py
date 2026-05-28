import sys, os
sys.path.insert(0, os.environ.get("RAYNET_HOME", os.path.join(os.getenv("HOME"), "raynet")))
from raynet_numpy_compat import install_numpy_core_aliases, install_rllib_checkpoint_compat
install_numpy_core_aliases()
install_rllib_checkpoint_compat()
from raynet_paths import materialize_raynet_ini
from ray.runtime_env import RuntimeEnv
import gymnasium as gym
from gymnasium import spaces
import numpy as np
import math
from ray.tune.registry import register_env
from ray.rllib.callbacks.callbacks import RLlibCallback
import pprint
import ray
from ray import tune
from ray.tune import Tuner
from ray.air import CheckpointConfig
import random
import math
from ray.rllib.algorithms.ppo.ppo import PPOConfig
from ray.rllib.algorithms.sac.sac import SACConfig
from ray.rllib.algorithms.sac.sac import SAC
import os
import time
from random import randint
from collections import defaultdict, deque
import torch

class OmnetGymApiEnv(gym.Env):
    def __init__(self,env_config):
        """
        Initialize the training environment configuration
        - This mostly involves setting spcaes (bounds, shapes, types) for actions and observations.
        - These bounds are needed for RL algorithms provided by RLlib- They limit the problem space and are also used for normalization.
        """
        sys.path.insert(0, os.path.join(os.getenv('HOME'), "raynet", "build"))
        from omnetbind import OmnetGymApi
        self.runner = OmnetGymApi()
        
        self.env_config = env_config
        self.step_count = 0 # just for debugging
        self.random_seed = os.getpid() # Ensures each ray worker generates different parameters
        random.seed(self.random_seed)
        # Initialize env parameters to some reasonable defaults (these should be quickly overwritten in reset())
        self.stacking = self.env_config["stacking"]

        self.has_reset = False

        # Define the action space (possible values for actions)
        self.action_space = spaces.Box(low=-2, high=2, shape=(1,), dtype=np.float32) # Orca: A float value from -2.0 to 2.0. Will be used to alter cwnd via (cwnd = 2^action * cwnd).

        # Define the observation space (expected values/types for each observation feature)
        self.obs_min = np.tile(np.array(
                     [0,                            # Throughput
                      0,                            # Pacerate
                      0,                            # Lossrate
                      0,                            # number of acks
                      0,                            # Interval duration
                      0,                            # srtt
                      0                             # Delay metric
                      ], dtype=np.float32), self.stacking)
        self.obs_max = np.tile(np.array(
                     [1,                            # Throughput
                      10,                           # Pacerate
                      10,                           # Lossrate
                      10,                           # Number of ACKs
                      1,                            # Interval duration
                      1,                            # srtt
                      1,                            # Delay metric
                      ], dtype=np.float32), self.stacking)
        self.observation_space = spaces.Box(
            low=self.obs_min, 
            high=self.obs_max, 
            dtype=np.float32) # A 4-dimensional array, each feature is a float value with its own bounds
        
        self.num_observations = 7
        self.obs_history = defaultdict(self._new_obs_history)
        self.last_obs_by_agent = {}
        
    def _new_obs_history(self):
        return deque(
            np.zeros(self.stacking * self.num_observations, dtype=np.float32),
            maxlen=self.stacking * self.num_observations,
        )

    def _stack_agent_obs(self, agent_id, agent_obs, reset=False):
        if reset:
            self.obs_history[agent_id] = self._new_obs_history()

        agent_obs = np.asarray(agent_obs, dtype=np.float32)
        if reset:
            for _ in range(self.stacking):
                self.obs_history[agent_id].extend(agent_obs)
        self.obs_history[agent_id].extend(agent_obs)
        return np.asarray(list(self.obs_history[agent_id]), dtype=np.float32)

    def _stack_obs_map(self, obs_map, reset=False):
        stacked = {}
        for agent_id, agent_obs in obs_map.items():
            if agent_id == "SIMULATION_END":
                continue
            stacked[agent_id] = self._stack_agent_obs(agent_id, agent_obs, reset=reset)
        self.last_obs_by_agent = stacked
        return stacked

    def _coerce_action_map(self, actions):
        if isinstance(actions, dict):
            return {
                agent_id: float(np.asarray(action, dtype=np.float32).reshape(-1)[0])
                for agent_id, action in actions.items()
            }

        if len(self.last_obs_by_agent) != 1:
            raise ValueError(
                f"Expected an action dict for agents {sorted(self.last_obs_by_agent)}, got a single action."
            )

        agent_id = next(iter(self.last_obs_by_agent))
        return {agent_id: float(np.asarray(actions, dtype=np.float32).reshape(-1)[0])}

       
    def reset(self, *, seed=None, options=None):
        self.obs_history = defaultdict(self._new_obs_history)
        self.last_obs_by_agent = {}
        
        self.runner.initialise(self.env_config["iniPath"], self.env_config["config_section"])
        
        
        obs = self.runner.reset()
        return self._stack_obs_map(obs, reset=True), {}

    def step(self, actions):
        action = self._coerce_action_map(actions)
        obs, rewards, terminateds, info_ = self.runner.step(action)
        if info_['simDone']:
            self.runner.cleanup()
            truncateds = {agent_id: True for agent_id in self.last_obs_by_agent}
            truncateds["__all__"] = True
            terminateds = dict(terminateds)
            terminateds["__all__"] = bool(terminateds.get("__all__", False))
            return {}, rewards, terminateds, truncateds, {}

        return_obs_history = self._stack_obs_map(obs)
        raw_terminateds = terminateds
        rewards = {agent_id: float(rewards.get(agent_id, 0.0)) for agent_id in return_obs_history}
        terminateds = {
            agent_id: bool(raw_terminateds.get(agent_id, False))
            for agent_id in return_obs_history
        }
        terminateds["__all__"] = bool(raw_terminateds.get("__all__", False))
        truncateds = {agent_id: False for agent_id in return_obs_history}
        truncateds["__all__"] = False
        
        if terminateds["__all__"]:
            print(terminateds)
            self.runner.shutdown()
            self.runner.cleanup()
        
        # Debug
        printFreq = 1
        if self.step_count % printFreq == -1:
            print("-")
            print(f"{printFreq} step(s) completed (Agent total: {self.step_count}):")
            print("\tObservations:")
            debug_agent, debug_obs = next(iter(return_obs_history.items()))
            print(f"\t\tAgent: {debug_agent}")
            print(f"\t\tThroughput: {debug_obs[0]:.2f}%             \t\t(Normalized, per interval)")
            print(f"\t\tPacing Rate: {debug_obs[1]:.2f}%        \t\t(Normalized, per interval)")
            print(f"\t\tLoss Rate: {debug_obs[2]:.2f}%          \t\t(Normalized, per interval)")
            print(f"\t\tACKs: {debug_obs[3]:.2f}x              \t\t(Multiplier of cwnd, per interval)") #? Identical to goodput(throughput) if normalized. 
            print(f"\t\tInterval time: {debug_obs[4]:.2f}s      \t\t(Raw, per interval)") #? Identical to delay if normalized?
            print(f"\t\tSRTT: {debug_obs[5]:.2f}%                   \t\t(Normalized, current)") #? Basically same as delay? slightly longer time horizon
            print(f"\t\tDelay: {debug_obs[6]:.2f}%                    \t\t(Log, current)") #? Maybe normalize?
            
            print(f"\tRewards:")
            print(f"\t\tREWARD: {rewards.get(debug_agent, 0.0):.5f}                  \t(Raw, per interval)")
        self.step_count += 1
        
        # OBS, REWARD, IS_TERMINATED, IS_TRUNCATED, EXTRA_INFO
        return return_obs_history, rewards, terminateds, truncateds, {}
        
# Generates the OmnetGymApiEnv for the calling ray worker
def omnetgymapienv_creator(env_config):
    return OmnetGymApiEnv(env_config)  # return an env instance

register_env("OmnetGymApiEnv", omnetgymapienv_creator)

if __name__ == '__main__':
    env_name = "Orca-inference"
    register_env(env_name, omnetgymapienv_creator)
    
    load_from_checkpoint = True
    checkpoint_load_dir = os.getenv('HOME') + "/raynet/_models/Orca"
    env_config = {"iniPath": materialize_raynet_ini(sys.argv[1]),
                  "config_section": sys.argv[2] if len(sys.argv) > 2 else "Orca", # Optional argument to specifcy which config.ini section to run. Orca by default.
                  "stacking": 10}
    
    ray.init(
        local_mode=True,
        include_dashboard=False,
        ignore_reinit_error=True,
        _temp_dir=f"/tmp/ray_{os.getpid()}",
        num_cpus=1
    )
    config = (
            SACConfig()
            # .resources(num_gpus=len(gpus), num_gpus_per_learner_worker=1)
            .env_runners(explore=False) #, rollout_fragment_length=1000
            .environment(env_name, env_config=env_config, disable_env_checking=True) # "OmnetGymApiEnv
            )
    algo = config.build_algo()
    
    # Convert betas? (solution found online, fixes a crash when loading a checkpoint)
    def betas_tensor_to_float(learner):
        for param_grp_key in learner._optimizer_parameters.keys():
            param_grp = param_grp_key.param_groups[0]
            param_grp["betas"] = tuple(beta.item() for beta in param_grp["betas"])
    if (load_from_checkpoint):
        algo.restore(checkpoint_load_dir)
        #algo.learner_group.foreach_learner(betas_tensor_to_float)
    
    # Inference Loop! Only tested for cleanSlate but MUCH faster that .train()
    steps = 0
    check_in_freq = 100
    env = OmnetGymApiEnv(env_config)
    obs_by_agent, _ = env.reset()
    module = algo.get_module("default_module")
    while True:
        actions = {}
        for agent_id, obs in obs_by_agent.items():
            obs_batch = torch.from_numpy(np.asarray(obs, dtype=np.float32)).unsqueeze(0)
            with torch.no_grad():
                out = module.forward_inference({"obs": obs_batch})

            actions[agent_id] = module.get_inference_action_dist_cls().from_logits(
                out["action_dist_inputs"]
            ).sample()[0].cpu().numpy()

        obs_by_agent, rewards, terminateds, truncateds, _ = env.step(actions)

        if steps % check_in_freq == 0:
            print(f"Step {steps}, rewards={rewards}")
        steps += 1
        if terminateds.get("__all__", False) or truncateds.get("__all__", False) or not obs_by_agent:
            break
