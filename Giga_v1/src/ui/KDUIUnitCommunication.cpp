#include <Arduino.h>
#include "KDUIUnitCommunication.hpp"
#include "../utilities/KDDebugUtility.hpp"

void KDUIUnitCommunication::init()
{
    _serialInstance->begin(11520);
}

void KDUIUnitCommunication::reset() {}

int KDUIUnitCommunication::read()
{
    constexpr int Size = 2 + sizeof(HederCharacter);
    while (_serialInstance->available() > Size)
    {
        char header = _serialInstance->read();
        if (header == HederCharacter)
        {
            const byte centerSwitch = _serialInstance->read();
            const byte sideSwitch = _serialInstance->read();
            value = centerSwitch + (sideSwitch << 1);
            Serial.println("recieved");
        }
    }
    return value;
}

void KDUIUnitCommunication::printValue()
{
    int switchState = read();
    KDDebugUtility::printValueWithTag("switch1", switchState - (switchState >> 1 << 1));
    KDDebugUtility::printValueWithTag("switch2", switchState >> 1);
    KDDebugUtility::println();
}