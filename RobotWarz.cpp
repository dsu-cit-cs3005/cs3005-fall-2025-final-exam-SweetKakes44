#include "Arena.h"
#include <vector>
#include <string>

int main()
{
    Arena arena(5,8);
    arena.placeObstacles();

    // list of shared library files for robots
    std::vector<std::string> robotLibs =
    {
        "./libRobot_FireBoi.so",
        "./libRobot_Flame_e_o.so",
        "./libRobot_Ratboy.so"
    };

    // load robots from shared libraries into arena
    arena.loadRobots(robotLibs);

    // start battle
    arena.startBattle();
    return 0;
}