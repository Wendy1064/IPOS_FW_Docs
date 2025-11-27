/*
 * safety_utils.c
 *
 *  Created on: Nov 3, 2025
 *      Author: WendyShearer
 */

#include "safety_utils.h"

extern uint8_t stableState[NUM_INPUTS];  // defined in inputs.c
extern volatile bool systemReady;

bool AllSafetiesActive(void)
{
    // During startup grace, treat as not ready/safe
    if (!systemReady)
        return false;

    uint8_t door    = stableState[INPUT_DOOR];   // 1 = door closed
    uint8_t estop   = stableState[INPUT_ESTOP];  // 1 = estop released
    uint8_t keyOK   = stableState[INPUT_KEY];    // 1 = key turned
    uint8_t bdoOK   = stableState[INPUT_BDO];    // 1 = back door OK

    return (door && estop && keyOK && bdoOK);
}

bool AnySafetyOpen(void)
{
    return !AllSafetiesActive();
}


