#include <GymApi.h>
#include <omnetpp.h>
#include <unordered_map>
#include <stdlib.h>
#include <string.h>


using namespace std;
using namespace omnetpp;

int main(int argc, char **argv){
    std::string HOME(getenv("HOME"));
    std::string RAYNET_HOME = getenv("RAYNET_HOME") ? getenv("RAYNET_HOME") : HOME + "/raynet";
    std::string OMNETPP_ROOT = getenv("OMNETPP_ROOT") ? getenv("OMNETPP_ROOT") : HOME + "/omnetpp";
    std::string INET_ROOT = getenv("INET_ROOT") ? getenv("INET_ROOT") : OMNETPP_ROOT + "/samples/inet4.5";
    cout << "Home: " << HOME << endl;
    setenv("NEDPATH", (RAYNET_HOME + "/simulations;" +
                   RAYNET_HOME + "/simlibs/RLComponents/src;" +
                   RAYNET_HOME + "/simlibs/ecmp/src;" +
                   RAYNET_HOME + "/simlibs/TcpPaced/src;" +
                   RAYNET_HOME + "/simlibs/RLCC/src;" +
                   RAYNET_HOME + "/simlibs/rdp/src;" +
                   INET_ROOT + "/src/inet;" +
                   INET_ROOT + "/examples")
                   .c_str(), 1);

    // TODO: Initialise CmdRllibenv. This class will be bound to Python.
    // std::cout << NEDPATH << std::endl;
    std::string _iniPath;
    ObsType  obs;

    _iniPath = RAYNET_HOME + "/configs/orca/orca.ini";

    GymApi* gymapi = new GymApi();
   
   
   
    gymapi->initialise(_iniPath, "General");
    auto id_obs = gymapi->reset();

    std::vector<std::string> keys;
    keys.reserve(id_obs.size());

    std::vector<ObsType> vals;
    vals.reserve(id_obs.size());

    for(auto kv : id_obs) {
        keys.push_back(kv.first);
        vals.push_back(kv.second);  
    } 

    std::string agentId = keys.front();

    bool done = false;
    bool simDone = false;
    while (!done && strcmp(agentId.c_str(), "SIMULATION_END") != 0 && !simDone) {
        for(std::unordered_map<std::string,ObsType>::iterator it = id_obs.begin(); it != id_obs.end(); ++it) {
            agentId = it->first;
            }
        std::unordered_map<std::string, ActionType> actions({ {agentId, 1} });
        auto ret = gymapi->step(actions);
        done = std::get<2>(ret)["__all__"];
        obs = std::get<0>(ret)[agentId];
        simDone = std::get<3>(ret)["simDone"];
    }

    gymapi->shutdown();
    gymapi->cleanupmemory();

    // gymapi->initialise(_iniPath);
    // id_obs = gymapi->reset();

    // keys.reserve(id_obs.size());
    // vals.reserve(id_obs.size());

    // for(auto kv : id_obs) {
    //     keys.push_back(kv.first);
    //     vals.push_back(kv.second);  
    // } 

    //  agentId = keys.front();

    // done = false;
    // while (!done && strcmp(agentId.c_str(), "nostep") != 0) {
    //     for(std::unordered_map<std::string,ObsType>::iterator it = id_obs.begin(); it != id_obs.end(); ++it) {
    //         agentId = it->first;
    //         }

    //     std::cout << "Agent Id is:" << agentId << std::endl;
    //     std::cout << "Agent ID printed" << std::endl;
    //     std::unordered_map<std::string, ActionType> actions({ {agentId, 0} });
    //     auto ret = gymapi->step(actions);
    //     done = std::get<2>(ret)["__all__"];
    //     obs = std::get<0>(ret)[agentId];
    // }

    // gymapi->shutdown();
    // gymapi->cleanupmemory();
    

    return 0;
}
