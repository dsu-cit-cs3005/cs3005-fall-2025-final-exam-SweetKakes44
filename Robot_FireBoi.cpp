#include "RobotBase.h"
#include "RadarObj.h"
#include <vector>
#include <algorithm> // std::any_of
#include <limits>

class Robot_FireBoi : public RobotBase
{
    private:
        int facing;
        bool targetFound;
        int targetRow;
        int targetCol;
        const int maxRange = 4; // Maximum range of the flamethrower
        std::vector<RadarObj> knownObstacles; // list of obstacles

        // Helper function to determine if a cell is an obstacle
        bool is_obstacle(int row, int col) const 
        {
            return std::any_of(knownObstacles.begin(), knownObstacles.end(), 
                            [&](const RadarObj& obj) {
                                return obj.m_row == row && obj.m_col == col;
                            });
        }
        // Helper function to add an obstacle to the list if it's not already there
        void add_obstacle(const RadarObj& obj) 
        {
            if (obj.m_type == 'M' || obj.m_type == 'P' || obj.m_type == 'F' && 
                !is_obstacle(obj.m_row, obj.m_col)) 
            {
                knownObstacles.push_back(obj);
            }
        }
        // Helper function to calculate Manhattan distance
        int calculate_distance(int row1, int col1, int row2, int col2) const 
        {
            return std::abs(row1 - row2) + std::abs(col1 - col2);
        }

    public:
        Robot_FireBoi() : RobotBase(2, 5, flamethrower)
        {
            m_name = "Robot_FireBoi";
            
            facing = 0;
            targetFound = false;
            targetRow = -1;
            targetCol = -1;
        }

        virtual void get_radar_direction(int& radar_direction_out) override 
        {
            // set scanning radar direction
            facing++;
            int currentRow, currentCol;
            get_current_location(currentRow, currentCol);

            // if in the corner, swivel
            if (currentRow == m_board_row_max && currentCol == m_board_col_max)
            {
                radar_direction_out = ((facing % 3) + 7) % 9;
                if(radar_direction_out == 0)
                {
                    radar_direction_out++;
                }
            }
            else
            {
                // swivel bottom right
                radar_direction_out = (facing % 3) + 3;
            }
            
        }

        virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override 
        {
            // process radar results and update target/obstacles
            targetFound = false; // reset to no target
            targetRow, targetCol = -1;
            int closest_distance = std::numeric_limits<int>::max();
            int currentRow, currentCol;
            get_current_location(currentRow, currentCol);

            for (const auto& obj : radar_results) 
            {
                // Add static obstacles to the obstacle list
                add_obstacle(obj);

                // Identify the first enemy found as the target
                if (obj.m_type == 'R' && targetRow == -1 && targetCol == -1) 
                {
                    int distance = calculate_distance(currentRow, currentCol, obj.m_row, obj.m_col);
                    if (distance <= maxRange && distance < closest_distance) 
                    {
                        closest_distance = distance;
                        targetRow = obj.m_row;
                        targetCol = obj.m_col;
                        targetFound = true;
                    }
                }
            }



        }
        
        virtual bool get_shot_location(int& shotRow, int& shotCol) override 
        {
            // get location for shooting
            if (targetFound)
            {
                shotRow = targetRow;
                shotCol = targetCol;
                return true;
            }
            return false;
        }

        virtual void get_move_direction(int& move_direction, int& move_distance) override 
        {
            // get movement direction and distance
            int currentRow, currentCol;
            get_current_location(currentRow, currentCol);
            int move = get_move_speed();

            // move up until hit a wall
            if (currentRow < m_board_row_max)
            {
                move_direction = 5; // Down
                move_distance = std::min(move, m_board_row_max - currentRow);
            }
            else if (currentCol < m_board_col_max)
            {
                // switch to moving right
                move_direction = 3;
                move_distance = std::min(move, m_board_row_max - currentRow);
            }
            else
            {
                move_direction = 0;
                move_distance = 0;
            }

        }
};

// Factory function to create Robot_FireBoi
extern "C" RobotBase* create_robot() 
{
    return new Robot_FireBoi();
}