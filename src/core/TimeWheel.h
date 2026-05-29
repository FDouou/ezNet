#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace ezNet {

class TimeWheel {
public:
    struct Entry {
        int bucketIndex = -1;
        double timeoutSec = 0.0;
        std::function<void()> callback;
        std::weak_ptr<void> owner;
        Entry* prev = nullptr;
        Entry* next = nullptr;
    };

    TimeWheel(int bucketCount, double tickIntervalSec);
    ~TimeWheel();

    TimeWheel(const TimeWheel&) = delete;
    TimeWheel& operator=(const TimeWheel&) = delete;

    Entry* add(double timeoutSec, std::function<void()> callback, std::weak_ptr<void> owner);
    void remove(Entry* entry);
    void refresh(Entry* entry);
    void tick();

    double tickInterval() const { return tickIntervalSec_; }
    int bucketCount() const { return bucketCount_; }

private:
    void detach(Entry* entry);
    void attach(int bucket, Entry* entry);

    std::vector<Entry*> buckets_;
    int bucketCount_;
    double tickIntervalSec_;
    int currentBucket_ = 0;
};

} // namespace ezNet
