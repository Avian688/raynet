import itertools
import os

if __name__ == "__main__":
    
    SEEDS =  [1,10,100]
    ENVS = ["OmnetGymApiEnv"]
    WORKERS = [1]
    
    
    for params in itertools.product(ENVS, WORKERS, SEEDS):
        if params[0] != 'ns3-v0':
            os.chdir(f"{os.getenv('HOME')}/raynet/scripts/cartpole_old")
            os.system(f"python3 cartpoleexp.py {params[0]} {params[1]} {params[2]}")
        else:
            os.chdir(f"{os.getenv('HOME')}/ns-allinone-3.38/ns-3.38/contrib/opengym/examples/cartpole")
            os.system(f"python3 cartpoleexp.py {params[0]} {params[1]} {params[2]}")


    
