#include <Arduino.h>
#include "KDHardwere.hpp"
#include "KDSharedObjects.hpp"
#include "KDDebugUtility.hpp"
#include "KDLineSensors.hpp"

void KDLineSensors::printValue()
{
    KDDebugUtility::printValueWithTag("Front", analogRead(KDHardwere::FrontLineSensorPin));
    KDDebugUtility::printValueWithTag("Rear", analogRead(KDHardwere::RearLineSensorPin));
    KDDebugUtility::printValueWithTag("Right", analogRead(KDHardwere::RightLineSensorPin));
    KDDebugUtility::printValueWithTag("Left", analogRead(KDHardwere::LeftLineSensorPin));
    /*KDDebugUtility::printValueWithTag("Front", CheckFrontLineSensor());
    KDDebugUtility::printValueWithTag("Rear", CheckRearLineSensor());
    KDDebugUtility::printValueWithTag("Right", CheckRightLineSensor());
    KDDebugUtility::printValueWithTag("Left", CheckLeftLineSensor());*/
    KDDebugUtility::println();
}
