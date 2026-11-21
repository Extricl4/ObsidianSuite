#pragma once
// =============================================================================
//  Capture amps: the finished amps of the amp pack (Content/Amps/<key>.json).
//  Each one is a real amp captured many times over (gain steps, channels,
//  modes, mics...). Its "map" says which capture plays for every setting of the
//  amp's knobs and buttons:
//    one   : a single capture; the gain knob drives its input
//    gain  : a knob steps through the files and blends neighbours
//    mid   : the same, with the middle knob
//    grid  : one knob steps the columns, another picks the row
//    near  : the files were captured at real knob settings; the two closest play
//  plus an optional selector knob (Eos's SOLO) and "rig" captures that already
//  contain a cab (the plugin's cab is bypassed for those).
//
//  CaptureAmpMap is that map compiled for the audio thread (no strings, no
//  allocation while playing). CaptureAmp is the map plus every capture loaded.
// =============================================================================
#include "DspCore.h"
#include "NamEngine.h"
#include <memory>
#include <vector>
#include <cmath>
#include <limits>

namespace obs::dsp
{
/** What a knob on a capture amp's face does. */
enum class KnobRole : int
{
    Plain = 0,      // only used to pick captures (Lead drive, Tone Cut, Lead pre...)
    Gain, Bass, Mid, Treble, Presence, Depth, Master,     // the plugin's AMP knobs (automatable)
    Level, LevelCh0, LevelCh1, Drive,                     // output / input trims
    Tight, Air, Body, Reverb, Speed, Intensity, ChorusSpeed, ChorusDepth,
    Selector
};

struct CaptureAmpMap
{
    static constexpr int kMaxKnobs = 16, kMaxDims = 4, kMaxSwitches = 4;

    struct Setting
    {
        enum By { One, Steps, Grid, Near } by = One;
        int knob = -1;                       // Steps: the knob that blends; One: the knob that drives the input
        std::vector<int> files;              // bank indices (Grid: rows x cols, row-major)
        int rows = 1, cols = 1, rowKnob = -1, colKnob = -1;
        std::vector<int> axes;               // Near
        std::vector<float> w, at;            // Near: at[point * axes + axis], NaN = "any"
        bool rig = false;
        uint32_t selects = 0;                // knobs this setting reads to pick files
        juce::String name;
    };
    struct Dim { juce::String id; int count = 1; bool per = false; int start = 0; std::vector<int> perKnobs; };

    juce::StringArray knobLabels;
    std::vector<KnobRole> roles;
    std::vector<float> defaults;             // 0..1
    std::vector<int> steps;                  // 0 = continuous
    int gainKnob = 0, midKnob = -1;
    std::vector<Dim> dims;
    std::vector<int> stride;
    std::vector<int> table;                  // flat dim key -> setting index
    std::vector<Setting> settings;
    int selKnob = -1, selSteps = 0;
    std::vector<int> selFiles;               // [0] = -1 (selector off)
    bool selRig = false;
    juce::StringArray switchIds;             // "crunch", "bright", "chorus"...
    juce::StringArray bank;                  // capture paths relative to Captures/, load order
    bool firstDimIsChannel = false;

    struct Pick
    {
        int a = -1, b = -1;                  // bank indices (b = -1: one capture)
        float f = 0.f;                       // blend toward b
        int setting = -1;
        bool rig = false, driveFromGain = true;
        int driveKnob = 0;
        uint32_t selects = 0;
    };

    /** The plugin parameter a role is wired to ("" = the knob is the amp's own). */
    static const char* paramFor (KnobRole r)
    {
        switch (r)
        {
            case KnobRole::Gain: return "amp.gain";       case KnobRole::Bass: return "amp.bass";
            case KnobRole::Mid: return "amp.mid";         case KnobRole::Treble: return "amp.treble";
            case KnobRole::Presence: return "amp.presence"; case KnobRole::Depth: return "amp.depth";
            case KnobRole::Master: return "amp.master";
            default: return "";
        }
    }
    static const char* roleName (KnobRole r)
    {
        static const char* n[] = { "plain", "gain", "bass", "mid", "treble", "presence", "depth", "master", "level", "level", "level", "drive",
                                   "tight", "air", "body", "reverb", "speed", "intensity", "speed", "depth", "selector" };
        return n[(int) r];
    }

