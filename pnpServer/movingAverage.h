
#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

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

#endif
