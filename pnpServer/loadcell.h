#ifndef LOADCELL_H
#define LOADCELL_H

#include <stdint.h>
#include "../common/pnpMessages.h"

extern volatile bool loadcellNonZero;

void resetLoadcell();
void updateLoadcell(int32_t loadCellRaw);
float getLoadCellBaseline();
float getLoadCellMeasurement();
float getWeight();

#endif // LOADCELL_H