    /** Roles from the knob names (the same rules the UI shows). */
    void assignRoles (bool hasChorus)
    {
        const int n = knobLabels.size();
        roles.assign ((size_t) n, KnobRole::Plain);
        uint32_t picks = 0;
        for (auto& s : settings) picks |= s.selects;
        bool hasMaster = false;
        for (auto& l : knobLabels) if (l.equalsIgnoreCase ("master")) hasMaster = true;
        bool masterTaken = hasMaster;
        for (int j = 0; j < n; ++j)
        {
            const auto l = knobLabels[j].toLowerCase().trim();
            KnobRole r = KnobRole::Plain;
            if (j == gainKnob) r = KnobRole::Gain;
            else if (j == selKnob) r = KnobRole::Selector;
            else if (l == "bass" || l == "low") r = KnobRole::Bass;
            else if (l == "middle" || l == "mid") r = KnobRole::Mid;
            else if (l == "treble" || l == "high") r = KnobRole::Treble;
            else if (l == "presence") r = KnobRole::Presence;
            else if (l == "depth") r = hasChorus ? KnobRole::ChorusDepth : KnobRole::Depth;
            else if (l == "resonance") r = KnobRole::Depth;
            else if (l == "master") r = KnobRole::Master;
            else if (l == "volume" || l == "output" || l == "level")
            {
                if (! masterTaken) { r = KnobRole::Master; masterTaken = true; }
                else r = KnobRole::Level;
            }
            else if (l == "rhythm post") r = KnobRole::LevelCh0;
            else if (l == "lead post")   r = KnobRole::LevelCh1;
            else if (l == "tight")  r = KnobRole::Tight;
            else if (l == "air")    r = KnobRole::Air;
            else if (l == "body")   r = KnobRole::Body;
            else if (l == "reverb") r = KnobRole::Reverb;
            else if (l == "speed")  r = hasChorus ? KnobRole::ChorusSpeed : KnobRole::Speed;
            else if (l == "intensity") r = KnobRole::Intensity;
            else if (! (picks & (1u << j)) && l.contains ("drive")) r = KnobRole::Drive;   // a drive no setting uses: boost into the captures
            roles[(size_t) j] = r;
        }
    }

