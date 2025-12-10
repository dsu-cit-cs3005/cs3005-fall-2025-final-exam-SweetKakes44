#ifndef ARENA_H
#define ARENA_H

#include <iostream>
#include <vector>
#include <string>
#include "RobotBase.h"

// Cell types
enum CellType { EMPTY, OBSTACLE_FLAMETHROWER, OBSTACLE_PIT, OBSTACLE_MOUND, ROBOT, DEAD };

struct Cell 
{
    CellType type = EMPTY;
    std::string specialChar;
    RobotBase* robot = nullptr; // Pointer to a robot if the cell contains one
};

class Arena 
{
public:
    Arena(int rows, int cols);

    void loadRobots(const std::vector<std::string>& robotLibs);
    void placeObstacles();
    void startBattle();

    ~Arena();

private:
    int rows, cols;
    std::vector<std::vector<Cell>> grid;
    std::vector<RobotBase*> robots;
    std::vector<void*> robotHandles;

    std::vector<char> specialCharacters = { '^', '*', '#', '>', '&', '@', '%', '!', '+'};
    int get_robot_index(int row, int col) const;

    RobotBase* loadRobot(const std::string& sharedLib, void*& handle);
    void placeRobot(RobotBase* robot);
    void resolveShot(RobotBase* shooter, int targetRow, int targetCol);
    void moveRobot(RobotBase* robot, int direction, int distance);
    
    std::vector<RadarObj> simulateRadar(RobotBase* robot, int radarDir);
    std::pair<int, int> getNextCell(int row, int col, int radarDir);
    void applyDamageToCell(int row, int col, int baseDamage);

    void printArena() const;
    void printHealthBar(RobotBase* robot) const;
    void announceDeath(const RobotBase* robot) const;
    void simulateTurn(RobotBase* robot);
};

#endif // ARENA_H
