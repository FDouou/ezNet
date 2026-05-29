#include "core/TimeWheel.h"
#include <atomic>
#include <iostream>
#include <memory>

using namespace ezNet;

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; g_fail++; } \
    else { g_pass++; } \
} while(0)

void test_add_and_tick_expires() {
    TimeWheel tw(10, 0.1);

    std::atomic<bool> fired{false};
    auto owner = std::make_shared<int>(42);
    tw.add(0.3, [&]() { fired = true; }, owner);

    EXPECT(!fired.load(), "not fired before tick");

    for (int i = 0; i < 2; ++i) tw.tick();
    EXPECT(!fired.load(), "not fired after 2 ticks (0.2s)");

    tw.tick();
    EXPECT(fired.load(), "fired after 3 ticks (0.3s)");
}

void test_add_zero_timeout_fires_next_tick() {
    TimeWheel tw(10, 0.1);

    std::atomic<bool> fired{false};
    auto owner = std::make_shared<int>(42);
    tw.add(0.0, [&]() { fired = true; }, owner);

    EXPECT(!fired.load(), "not fired before tick");
    tw.tick();
    EXPECT(fired.load(), "fired after 1 tick for zero timeout");
}

void test_add_timeout_exceeds_bucket_count() {
    TimeWheel tw(5, 0.1);

    std::atomic<bool> fired{false};
    auto owner = std::make_shared<int>(42);
    tw.add(10.0, [&]() { fired = true; }, owner);

    for (int i = 0; i < 4; ++i) tw.tick();
    EXPECT(!fired.load(), "not fired before max bucket");

    tw.tick();
    EXPECT(fired.load(), "fired at max bucket when timeout exceeds count");
}

void test_refresh_resets_timer() {
    TimeWheel tw(10, 0.1);

    std::atomic<bool> fired{false};
    auto owner = std::make_shared<int>(42);
    auto* entry = tw.add(0.3, [&]() { fired = true; }, owner);

    for (int i = 0; i < 2; ++i) tw.tick();
    EXPECT(!fired.load(), "not fired after 2 ticks");

    tw.refresh(entry);
    EXPECT(!fired.load(), "not fired after refresh");

    for (int i = 0; i < 2; ++i) tw.tick();
    EXPECT(!fired.load(), "not fired after 2 more ticks");

    tw.tick();
    EXPECT(fired.load(), "fired after original timeout from refresh point");
}

void test_remove_cancels_timer() {
    TimeWheel tw(10, 0.1);

    std::atomic<bool> fired{false};
    auto owner = std::make_shared<int>(42);
    auto* entry = tw.add(0.3, [&]() { fired = true; }, owner);

    tw.remove(entry);

    for (int i = 0; i < 5; ++i) tw.tick();
    EXPECT(!fired.load(), "not fired after remove and 5 ticks");
}

void test_remove_nullptr_is_safe() {
    TimeWheel tw(10, 0.1);
    tw.remove(nullptr);
    EXPECT(true, "remove(nullptr) does not crash");
}

void test_refresh_nullptr_is_safe() {
    TimeWheel tw(10, 0.1);
    tw.refresh(nullptr);
    EXPECT(true, "refresh(nullptr) does not crash");
}

void test_owner_expired_skips_callback() {
    TimeWheel tw(10, 0.1);

    std::atomic<bool> fired{false};
    {
        auto owner = std::make_shared<int>(42);
        tw.add(0.3, [&]() { fired = true; }, owner);
    }

    for (int i = 0; i < 5; ++i) tw.tick();
    EXPECT(!fired.load(), "callback not fired when owner expired");
}

void test_owner_alive_callback_fires() {
    TimeWheel tw(10, 0.1);

    std::atomic<bool> fired{false};
    auto owner = std::make_shared<int>(42);
    tw.add(0.2, [&]() { fired = true; }, owner);

    for (int i = 0; i < 3; ++i) tw.tick();
    EXPECT(fired.load(), "callback fired when owner alive");
}

void test_multiple_entries_same_bucket() {
    TimeWheel tw(10, 0.1);

    std::atomic<int> count{0};
    auto owner = std::make_shared<int>(42);

    tw.add(0.3, [&]() { count++; }, owner);
    tw.add(0.3, [&]() { count++; }, owner);
    tw.add(0.3, [&]() { count++; }, owner);

    for (int i = 0; i < 3; ++i) tw.tick();
    EXPECT(count.load() == 3, "all 3 entries in same bucket fired");
}

void test_multiple_entries_different_buckets() {
    TimeWheel tw(10, 0.1);

    std::atomic<int> count{0};
    auto owner = std::make_shared<int>(42);

    tw.add(0.1, [&]() { count++; }, owner);
    tw.add(0.2, [&]() { count++; }, owner);

    tw.tick();
    EXPECT(count.load() == 1, "first entry fired after 1 tick");

    tw.tick();
    EXPECT(count.load() == 2, "second entry fired after 2 ticks");
}

void test_tick_wraps_around() {
    TimeWheel tw(5, 0.1);

    std::atomic<int> count{0};
    auto owner = std::make_shared<int>(42);

    tw.tick();
    tw.tick();
    tw.tick();

    tw.add(0.3, [&]() { count++; }, owner);

    for (int i = 0; i < 3; ++i) tw.tick();
    EXPECT(count.load() == 1, "entry fires correctly after wheel wrapped");
}

void test_double_remove_is_safe_via_nullptr() {
    TimeWheel tw(10, 0.1);

    auto owner = std::make_shared<int>(42);
    auto* entry = tw.add(0.3, [&]() {}, owner);
    tw.remove(entry);

    tw.remove(nullptr);
    EXPECT(true, "remove(nullptr) after removing entry does not crash");
}

void test_interleaved_add_remove() {
    TimeWheel tw(10, 0.1);

    std::atomic<int> count{0};
    auto owner = std::make_shared<int>(42);

    auto* keep = tw.add(0.3, [&]() { count++; }, owner);
    auto* remove = tw.add(0.3, [&]() { count += 100; }, owner);

    tw.remove(remove);

    for (int i = 0; i < 3; ++i) tw.tick();
    EXPECT(count.load() == 1, "only kept entry fired, removed entry did not");
}

int main() {
    std::cout << "=== TimeWheel Tests ===" << std::endl;

    test_add_and_tick_expires();
    test_add_zero_timeout_fires_next_tick();
    test_add_timeout_exceeds_bucket_count();
    test_refresh_resets_timer();
    test_remove_cancels_timer();
    test_remove_nullptr_is_safe();
    test_refresh_nullptr_is_safe();
    test_owner_expired_skips_callback();
    test_owner_alive_callback_fires();
    test_multiple_entries_same_bucket();
    test_multiple_entries_different_buckets();
    test_tick_wraps_around();
    test_double_remove_is_safe_via_nullptr();
    test_interleaved_add_remove();

    std::cout << "Passed: " << g_pass << ", Failed: " << g_fail << std::endl;
    return g_fail > 0 ? 1 : 0;
}
