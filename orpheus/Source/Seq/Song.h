#pragma once
/*  ORPHEUS · the song and its player (the C++ copy of ui/src/orpheus.js's playing part).
    A song is four lines of dots {t: beat, m: MIDI note}. Between two dots on a line the pitch slides with an ease
    (u * u * (3 - 2u)); a line sounds from its first dot on. The player holds the notes (they ring until the line moves)
    and kicks the filter envelope wherever the chord name changes (the "marks", worked out by the UI).               */
#include <juce_core/juce_core.h>
#include <vector>
#include <array>

namespace orph
{
struct Song
{
    struct Dot { double t; float m; };
    static constexpr int VOICES = 4;
    std::array<std::vector<Dot>, VOICES> V;
    int bars = 10, sigN = 4, sigD = 4; double bpm = 80; bool loop = true;
    std::vector<double> marks;                 // beats where the chord name changes
    juce::String json;                         // the UI's own copy (K.orphSave()), kept with the plugin's state

    double barBeats() const noexcept { return sigN * 4.0 / sigD; }
    double length() const noexcept { return std::max (1, bars) * barBeats(); }

    /** the pitch of line v at beat t (false: the line hasn't started) */
    bool pitchAt (int v, double t, float& out) const noexcept
    {
        const auto& a = V[(size_t) v];
        if (a.empty() || t < a[0].t - 1e-9) return false;
        size_t lo = 0, hi = a.size() - 1;
        while (lo < hi) { const size_t mid = (lo + hi + 1) >> 1; if (a[mid].t <= t + 1e-9) lo = mid; else hi = mid - 1; }
        const Dot& A = a[lo];
        if (lo + 1 >= a.size() || a[lo + 1].m == A.m) { out = A.m; return true; }
        const Dot& B = a[lo + 1];
        const double u = std::max (0.0, std::min (1.0, (t - A.t) / std::max (1e-9, B.t - A.t)));
        out = (float) (A.m + (B.m - A.m) * (u * u * (3 - 2 * u)));
        return true;
    }
    int sounding (double t, float* out) const noexcept
    {
        int n = 0; for (int v = 0; v < VOICES; ++v) { float m; if (pitchAt (v, t, m)) out[n++] = m; }
        return n;
    }
    int markAt (double t) const noexcept
    {
        int k = -1; size_t lo = 0, hi = marks.size();
        while (lo < hi) { const size_t mid = (lo + hi) / 2; if (marks[mid] <= t + 1e-6) lo = mid + 1; else hi = mid; }
        k = (int) lo - 1; return k;
    }

    /** from the UI: { V: [[{t, m}...] x4], bars, sig: "4/4", bpm, loop, ... } and the marks */
    static std::unique_ptr<Song> fromJson (const juce::String& songJson, const juce::var& marks)
    {
        auto v = juce::JSON::parse (songJson);
        if (! v.isObject()) return nullptr;
        auto s = std::make_unique<Song>();
        auto* lines = v["V"].getArray();
        if (lines == nullptr || lines->size() != VOICES) return nullptr;
        for (int i = 0; i < VOICES; ++i)
            if (auto* dots = (*lines)[i].getArray())
            {
                for (auto& d : *dots)
                {
                    const double t = d["t"], m = d["m"];
                    if (std::isfinite (t) && std::isfinite (m)) s->V[(size_t) i].push_back ({ std::max (0.0, t), (float) juce::jlimit (36.0, 96.0, std::round (m)) });
                }
                std::stable_sort (s->V[(size_t) i].begin(), s->V[(size_t) i].end(), [] (const Dot& a, const Dot& b) { return a.t < b.t; });
            }
        s->bars = juce::jlimit (2, 999, (int) v.getProperty ("bars", 10));
        const auto sig = v.getProperty ("sig", "4/4").toString();
        const int n = sig.upToFirstOccurrenceOf ("/", false, false).getIntValue(), d = sig.fromFirstOccurrenceOf ("/", false, false).getIntValue();
        if (n > 0 && d > 0) { s->sigN = n; s->sigD = d; }
        s->bpm = juce::jlimit (40.0, 220.0, (double) v.getProperty ("bpm", 80));
        s->loop = ! v.hasProperty ("loop") || (bool) v["loop"];
        if (auto* mk = marks.getArray()) for (auto& m : *mk) { const double t = m; if (std::isfinite (t)) s->marks.push_back (t); }
        std::sort (s->marks.begin(), s->marks.end());
        s->json = songJson;
        return s;
    }
};
} // namespace orph
