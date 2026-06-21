#ifndef SIGMA_TRACKER_32_H
#define SIGMA_TRACKER_32_H

#include <Arduino.h>

class SigmaTracker32 {
private:
    uint64_t count = 0;
    uint64_t sum = 0;
    uint64_t sum_sq = 0;
    uint32_t warmup_samples;

    // Cached baseline values to speed up evaluation loops
    uint32_t baseline_mean = 0;
    uint64_t baseline_variance_36 = 0; 

public:
    /**
     * @brief Construct a new Sigma Tracker object.
     * @param warmup The number of baseline samples needed before checking for outliers.
     */
    SigmaTracker32(uint32_t warmup = 500) {
        warmup_samples = warmup;
    }

    /**
     * @brief Processes a new uint32_t reading and checks if it is a 6-sigma outlier.
     * @param reading The new data sample from your stream.
     * @return true if the sample deviates by more than 6 standard deviations from the baseline.
     * @return false if the sample is within normal limits or if the tracker is still warming up.
     */
    bool process_reading(uint32_t reading) {
        if (count < warmup_samples) {
            update_baseline(reading);
            return false;
        }

        // Calculate distance from current mean
        int64_t diff = (int64_t)reading - baseline_mean;
        uint64_t diff_sq = (uint64_t)(diff * diff);

        // Pure integer 6-sigma check: (reading - mean)^2 > 36 * variance
        bool is_outlier = (diff_sq > baseline_variance_36);

        // Only update baseline statistics if it's a normal reading
        if (!is_outlier) {
            update_baseline(reading);
        }

        return is_outlier;
    }

    /**
     * @brief Get the current cached baseline mean.
     */
    uint32_t get_mean() const { 
        return baseline_mean; 
    }
    
    /**
     * @brief Get the current standard deviation.
     */
    uint32_t get_std_dev() const {
        if (count < 2) return 0;
        uint64_t variance = (sum_sq - (sum * sum) / count) / (count - 1);
        return (uint32_t)sqrt(variance);
    }

    /**
     * @brief Reset the tracker statistics completely.
     */
    void reset() {
        count = 0; 
        sum = 0; 
        sum_sq = 0;
        baseline_mean = 0; 
        baseline_variance_36 = 0;
    }

private:
    /**
     * @brief Update running accumulators and refresh cached metrics.
     */
    void update_baseline(uint32_t x) {
        count++;
        sum += x;
        sum_sq += (uint64_t)x * x;

        baseline_mean = sum / count;
        
        if (count > 1) {
            // Traditional variance formula: (SumSq - (Sum^2 / N)) / (N - 1)
            uint64_t variance = (sum_sq - (sum * sum) / count) / (count - 1);
            baseline_variance_36 = 36 * variance;
        }
    }
};

#endif // SIGMA_TRACKER_32_H