    /** Message thread: compile the "map" of Content/Amps/<key>.json. `face` (Content/Faces/<key>.json,
        may be void) gives the knob defaults and steps. Returns nullptr and fills err if it can't. */
    static std::shared_ptr<CaptureAmpMap> compile (const juce::var& map, const juce::var& face, juce::String& err)
    {
        auto m = std::make_shared<CaptureAmpMap>();
        auto* knobs = map["knobs"].getArray();
        auto* setObj = map["settings"].getDynamicObject();
        if (knobs == nullptr || setObj == nullptr || knobs->isEmpty()) { err = "the amp file has no wiring"; return nullptr; }
        for (auto& k : *knobs) m->knobLabels.add (k.toString());
        if (m->knobLabels.size() > kMaxKnobs) { err = "too many knobs"; return nullptr; }
        const int nk = m->knobLabels.size();
        m->gainKnob = juce::jlimit (0, nk - 1, (int) map.getProperty ("gainKnob", 0));
        m->midKnob = map.hasProperty ("mid") && ! map["mid"].isVoid() ? juce::jlimit (0, nk - 1, (int) map["mid"]) : -1;
        m->defaults.assign ((size_t) nk, 0.5f);
        m->steps.assign ((size_t) nk, 0);
        if (auto* fk = face["knobs"].getArray())
            for (int j = 0; j < juce::jmin (nk, fk->size()); ++j)
            {
                m->defaults[(size_t) j] = juce::jlimit (0.f, 1.f, (float) (double) (*fk)[j].getProperty ("v", 0.5));
                m->steps[(size_t) j] = (int) (*fk)[j].getProperty ("steps", 0);
            }
        if (auto* st = map["steps"].getDynamicObject())
            for (auto& p : st->getProperties())
                if (auto* names = p.value.getArray()) { const int j = p.name.toString().getIntValue(); if (j >= 0 && j < nk) m->steps[(size_t) j] = names->size(); }

        auto bankIndex = [&m] (const juce::String& path) -> int
        {
            const int i = m->bank.indexOf (path);
            if (i >= 0) return i;
            m->bank.add (path);
            return m->bank.size() - 1;
        };

        // dims (the amp's buttons)
        if (auto* btns = map["buttons"].getArray())
            for (auto& b : *btns)
            {
                Dim d; d.id = b["id"].toString(); d.per = b.hasProperty ("per"); d.start = (int) b.getProperty ("start", 0);
                if (auto* names = b["names"].getArray()) d.count = names->size(); else d.count = 0;
                if (auto* pk = b["perknobs"].getArray()) for (auto& k : *pk) d.perKnobs.push_back ((int) k);
                m->dims.push_back (d);
            }
        if ((int) m->dims.size() > kMaxDims) { err = "too many buttons"; return nullptr; }
        m->firstDimIsChannel = ! m->dims.empty() && m->dims[0].id == "channel";

        // settings
        std::vector<std::vector<int>> keys;
        for (auto& p : setObj->getProperties())
        {
            const auto& v = p.value;
            Setting s;
            const auto by = v["by"].toString();
            s.rig = (bool) v.getProperty ("rig", false);
            s.name = v.getProperty ("name", v.getProperty ("label", "")).toString();
            if (by == "grid")
            {
                s.by = Setting::Grid;
                s.rowKnob = (int) v["rowKnob"]; s.colKnob = (int) v["colKnob"];
                auto* rows = v["files"].getArray();
                if (rows == nullptr || rows->isEmpty()) { err = "empty grid"; return nullptr; }
                s.rows = rows->size(); s.cols = 0;
                for (auto& r : *rows)
                {
                    auto* cols = r.getArray();
                    if (cols == nullptr || cols->isEmpty()) { err = "empty grid row"; return nullptr; }
                    if (s.cols == 0) s.cols = cols->size();
                    if (cols->size() != s.cols) { err = "ragged grid"; return nullptr; }
                    for (auto& f : *cols) s.files.push_back (bankIndex (f.toString()));
                }
                s.selects = (1u << s.rowKnob) | (1u << s.colKnob);
            }
            else
            {
                auto* files = v["files"].getArray();
                if (files == nullptr || files->isEmpty()) { err = "a setting has no files"; return nullptr; }
                for (auto& f : *files) s.files.push_back (bankIndex (f.toString()));
                if (by == "near")
                {
                    s.by = Setting::Near;
                    if (auto* ax = v["axes"].getArray()) for (auto& a : *ax) s.axes.push_back ((int) a);
                    if (auto* w = v["w"].getArray()) for (auto& x : *w) s.w.push_back ((float) (double) x);
                    s.w.resize (s.axes.size(), 1.f);
                    auto* at = v["at"].getArray();
                    if (at == nullptr || at->size() != (int) s.files.size()) { err = "near: points and files differ"; return nullptr; }
                    for (auto& pt : *at)
                    {
                        auto* c = pt.getArray();
                        for (size_t k = 0; k < s.axes.size(); ++k)
                            s.at.push_back (c != nullptr && (int) k < c->size() && ! (*c)[(int) k].isVoid() ? (float) (double) (*c)[(int) k] : std::numeric_limits<float>::quiet_NaN());
                    }
                    for (int a : s.axes) s.selects |= 1u << a;
                }
                else if (by == "gain" || by == "mid")
                {
                    s.by = s.files.size() > 1 ? Setting::Steps : Setting::One;
                    s.knob = v.hasProperty ("knob") ? (int) v["knob"] : (by == "mid" ? m->midKnob : m->gainKnob);
                    if (s.knob < 0) s.knob = m->gainKnob;
                    if (s.by == Setting::Steps) s.selects = 1u << s.knob;
                    else s.knob = -1;
                }
                else { s.by = Setting::One; s.knob = v.hasProperty ("knob") ? (int) v["knob"] : -1; }
            }
            for (int k : { s.knob, s.rowKnob, s.colKnob }) if (k >= nk) { err = "a setting uses a knob that isn't there"; return nullptr; }
            for (int k : s.axes) if (k < 0 || k >= nk) { err = "a setting uses a knob that isn't there"; return nullptr; }
            std::vector<int> key;
            const auto ks = p.name.toString();
            if (ks.isNotEmpty()) for (auto& t : juce::StringArray::fromTokens (ks, "|", "")) key.push_back (t.getIntValue());
            key.resize (m->dims.size(), 0);
            keys.push_back (key);
            m->settings.push_back (std::move (s));
        }
        if (m->settings.empty()) { err = "no settings"; return nullptr; }

        // selector (Eos SOLO)
        if (auto sel = map["selector"]; sel.isObject())
        {
            m->selKnob = (int) sel["knob"];
            m->selRig = (bool) sel.getProperty ("rig", false);
            if (auto* fs = sel["files"].getArray())
                for (auto& f : *fs) m->selFiles.push_back (f.isVoid() || f.toString().isEmpty() ? -1 : bankIndex (f.toString()));
            m->selSteps = (int) m->selFiles.size();
            if (m->selKnob < 0 || m->selKnob >= nk || m->selSteps < 2) { m->selKnob = -1; m->selFiles.clear(); }
            else m->steps[(size_t) m->selKnob] = m->selSteps;
        }
        if (auto* sw = map["switches"].getArray())
            for (auto& s : *sw) if (m->switchIds.size() < kMaxSwitches) m->switchIds.add (s["id"].toString());

        // dim sizes, the lookup table (missing combinations fall back to the same channel, then the first setting)
        for (size_t i = 0; i < m->dims.size(); ++i)
        {
            int mx = 0; for (auto& k : keys) mx = juce::jmax (mx, k[i] + 1);
            m->dims[i].count = juce::jmax (1, juce::jmax (m->dims[i].count, mx));
        }
        int size = 1;
        m->stride.assign (m->dims.size(), 1);
        for (int i = (int) m->dims.size() - 1; i >= 0; --i) { m->stride[(size_t) i] = size; size *= m->dims[(size_t) i].count; }
        m->table.assign ((size_t) size, -1);
        for (size_t s = 0; s < keys.size(); ++s)
        {
            int idx = 0; bool ok = true;
            for (size_t i = 0; i < keys[s].size(); ++i) { if (keys[s][i] < 0 || keys[s][i] >= m->dims[i].count) ok = false; idx += keys[s][i] * m->stride[i]; }
            if (ok) m->table[(size_t) idx] = (int) s;
        }
        for (int idx = 0; idx < size; ++idx)
        {
            if (m->table[(size_t) idx] >= 0) continue;
            int fb = 0;
            if (! m->dims.empty())
            {
                const int ch = idx / m->stride[0];
                for (size_t s = 0; s < keys.size(); ++s) if (keys[s][0] == ch) { fb = (int) s; break; }
            }
            m->table[(size_t) idx] = fb;
        }
        bool hasChorus = m->switchIds.contains ("chorus");
        m->assignRoles (hasChorus);
        return m;
    }

