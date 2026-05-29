#include "core/TimeWheel.h"
#include <cmath>
#include <cstdint>

namespace ezNet {

TimeWheel::TimeWheel(int bucketCount, double tickIntervalSec)
    : buckets_(bucketCount, nullptr)
    , bucketCount_(bucketCount)
    , tickIntervalSec_(tickIntervalSec) {
}

TimeWheel::~TimeWheel() {
    for (int i = 0; i < bucketCount_; ++i) {
        Entry* entry = buckets_[i];
        while (entry) {
            Entry* next = entry->next;
            delete entry;
            entry = next;
        }
    }
}

TimeWheel::Entry* TimeWheel::add(double timeoutSec, std::function<void()> callback,
                                  std::weak_ptr<void> owner) {
    auto* entry = new Entry();
    entry->timeoutSec = timeoutSec;
    entry->callback = std::move(callback);
    entry->owner = std::move(owner);

    int ticks = static_cast<int>(std::ceil(timeoutSec / tickIntervalSec_));
    if (ticks <= 0) ticks = 1;
    if (ticks > bucketCount_) ticks = bucketCount_;

    int bucket = (currentBucket_ + ticks) % bucketCount_;
    attach(bucket, entry);
    return entry;
}

void TimeWheel::remove(Entry* entry) {
    if (!entry) return;
    if (entry->bucketIndex >= 0) {
        detach(entry);
    }
    delete entry;
}

void TimeWheel::refresh(Entry* entry) {
    if (!entry || entry->bucketIndex < 0) return;
    detach(entry);

    int ticks = static_cast<int>(std::ceil(entry->timeoutSec / tickIntervalSec_));
    if (ticks <= 0) ticks = 1;
    if (ticks > bucketCount_) ticks = bucketCount_;

    int bucket = (currentBucket_ + ticks) % bucketCount_;
    attach(bucket, entry);
}

void TimeWheel::tick() {
    currentBucket_ = (currentBucket_ + 1) % bucketCount_;

    Entry* head = buckets_[currentBucket_];
    buckets_[currentBucket_] = nullptr;

    while (head) {
        Entry* next = head->next;
        head->prev = nullptr;
        head->next = nullptr;
        head->bucketIndex = -1;

        if (!head->owner.expired()) {
            head->callback();
        }
        delete head;
        head = next;
    }
}

void TimeWheel::detach(Entry* entry) {
    int idx = entry->bucketIndex;
    if (idx < 0) return;

    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        buckets_[idx] = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    entry->prev = nullptr;
    entry->next = nullptr;
    entry->bucketIndex = -1;
}

void TimeWheel::attach(int bucket, Entry* entry) {
    entry->bucketIndex = bucket;
    entry->next = buckets_[bucket];
    if (buckets_[bucket]) {
        buckets_[bucket]->prev = entry;
    }
    buckets_[bucket] = entry;
}

} // namespace ezNet
