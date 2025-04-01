
#include <float.h>
#include "loadcell.h"
#include "log.h"

int32_t loadcellCalibrationRawOffset = 1000;
float loadcellCalibrationWeight = 100;
volatile bool loadcellNonZero = false;

template<typename rawType, int numReadings>
class MovingAverage {
    int readingCount;
    rawType readings[numReadings];
    rawType total;
    rawType delta; // delta between newest and oldest readings
    int index;
    int numReadingsTaken;
    float range; // largest sample distance from mean, stored after initial baseline measure
public:
    void reset() {
        readingCount = numReadings;
        memset(readings, 0, sizeof(readings));
        total = 0;
        index = 0;
        range = 0;
        delta = 0;
        numReadingsTaken = 0;
    }
    bool addReading(rawType r) {
        rawType old = readings[index];
        delta = r - old;
        total -= old;
        total += r;
        readings[index] = r;
        index = (index + 1) % readingCount;
        numReadingsTaken++;
        return ( (numReadingsTaken % 1000) == 0 ); // return true every 5 seconds
    }
    inline float getLowest() {
        int32_t best = INT32_MAX;
        for (int i = 0; i < readingCount; i++) {
            if ( readings[i] < best )
                best = readings[i];
        }
        return best;
    }
    inline float getHighest() {
        int32_t best = INT32_MIN;
        for (int i = 0; i < readingCount; i++) {
            if ( readings[i] > best )
                best = readings[i];
        }
        return best;
    }
    inline int getReadingCount() {
        return readingCount;
    }
    inline int getNumReadingsTaken() {
        return numReadingsTaken;
    }
    inline float getRange() {
        return range;
    }
    inline float getDelta() {
        return delta;
    }
    inline void storeRange() {
        float avg = getAverage();
        float low = getLowest();
        float high = getHighest();
        float d0 = fabsf(avg - low);
        float d1 = fabsf(avg - high);
        range = (d0 > d1) ? d0 : d1;
    }
    float getAverage() {
        int actualNumReadings = numReadingsTaken < readingCount ? numReadingsTaken : readingCount;
        if ( actualNumReadings < 1 )
            return 0;
        return total / (float)actualNumReadings;
    }
};

MovingAverage<int32_t, 200> baselineMA;    // 1 sec
MovingAverage<int32_t,   2> measurementMA; // 0.01 sec
MovingAverage<int32_t,   4> probingMA;     // 0.02 sec

void resetLoadcell() {
    baselineMA.reset();
    measurementMA.reset();
    probingMA.reset();
}

void updateLoadcell(int32_t loadCellRaw) {

    loadcellNonZero = false;

    int fullCount = baselineMA.getReadingCount();

    // always use for baseline if just started
    if ( baselineMA.getNumReadingsTaken() < fullCount ) {
        //g_log.log(LL_INFO, "loadCellRaw %d", loadCellRaw);
        baselineMA.addReading(loadCellRaw);
        measurementMA.reset();
        //loadcellNonZero = false;
        return;
    }

    if ( baselineMA.getNumReadingsTaken() == fullCount ) {
        baselineMA.storeRange();
        g_log.log(LL_INFO, "Load cell baseline: %f, range = %f", baselineMA.getAverage(), baselineMA.getRange());
    }

    float avg = baselineMA.getAverage();
    float range = baselineMA.getRange();
    float low  = avg - range;
    float high = avg + range;

    if ( loadCellRaw > low ) {
        probingMA.addReading( loadCellRaw );
        if ( (probingMA.getNumReadingsTaken() == probingMA.getReadingCount()) && probingMA.getDelta() > (50 * range) ) {
            loadcellNonZero = true;
            g_log.log(LL_DEBUG, "Fast trigger");
        }
        else if ( loadCellRaw > (avg + 100 * range)  ) {
            loadcellNonZero = true;
            g_log.log(LL_DEBUG, "Slow trigger");
        }
        if ( loadcellNonZero ) {
            probingMA.reset();
        }
    }

    // after baseline is established, only allow similar values to update baseline
    if ( loadCellRaw > low && loadCellRaw < high ) {
        bool b = baselineMA.addReading(loadCellRaw);
        if ( b ) {
            // periodically adjust range and output to log
            baselineMA.storeRange();
            g_log.log(LL_INFO, "Adjusted load cell baseline: %f, range = %f", avg, baselineMA.getRange());
        }
        measurementMA.reset();
        //loadcellNonZero = false;
        return;
    }

    //loadcellNonZero = true;
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













