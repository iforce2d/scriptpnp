
#include <float.h>
#include "loadcell.h"
#include "log.h"

int32_t loadcellCalibrationRawOffset = 1000;
float loadcellCalibrationWeight = 100;
volatile bool loadcellNonZero = false;

template<typename rawType, int numReadings>
class MovingAverage {
    rawType readings[numReadings];
    rawType total;
    rawType lowest;
    rawType highest;
    int index;
    uint32_t numReadingsTaken;
public:
    void reset() {
        memset(readings, 0, sizeof(readings));
        lowest = INT32_MAX;
        highest = INT32_MIN;
        total = 0;
        index = 0;
        numReadingsTaken = 0;
    }
    void addReading(rawType r) {
        if ( r < lowest )
            lowest = r;
        if ( r > highest )
            highest = r;
        total -= readings[index];
        total += r;
        readings[index] = r;
        index = (index + 1) % numReadings;
        numReadingsTaken++;
    }
    inline float getLowest() {
        int32_t best = INT32_MAX;
        for (int i = 0; i < numReadings; i++) {
            if ( readings[i] < best )
                best = readings[i];
        }
        return best;
    }
    inline float getHighest() {
        int32_t best = INT32_MIN;
        for (int i = 0; i < numReadings; i++) {
            if ( readings[i] > best )
                best = readings[i];
        }
        return best;
    }
    inline uint32_t getNumReadingsTaken() {
        return numReadingsTaken;
    }
    float getAverage() {
        int actualNumReadings = numReadingsTaken < numReadings ? numReadingsTaken : numReadings;
        if ( actualNumReadings < 1 )
            return 0;
        return total / (float)actualNumReadings;
    }
};

MovingAverage<int32_t, 200> baselineMA;
MovingAverage<int32_t,  10> measurementMA;

void resetLoadcell() {
    baselineMA.reset();
    measurementMA.reset();
}

void updateLoadcell(int32_t loadCellRaw) {

    // always use for baseline if just started
    if ( baselineMA.getNumReadingsTaken() < 200 ) {
        baselineMA.addReading(loadCellRaw);
        measurementMA.reset();
        loadcellNonZero = false;
        return;
    }

    if ( baselineMA.getNumReadingsTaken() == 200 ) {
        g_log.log(LL_INFO, "Load cell baseline: %f", baselineMA.getAverage());
    }

    // after baseline is established, only accept similar values
    float low = baselineMA.getLowest();
    float high = baselineMA.getHighest();
    float range = high - low;
    low -= range;
    high += range;
    if ( loadCellRaw > low && loadCellRaw < high ) {
        baselineMA.addReading(loadCellRaw);
        measurementMA.reset();
        loadcellNonZero = false;
        return;
    }

    loadcellNonZero = true;
    measurementMA.addReading(loadCellRaw);
}

float getLoadCellBaseline() {
    return baselineMA.getAverage();
}

float getLoadCellMeasurement() {
    return measurementMA.getAverage();
}

float getWeight() {
    if ( measurementMA.getNumReadingsTaken() < 1 )
        return 0;
    float bl = baselineMA.getAverage();
    return (measurementMA.getAverage() - bl) / (loadcellCalibrationRawOffset) * loadcellCalibrationWeight;
}













