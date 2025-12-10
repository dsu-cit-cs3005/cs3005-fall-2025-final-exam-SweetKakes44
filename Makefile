# Compiler
.PHONY: all clean robots

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -fPIC

# Targets
all: test_robot robots RobotWarz

robotSources = Robot_FireBoi.cpp Robot_Flame_e_o.cpp Robot_Ratboy.cpp
robotLibs = libRobot_FireBoi.so libRobot_Flame_e_o.so libRobot_Ratboy.so

%.o: %.cpp RobotBase.o
	$(CXX) -g $(CXXFLAGS) -Werror -Wno-c++11-extensions -c $<

lib%.so: %.cpp RobotBase.o
	$(CXX) -shared -fPIC -o $@ $< RobotBase.o -std=c++20

robots: $(robotLibs)

test_robot: test_robot.cpp RobotBase.o Arena.o
	$(CXX) $(CXXFLAGS) test_robot.cpp RobotBase.o -ldl -o test_robot

RobotWarz: RobotWarz.o RobotBase.o Arena.o
	$(CXX) -g $(CXXFLAGS) -o $@ RobotWarz.o RobotBase.o Arena.o -ldl 

clean:
	rm -f *.o test_robot *.so RobotWarz robots
