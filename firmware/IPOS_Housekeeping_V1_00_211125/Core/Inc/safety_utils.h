/*
 * safety_utils.h
 *
 *  Created on: Nov 3, 2025
 *      Author: WendyShearer
 */

#ifndef SAFETY_UTILS_H
#define SAFETY_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include "inputs.h"

// Returns true if all safety interlocks are active (safe state)
bool AllSafetiesActive(void);

// Returns true if any interlock is open (unsafe state)
bool AnySafetyOpen(void);

#endif
