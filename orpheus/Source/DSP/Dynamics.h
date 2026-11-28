#pragma once
/*  ORPHEUS · the end of the preview's chain: Chrome's DynamicsCompressorNode (its static curve, knee solver and automatic
    makeup gain, copied from Blink's DynamicsCompressorKernel; the envelope is a plain attack / release in dB), used twice:
    SIREN's glue compressor (-12 dB, 3:1, 30 dB knee) and the safety limiter (-2 dB, 20:1, hard knee) before a soft clip. */
#include "Prims.h"

namespace orph
{
class WACompressor
{
public:
    void set (double sampleRate, double thresholdDb, double kneeDb, double ratio, double attackSec, double releaseSec)
    {
        sr = sampleRate; dbThr = thresholdDb; dbKnee = kneeDb; slope = 1.0 / ratio;
        linThr = dbToGain (dbThr);
        K = kAtSlope (slope);
        kneeThrDb = dbThr + dbKnee; kneeThr = dbToGain (kneeThrDb);
        yKneeThrDb = lin2db (kneeCurve (kneeThr, K));
        makeup = std::pow (1.0 / saturate (1.0, K), 0.6);            // Blink: fullRangeMakeupGain^0.6
        att = tcCoef (attackSec, sr); rel = tcCoef (releaseSec, sr);
        gDb = 0;
    }
    void reset() noexcept { gDb = 0; }
    inline void process (float& l, float& r) noexcept
    {
        const double x = std::max (std::abs (l), std::abs (r));
        double want = 0;
        if (x > linThr) want = lin2db (saturate (x, K) / x);
        gDb += (want - gDb) * (want < gDb ? att : rel);
        const float g = (float) (dbToGain (gDb) * makeup);
        l *= g; r *= g;
    }
    double makeupGain() const noexcept { return makeup; }

private:
    static double lin2db (double x) noexcept { return 20.0 * std::log10 (std::max (1e-12, x)); }
    double kneeCurve (double x, double k) const noexcept
    {
        if (x < linThr) return x;
        return linThr + (1 - std::exp (-k * (x - linThr))) / k;
    }
    double saturate (double x, double k) const noexcept
    {
        if (x < kneeThr) return kneeCurve (x, k);
        return dbToGain (yKneeThrDb + slope * (lin2db (x) - kneeThrDb));
    }
    double slopeAt (double x, double k) const noexcept
    {
        if (x < linThr) return 1;
        const double x2 = x * 1.001, xDb = lin2db (x), x2Db = lin2db (x2);
        return (lin2db (kneeCurve (x2, k)) - lin2db (kneeCurve (x, k))) / (x2Db - xDb);
    }
    double kAtSlope (double desired) const noexcept
    {
        const double x = dbToGain (dbThr + dbKnee);
        double minK = .1, maxK = 10000, k = 5;
        for (int i = 0; i < 15; ++i)
        {
            if (slopeAt (x, k) < desired) maxK = k; else minK = k;
            k = std::sqrt (minK * maxK);
        }
        return k;
    }
    double sr = 48000, dbThr = -24, dbKnee = 30, slope = 1.0 / 12, linThr = .06, K = 5, kneeThrDb = 6, kneeThr = 2, yKneeThrDb = 0, makeup = 1;
    double att = .01, rel = .001, gDb = 0;
};
} // namespace orph