    int settingFor (const int* dimVals) const
    {
        int idx = 0;
        for (size_t i = 0; i < dims.size(); ++i) idx += juce::jlimit (0, dims[i].count - 1, dimVals[i]) * stride[i];
        return table[(size_t) idx];
    }

    /** Audio thread. knobs: 0..1 per face knob (stepped knobs already rounded). */
    Pick resolve (const float* k, const int* dimVals) const
    {
        Pick p;
        if (selKnob >= 0)
        {
            const int si = juce::jlimit (0, selSteps - 1, (int) std::lround (k[selKnob] * (float) (selSteps - 1)));
            if (si > 0 && selFiles[(size_t) si] >= 0)
            {
                p.a = selFiles[(size_t) si]; p.rig = selRig; p.setting = (int) settings.size() + si;
                p.driveKnob = gainKnob; p.driveFromGain = true;
                return p;
            }
        }
        const int si = settingFor (dimVals);
        const auto& s = settings[(size_t) si];
        p.setting = si; p.rig = s.rig; p.selects = s.selects;
        p.driveFromGain = ! (s.selects & (1u << gainKnob));
        p.driveKnob = (s.by == Setting::One && s.knob >= 0) ? s.knob : gainKnob;
        auto steps1 = [&p] (const std::vector<int>& files, int base, int n, float v)
        {
            if (n <= 1) { p.a = files[(size_t) base]; return; }
            const float pos = juce::jlimit (0.f, 1.f, v) * (float) (n - 1);
            const int i = juce::jmin (n - 2, (int) std::floor (pos));
            p.a = files[(size_t) (base + i)]; p.b = files[(size_t) (base + i + 1)]; p.f = pos - (float) i;
        };
        switch (s.by)
        {
            case Setting::One:   p.a = s.files[0]; break;
            case Setting::Steps: steps1 (s.files, 0, (int) s.files.size(), k[s.knob]); break;
            case Setting::Grid:
            {
                const int r = juce::jlimit (0, s.rows - 1, (int) std::lround (k[s.rowKnob] * (float) (s.rows - 1)));
                steps1 (s.files, r * s.cols, s.cols, k[s.colKnob]);
                break;
            }
            case Setting::Near:
            {
                const size_t na = s.axes.size();
                float d1 = 1e30f, d2 = 1e30f; int i1 = 0, i2 = -1;
                for (size_t i = 0; i < s.files.size(); ++i)
                {
                    float q = 0.f;
                    for (size_t a = 0; a < na; ++a)
                    {
                        const float t = s.at[i * na + a];
                        if (std::isnan (t)) continue;
                        const float dd = t - k[s.axes[a]] * 10.f;
                        q += s.w[a] * dd * dd;
                    }
                    const float d = std::sqrt (q);
                    if (d < d1) { d2 = d1; i2 = i1; d1 = d; i1 = (int) i; }
                    else if (d < d2) { d2 = d; i2 = (int) i; }
                }
                p.a = s.files[(size_t) i1];
                if (i2 >= 0 && i2 != i1) { p.b = s.files[(size_t) i2]; p.f = d1 / (d1 + d2 + 1e-9f); }
                break;
            }
        }
        if (p.b >= 0 && p.f < 0.02f) { p.b = -1; p.f = 0.f; }
        if (p.b >= 0 && p.f > 0.98f) { p.a = p.b; p.b = -1; p.f = 0.f; }
        return p;
    }
};

/** A capture amp ready to play: the map and every capture, in map->bank order (nullptr = missing). */
struct CaptureAmp
{
    std::shared_ptr<const CaptureAmpMap> map;
    std::vector<std::unique_ptr<NamEngine>> engines;
    juce::String key;
    float trimDb = 0.f;                       // level match (see levelMatch below)

