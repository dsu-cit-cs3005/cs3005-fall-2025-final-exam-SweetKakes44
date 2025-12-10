#include "Arena.h"
#include <dlfcn.h>
#include <cstdlib>
#include <ctime>
#include <math.h>
#include <thread>
#include <chrono>

// Constructor
Arena::Arena(int rows, int cols)
: rows(rows), cols(cols), grid(rows, std::vector<Cell>(cols)) 
{
    std::srand(std::time(nullptr));
}

// Load robots from shared libraries
void Arena::loadRobots(const std::vector<std::string>& robotLibs) 
{
    for (const auto& lib : robotLibs) 
    {
        void* handle;
        RobotBase* robot = loadRobot(lib, handle);
        if (robot) 
        {
            robotHandles.push_back(handle);
            robots.push_back(robot);
            
            placeRobot(robot);

            int r, c;
            robot->get_current_location(r, c);
            std::cout << "Compiling " << lib << " to lib" << robot->m_name << ".so...\n";
            std::cout << "boundaries: " << rows << ", " << cols << "\n";
            std::cout << "Loaded robot: " << robot->m_name << " at (" << r << ", " << c << ")\n";
        }
        else
        {
            std::cerr << "Failed to load robot from library: " << lib << "\n";
        }
    }
}

int Arena::get_robot_index(int row, int col) const
{
    for(size_t i = 0; i < robots.size(); i++)
    {
        int robotRow, robotCol;
        robots[i]->get_current_location(robotRow, robotCol);

        if(robotRow == row && robotCol == col)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Place obstacles in the arena
void Arena::placeObstacles() 
{
    int numObstacles = (rows * cols) / 10;
    for (int i = 0; i < numObstacles; ++i) 
    {
        int r = rand() % rows;
        int c = rand() % cols;
        if (grid[r][c].type == EMPTY) 
        {
            CellType obstacleType = static_cast<CellType>(rand() % 3 + 1);
            grid[r][c].type = obstacleType;
        }
    }
}

void Arena::announceDeath(const RobotBase* robot) const {
    std::cout << robot->m_name << " got absolutely destroyed!\n\n";
}

// Start the battle simulation
void Arena::startBattle() {
    int round = 0;
    int stagnationCounter = 0;
    const int MAX_STAGNATION_ROUNDS = 100; // Arbitrary threshold

    while (robots.size() > 1 && stagnationCounter < MAX_STAGNATION_ROUNDS && round < 10000) {
        std::cout << "\n=========== Round " << round << " ===========\n";
        printArena();

        bool progress = false;
        std::vector<std::pair<int, int>> prevLocations(robots.size());
        std::vector<int> prevDistances;

        // Track initial robot positions
        for (size_t i = 0; i < robots.size(); i++) {
            robots[i]->get_current_location(prevLocations[i].first, prevLocations[i].second);
        }

        for (size_t i = 0; i < robots.size(); i++) {
            if (robots[i]->get_health() <= 0) 
            {
                continue;
            }

            int prevHealth = robots[i]->get_health();
            int prevRow = prevLocations[i].first, prevCol = prevLocations[i].second;

            std::cout << robots[i]->m_name << "'s turn:\t";
            std::cout << robots[i]->get_health() << "/100\t";
            std::cout << "(" << prevCol << "," << prevRow <<  ")\n";

            simulateTurn(robots[i]);

            std::cout << "\n";

            int newHealth = robots[i]->get_health();
            int newRow, newCol;
            robots[i]->get_current_location(newRow, newCol);

            // Check if progress was made (damage or movement)
            if (newHealth < prevHealth || newRow != prevRow || newCol != prevCol) {
                progress = true;
            }
        }

        // Calculate proximity changes
        for (size_t i = 0; i < robots.size(); i++) {
            for (size_t j = i + 1; j < robots.size(); j++) {
                int newRow1, newCol1, newRow2, newCol2;
                robots[i]->get_current_location(newRow1, newCol1);
                robots[j]->get_current_location(newRow2, newCol2);

                int prevDist = std::abs(prevLocations[i].first - prevLocations[j].first) +
                               std::abs(prevLocations[i].second - prevLocations[j].second);
                int newDist = std::abs(newRow1 - newRow2) + std::abs(newCol1 - newCol2);

                if (newDist < prevDist) {
                    progress = true;
                }
            }
        }

        // Remove destroyed robots
        auto it = robots.begin();
        while (it != robots.end()) {
            if ((*it)->get_health() <= 0) {
                int r, c;
                (*it)->get_current_location(r, c);

                announceDeath(*it);

                grid[r][c].type = DEAD;
                int botIndex = get_robot_index(r, c);
                grid[r][c].specialChar = specialCharacters[botIndex];
                grid[r][c].robot = nullptr;
                delete *it; // Free the memory of destroyed robots
                it = robots.erase(it);
            } else {
                ++it;
            }
        }

        stagnationCounter = progress ? 0 : stagnationCounter + 1;
        ++round;
        if (robots.size() == 1) {
            std::cout << "\n=========== Game Over ===========\n";
            std::cout << "Winner: " << robots.front()->m_name << "!\n";
            return; // End the game
        }
    }

    if (robots.size() > 1) {
        std::cout << "\n=========== Game Over ===========\n";
        std::cout << "Draw due to stagnation.\n";
        return;
    }
}

// Destructor
Arena::~Arena() 
{
    for (void* handle : robotHandles) 
    {
        dlclose(handle);
    }
}

// Load a robot from a shared library
RobotBase* Arena::loadRobot(const std::string& sharedLib, void*& handle) 
{
    handle = dlopen(sharedLib.c_str(), RTLD_LAZY);
    if (!handle) 
    {
        std::cerr << "Failed to load " << sharedLib << ": " << dlerror() << '\n';
        return nullptr;
    }

    // Locate the factory function to create the robot and 'assign' it to this 'create_robot' function.
    using RobotFactory = RobotBase* (*)();
    RobotFactory create_robot = (RobotFactory)dlsym(handle, "create_robot");
    if (!create_robot) 
    {
        std::cerr << "Failed to find create_robot in " << sharedLib << ": " << dlerror() << '\n';
        dlclose(handle);
        return nullptr;
    }

    // Instantiate the robot - it will need to be deleted later. This actually calls the function that exists
    // in the ROBOT code! Cool huh! It's in the bottom of the Robot where it says extern "C"
    RobotBase* robot = create_robot();
    if (!robot) 
    {
        std::cerr << "Failed to create robot instance from " << sharedLib << '\n';
        dlclose(handle);
        return nullptr;
    }

    return robot;
}

// Place a robot in the arena
void Arena::placeRobot(RobotBase* robot) 
{
    int r, c;
    do 
    {
        r = rand() % rows;
        c = rand() % cols;
    } while (grid[r][c].type != EMPTY);

    grid[r][c].type = ROBOT;
    grid[r][c].robot = robot;
    robot->move_to(r, c);
}

// Simulate a robot's turn
void Arena::simulateTurn(RobotBase* robot) 
{
    int radarDir = 0;
    robot->get_radar_direction(radarDir);
    std::cout << "Radar Directions:" << radarDir << "\n";
    
    std::vector<RadarObj> radarResults = simulateRadar(robot, radarDir);
    robot->process_radar_results(radarResults);

    std::cout << "Radar Results for " << robot->m_name << ": ";
    for (const auto& obj : radarResults) {
        if(obj.m_type == '.')
        {
            continue;
        }
        std::cout << " Type: " << obj.m_type << " (" << obj.m_col << ", " << obj.m_row <<  ")  ";
    }
    std::cout << "\n";

    // Shooting
    int shotRow, shotCol;
    if (robot->get_shot_location(shotRow, shotCol)) 
    {
        std::cout << "Shooting: " << robot->m_name << " shoots at (" << shotCol << ", " << shotRow << ")\n";
        resolveShot(robot, shotRow, shotCol);
        return;
    }

    

    // Movement
    int moveDir = 0, moveDist = 0;
    robot->get_move_direction(moveDir, moveDist);
    int row, col;
    robot->get_current_location(row, col);
    if(grid[row][col].type == OBSTACLE_PIT)
    {
        std::cout << robot->m_name << " is trapped in a pit and cannot move!\n";
        return;
    }
    if(moveDist > 0)
    {
        moveRobot(robot, moveDir, moveDist);
        int col, row;
        robot->get_current_location(row, col);
        std::cout << robot->m_name << " moves to (" << row << ", " << col << ")\n";
    }
}

// Simulate radar results
std::vector<RadarObj> Arena::simulateRadar(RobotBase* robot, int radarDir) {
    int row, col;
    robot->get_current_location(row, col);
    std::vector<RadarObj> radarResults;

    while (true) {
        auto [newRow, newCol] = getNextCell(row, col, radarDir);

        // Check for out-of-bounds
        if (newRow < 0 || newRow >= rows || newCol < 0 || newCol >= cols) break;

        Cell& cell = grid[newRow][newCol];

        // Determine the type of object detected
        char objTypeChar = '.'; // Default to empty
        bool blocksRadar = false; // Whether this object blocks radar propagation

        switch (cell.type) {
            case EMPTY:
                objTypeChar = '.';
                blocksRadar = false;
                break;
            case ROBOT:
                objTypeChar = 'R';
                blocksRadar = true; // Assume robots block radar
                break;
            case OBSTACLE_FLAMETHROWER:
                objTypeChar = 'F';
                blocksRadar = false;
                break;
            case OBSTACLE_PIT:
                objTypeChar = 'P';
                blocksRadar = false;
                break;
            case OBSTACLE_MOUND:
            case DEAD:
                objTypeChar = (cell.type == DEAD) ? 'X' : 'M';
                blocksRadar = true;
                break;
            default:
                objTypeChar = '.'; // Unknown type
                blocksRadar = false; // Assume unknown objects block radar
                break;
        }

        // Add detected object to radar results
        RadarObj radarObj{objTypeChar, newRow, newCol};
        radarResults.push_back(radarObj);

        // Stop if the object blocks further radar propagation
        if (blocksRadar) break;

        // Move to the next cell
        row = newRow;
        col = newCol;
    }

    return radarResults;
}

// Resolve a shot
void Arena::resolveShot(RobotBase* shooter, int targetRow, int targetCol) {
    std::cout << "Resolving shot at (" << targetCol << "," << targetRow << ")\n";

    int shooterRow, shooterCol;
    int shooterWeapon = shooter->get_weapon();
    shooter->get_current_location(shooterRow, shooterCol);
    if (targetRow == shooterRow && targetCol == shooterCol) {
        return;
    }

    if (targetRow < 0 || targetRow >= rows || targetCol < 0 || targetCol >= cols) {
        return;
    }

    switch (shooterWeapon) {
        case 0: { // Flamethrower
            for (int r = targetRow - 2; r <= targetRow + 2; ++r) {
                for (int c = targetCol - 2; c <= targetCol + 2; ++c) {
                    applyDamageToCell(r, c, 30 + rand() % 21);
                }
            }
            break;
        }
        case 1: { // Railgun
            for (int c = 0; c < cols; ++c) {
                applyDamageToCell(targetRow, c, 10 + rand() % 11);
            }
            break;
        }
        case 2: { // Hammer
            if (abs(targetRow - shooterRow) <= 1 && abs(targetCol - shooterCol) <= 1) {
                applyDamageToCell(targetRow, targetCol, 50 + rand() % 11);
            } else {
                std::cerr << "Hammer can only target adjacent cells.\n";
            }
            break;
        }
        case 3: {// Grenade
            for (int r = targetRow - 1; r <= targetRow + 1; ++r) {
                for (int c = targetCol - 1; c <= targetCol + 1; ++c) {
                    applyDamageToCell(r, c, 10 + rand() % 31);
                }
            }
            break;
        }
    }
}

void Arena::applyDamageToCell(int row, int col, int baseDamage)
{
    if(row < 0 || row >= rows || col < 0 || col >= cols) return;

    Cell& targetCell = grid[row][col];
    if (targetCell.type == ROBOT && targetCell.robot) {
        std::cout << "Hit robot: " << targetCell.robot->m_name << "\n";
        int damage = baseDamage * (1 - 0.1 * std::min(targetCell.robot->get_armor(), 4));

        targetCell.robot->take_damage(damage);
        targetCell.robot->reduce_armor(1);

        if (targetCell.robot->get_health() <= 0) {
            std::cout << targetCell.robot->m_name << " is destroyed!\n";
            targetCell.type = EMPTY;
            targetCell.robot = nullptr;
        }
    } else if (targetCell.type != EMPTY) {
        std::cout << "Shot hit an obstacle: ";
        if (targetCell.type == OBSTACLE_FLAMETHROWER) std::cout << "Flamethrower\n";
        else if (targetCell.type == OBSTACLE_PIT) std::cout << "Pit\n";
        else if (targetCell.type == OBSTACLE_MOUND) std::cout << "Mound\n";
    }
}

void Arena::moveRobot(RobotBase* robot, int direction, int distance) {
    int row, col;
    robot->get_current_location(row, col);

    for (int i = 0; i < distance; ++i) {
        auto [newRow, newCol] = getNextCell(row, col, direction);

        if (newRow < 0 || newRow >= rows || newCol < 0 || newCol >= cols) {
            std::cerr << robot->m_name << " attempted to move out of bounds.\n";
            break;
        }

        Cell& nextCell = grid[newRow][newCol];
        if (nextCell.type == OBSTACLE_PIT) {
            std::cerr << robot->m_name << " fell into a pit and is stuck!\n";
            return; // Robot cannot move further
        } else if (nextCell.type == OBSTACLE_FLAMETHROWER) {
            std::cerr << robot->m_name << " took flamethrower damage!\n";
            robot->take_damage(30 + rand() % 21); // Flamethrower damage
        } else if (nextCell.type == OBSTACLE_MOUND) {
            std::cerr << robot->m_name << " hit a mound and cannot move there!\n";
            break;
        } else if (nextCell.type == DEAD) {
            std::cerr << robot->m_name << " hit a dead robot and cannot move there!\n";
            break;
        } else if (nextCell.type == ROBOT) {
            std::cerr << robot->m_name << " collided with another robot.\n";
            break;
        }

        grid[row][col].type = EMPTY;
        grid[row][col].robot = nullptr;

        row = newRow;
        col = newCol;

        grid[row][col].type = ROBOT;
        grid[row][col].robot = robot;
    }

    robot->move_to(row, col);
}

void Arena::printArena() const {

    std::cout << "Legend:\n";
    std::cout << ".: Empty  ";
    std::cout << "F: Flamethrower  ";
    std::cout << "P: Pit  ";
    std::cout << "M: Mound  ";
    std::cout << "R: Robot  ";
    std::cout << "X: Destroyed Robot\n\n";

    // Print column headers
    std::cout << "    "; // Padding for row headers
    for (int c = 0; c < cols; ++c) {
        std::cout << c << (c < 10 ? "  " : " "); // Align single- and double-digit numbers
    }

    std::cout << "\n   +" << std::string(cols * 3 + 1, '-') << "+\n";

    // Print rows
    for (int r = 0; r < rows; ++r) {
        // Print row header
        std::cout << (r < 10 ? " " : "") << r << " | "; // Align single- and double-digit row numbers

        // Print row content
        for (int c = 0; c < cols; ++c) {
            switch (grid[r][c].type) {
                case EMPTY: std::cout << ".  "; break;
                case OBSTACLE_FLAMETHROWER: std::cout << "F  "; break;
                case OBSTACLE_PIT: std::cout << "P  "; break;
                case OBSTACLE_MOUND: std::cout << "M  "; break;
                case ROBOT:
                    if (grid[r][c].robot) {
                        int botIndex = get_robot_index(r, c);
                        std::cout << "R" << specialCharacters[botIndex] << " ";
                    } else {
                        std::cout << ".  ";
                    }
                    break;
                case DEAD: std::cout << "X" << grid[r][c].specialChar << " "; break;
                default: std::cout << ".  "; break;
            }
        }
        std::cout << "|\n"; // Double space for row separation
    }
    std::cout << "   +" << std::string(cols * 3 + 1, '-') << "+\n\n";
}

std::pair<int, int> Arena::getNextCell(int row, int col, int radarDir) {
    // Directions corresponding to radarDir values 1 to 8
    static const std::pair<int, int> directions[] = {
        {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}
    };

    // Validate radarDir
    if (radarDir < 1 || radarDir > 8) {
        return {-1, -1}; // Invalid direction
    }

    // Get next cell coordinates
    int index = radarDir - 1;
    int newRow = row + directions[index].first;
    int newCol = col + directions[index].second;

    // Check bounds
    if (newRow < 0 || newRow >= rows || newCol < 0 || newCol >= cols) {
        newRow = (newRow + rows) % rows;
        newCol = (newCol + cols) % cols;
    }

    // Return the valid next cell
    return {newRow, newCol};
}
