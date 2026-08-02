#pragma once

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <limits>

class RecentTrafficWindow
{
public:
    static constexpr qint64 kWindowMs = 1000;
    static constexpr qint64 kBucketDurationMs = 1;
    static constexpr int kBucketCount = static_cast<int>(kWindowMs / kBucketDurationMs) + 1;

    void add(qint64 timestampMs, qint64 byteCount = 0)
    {
        const qint64 bucketStartMs = timestampMs - timestampMs % kBucketDurationMs;
        const qint64 rawBucketIndex = (bucketStartMs / kBucketDurationMs) % kBucketCount;
        const int bucketIndex = static_cast<int>(
            rawBucketIndex >= 0 ? rawBucketIndex : rawBucketIndex + kBucketCount);
        Bucket &bucket = m_buckets.at(static_cast<std::size_t>(bucketIndex));
        if (bucket.startMs != bucketStartMs) {
            if (bucket.startMs > bucketStartMs) {
                return;
            }
            bucket = {};
            bucket.startMs = bucketStartMs;
        }
        ++bucket.eventCount;
        bucket.byteCount += byteCount > 0 ? byteCount : 0;
    }

    int eventCount(qint64 nowMs) const
    {
        qint64 total = 0;
        for (const Bucket &bucket : m_buckets) {
            if (isWithinWindow(bucket, nowMs)) {
                total += bucket.eventCount;
            }
        }
        return static_cast<int>((std::min)(
            total,
            qint64((std::numeric_limits<int>::max)())));
    }

    qint64 byteCount(qint64 nowMs) const
    {
        qint64 total = 0;
        for (const Bucket &bucket : m_buckets) {
            if (isWithinWindow(bucket, nowMs)) {
                total += bucket.byteCount;
            }
        }
        return total;
    }

    int storedEventCount() const
    {
        qint64 total = 0;
        for (const Bucket &bucket : m_buckets) {
            total += bucket.eventCount;
        }
        return static_cast<int>((std::min)(
            total,
            qint64((std::numeric_limits<int>::max)())));
    }

    int activeBucketCount() const
    {
        int total = 0;
        for (const Bucket &bucket : m_buckets) {
            total += bucket.eventCount > 0 ? 1 : 0;
        }
        return total;
    }

    bool isEmpty() const
    {
        return activeBucketCount() == 0;
    }

    void clear()
    {
        m_buckets = {};
    }

private:
    struct Bucket {
        qint64 startMs = -1;
        qint64 eventCount = 0;
        qint64 byteCount = 0;
    };

    static bool isWithinWindow(const Bucket &bucket, qint64 nowMs)
    {
        if (bucket.eventCount <= 0 || bucket.startMs > nowMs) {
            return false;
        }
        const qint64 cutoffMs = nowMs - kWindowMs;
        return bucket.startMs >= cutoffMs;
    }

    std::array<Bucket, kBucketCount> m_buckets {};
};