    /** Level matching, so every amp in the pack plays about as loud as the in-house amps: the amp's
        default capture plays a fixed test riff when it loads, and the amp is trimmed to a target level.
        Captures that contain a cab skip the plugin's cab, so they get the cab's typical loss on top. */
    static constexpr float kTargetDb = -16.5f, kRigDb = -13.f;
    void levelMatch (double sr, int block)
    {
        if (map == nullptr) return;
        float k[CaptureAmpMap::kMaxKnobs] {};
        for (size_t j = 0; j < map->defaults.size() && j < (size_t) CaptureAmpMap::kMaxKnobs; ++j) k[j] = map->defaults[j];
        if (map->selKnob >= 0) k[map->selKnob] = 0.f;
        int dv[CaptureAmpMap::kMaxDims] {};
        for (size_t i = 0; i < map->dims.size(); ++i) dv[i] = map->dims[i].start;
        const auto pk = map->resolve (k, dv);
        auto* e = juce::isPositiveAndBelow (pk.a, (int) engines.size()) ? engines[(size_t) pk.a].get() : nullptr;
        if (e == nullptr) return;
        std::vector<float> x;
        testRiff (x, sr, 1.0);
        for (size_t i = 0; i < x.size(); i += (size_t) block) e->process (x.data() + i, (int) juce::jmin ((size_t) block, x.size() - i));
        double acc = 0; const size_t from = x.size() / 4;
        for (size_t i = from; i < x.size(); ++i) acc += (double) x[i] * x[i];
        const float rmsDb = gainToDb ((float) std::sqrt (acc / (double) (x.size() - from)));
        trimDb = clampf (kTargetDb - rmsDb, -18.f, 18.f);
    }
    /** Plucked strings (Karplus-Strong), the same every time. */
    static void testRiff (std::vector<float>& out, double sr, double seconds)
    {
        const int n = (int) (sr * seconds);
        out.assign ((size_t) n, 0.f);
        const float notes[] = { 82.41f, 110.f, 146.83f, 98.f };
        std::vector<float> line; int len = 0, pos = 0; uint32_t seed = 12345u;
        const int step = juce::jmax (1, (int) (sr * 0.25));
        for (int i = 0; i < n; ++i)
        {
            if (i % step == 0)
            {
                len = juce::jmax (2, (int) (sr / notes[(i / step) % 4]));
                line.assign ((size_t) len, 0.f);
                for (auto& v : line) { seed = seed * 1664525u + 1013904223u; v = ((float) (seed >> 9) * (1.f / 8388608.f) * 2.f - 1.f) * 0.35f; }
                pos = 0;
            }
            const int nx = (pos + 1) % len;
            const float y = line[(size_t) pos];
            line[(size_t) pos] = 0.996f * 0.5f * (y + line[(size_t) nx]);
            pos = nx;
            out[(size_t) i] = y;
        }
    }
};

/** Small mono room for the amps that have a Reverb knob (Freeverb-style combs + allpasses). */
class AmpReverb
{
public:
    void prepare (double sr)
    {
        const double k = sr / 44100.0;
        const int combs[4] = { 1116, 1188, 1277, 1356 }, aps[2] = { 556, 441 };
        for (int i = 0; i < 4; ++i) { len[i] = juce::jmax (8, (int) (combs[i] * k)); comb[i].prepare (len[i] + 4); comb[i].clear(); lp[i] = 0.f; }
        for (int i = 0; i < 2; ++i) { alen[i] = juce::jmax (8, (int) (aps[i] * k)); ap[i].prepare (alen[i] + 4); ap[i].clear(); }
    }
    inline float process (float x)
    {
        float s = 0.f;
        for (int i = 0; i < 4; ++i)
        {
            const float y = comb[i].at (len[i] - 1);
            lp[i] = y + 0.3f * (lp[i] - y);
            comb[i].push (x * 0.25f + lp[i] * 0.8f);
            s += y;
        }
        for (int i = 0; i < 2; ++i) s = ap[i].process (s, (float) alen[i], 0.5f);
        return s;
    }
private:
    DelayLine comb[4]; Allpass ap[2];
    int len[4] {}, alen[2] {};
    float lp[4] {};
};
} // namespace obs::dsp
