#pragma once

#include <Arduino.h>
#include <etl/singleton.h>

class MotorController
{
    public:
        MotorController() = default; // for now


        bool begin(); 

        // some kind of function 

    private:

};
using MotorControllerInstance = etl::singleton<MotorController>;