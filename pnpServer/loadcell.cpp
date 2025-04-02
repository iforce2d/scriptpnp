#include <float.h>
#include "loadcell.h"
#include "log.h"
#include "probing.h"

int32_t loadcellCalibrationRawOffset = 1000;
float loadcellCalibrationWeight = 100;
volatile bool isLoadcellTriggered = false;

// largest sample distance from mean (aka noise), stored after initial baseline measure, requires machine to stay still on startup for a bit
float baselineRange = 1000;

template<typename rawType, int numReadings>
class MovingAverage {
    int readingCount;
    rawType readings[numReadings];
    rawType total;
    rawType delta; // delta between newest and oldest readings
    int index;
    int numReadingsTaken;
public:
    void reset() {
        readingCount = numReadings;
        memset(readings, 0, sizeof(readings));
        total = 0;
        index = 0;
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
    inline float getDelta() {
        return delta;
    }
    inline float getRange() { // distance of furthest sample from mean
        float avg = getAverage();
        float low = getLowest();
        float high = getHighest();
        float d0 = fabsf(avg - low);
        float d1 = fabsf(avg - high);
        return (d0 > d1) ? d0 : d1;
    }
    float getAverage() {
        int actualNumReadings = numReadingsTaken < readingCount ? numReadingsTaken : readingCount;
        if ( actualNumReadings < 1 )
            return 0;
        return total / (float)actualNumReadings;
    }
    void dump() {
        for (int i = 0; i < readingCount; i++) {
            g_log.log(LL_DEBUG, "  %d", readings[i] );
        }
        g_log.log(LL_DEBUG, "\n");
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

    isLoadcellTriggered = false;

    int fullCount = baselineMA.getReadingCount();

    // always use for baseline if just started
    if ( baselineMA.getNumReadingsTaken() < fullCount ) {
        baselineMA.addReading(loadCellRaw);
        measurementMA.reset();
        return;
    }

    if ( baselineMA.getNumReadingsTaken() == fullCount ) {
        baselineRange = baselineMA.getRange();
        g_log.log(LL_INFO, "Load cell baselineRange: %f", baselineMA.getRange());
    }

    float avg = baselineMA.getAverage();

    if ( loadCellRaw > avg ) {
        probingMA.addReading( loadCellRaw );
        if ( (probingMA.getNumReadingsTaken() > probingMA.getReadingCount()) && (probingMA.getDelta() > (50 * baselineRange)) ) {
            isLoadcellTriggered = true;
            g_log.log(LL_DEBUG, "Fast trigger %f, %f", probingMA.getDelta(), 50*baselineRange);
        }
        else if ( loadCellRaw > (avg + 100 * baselineRange)  ) {
            isLoadcellTriggered = true;
            g_log.log(LL_DEBUG, "Slow trigger");
        }
        if ( isLoadcellTriggered ) {
            probingMA.reset();
        }
    }

    // Only update baseline while not probing, or during first approach which tends to be longer and baseline slowly moves.
    // After the first approach triggers, distances will be much smaller so lock in the baseline for the rest of the probe.
    if ( probing_phase == PP_APPROACH1 || probing_phase == PP_DONE ) {
        //bool b =
        baselineMA.addReading(loadCellRaw);
        /*if ( b ) {
            // periodically output to log
            g_log.log(LL_INFO, "Adjusted load cell baseline: %f", avg);
        }*/
        measurementMA.reset();
        return;
    }

    measurementMA.addReading(loadCellRaw);
}

float getLoadCellBaseline() {
    return baselineMA.getAverage();
}

float getLoadCellMeasurement() {
    return measurementMA.getAverage();
}

float getWeight() { return baselineMA.getAverage(); // temporarily show baseline in plot
    if ( measurementMA.getNumReadingsTaken() < 1 )
        return 0;
    float bl = baselineMA.getAverage();
    return (measurementMA.getAverage() - bl) / (loadcellCalibrationRawOffset) * loadcellCalibrationWeight;
}













