#include <Arduino.h>
#include "KDCatchSensor.hpp"
#include "../utilities/KDDebugUtility.hpp"

void KDCatchSensor::printValue()
{
    int catchSensprValue = read();
    KDDebugUtility::printValueWithTag("Catch", catchSensprValue);
    KDDebugUtility::println();
}
