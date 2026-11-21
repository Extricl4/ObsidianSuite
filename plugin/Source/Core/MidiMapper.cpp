#include "MidiMapper.h"

namespace obs
{
static int indexOfParam (const juce::String& id)
{
    const auto& defs = paramDefs();
    for (size_t i = 0; i < defs.size(); ++i) if (defs[i].id == id) return (int) i;
    return -1;
}

std::vector<MidiMapper::Mapping> MidiMapper::defaultMappings()
{
    std::vector<Mapping> m;
    auto add = [&] (int cc, const char* id, bool toggle)
    {
        Mapping x; x.cc = cc; x.paramId = id; x.paramIndex = indexOfParam (id); x.toggle = toggle;
        if (x.paramIndex >= 0) m.push_back (x);
    };
    add (11, "out.gain", false);      // expression pedal -> output volume
    add (64, "doubler.on", true);     // footswitch -> doubler
    add (20, "gate.thresh", false);   // knob -> gate threshold
    return m;
}

void MidiMapper::install (std::vector<Mapping> m)
{
    for (auto& x : m) x.paramIndex = indexOfParam (x.paramId);
    m.erase (std::remove_if (m.begin(), m.end(), [] (const Mapping& x) { return x.paramIndex < 0 || x.cc < 0 || x.cc > 127; }), m.end());
    {
        const juce::ScopedLock sl (lock);
        mappings = m;
    }
    table.set (std::make_unique<std::vector<Mapping>> (std::move (m)));
}

void MidiMapper::fromJson (const juce::var& v)
{
    std::vector<Mapping> m;
    if (auto* a = v.getArray())
        for (auto& e : *a)
        {
            Mapping x;
            x.cc = (int) e["cc"]; x.channel = (int) e["channel"]; x.paramId = e["param"].toString();
            x.lo = e.hasProperty ("lo") ? (float) (double) e["lo"] : 0.f;
            x.hi = e.hasProperty ("hi") ? (float) (double) e["hi"] : 1.f;
            x.toggle = (bool) e["toggle"];
            m.push_back (x);
        }
    install (std::move (m));
}

juce::var MidiMapper::toJson() const
{
    const juce::ScopedLock sl (lock);
    juce::Array<juce::var> a;
    const auto& defs = paramDefs();
    for (auto& x : mappings)
    {
        auto o = new juce::DynamicObject();
        o->setProperty ("cc", x.cc); o->setProperty ("channel", x.channel); o->setProperty ("param", x.paramId);
        o->setProperty ("paramName", x.paramIndex >= 0 ? defs[(size_t) x.paramIndex].name : x.paramId);
        o->setProperty ("lo", x.lo); o->setProperty ("hi", x.hi); o->setProperty ("toggle", x.toggle);
        a.add (juce::var (o));
    }
    return a;
}

void MidiMapper::remove (int index)
{
    auto m = current();
    if (index >= 0 && index < (int) m.size()) m.erase (m.begin() + index);
    install (std::move (m));
}

void MidiMapper::push (const Event& e)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) events[s1] = e; else if (n2 > 0) events[s2] = e; else return;   // full: drop (never block)
    fifo.finishedWrite (n1 + n2);
}

void MidiMapper::process (const juce::MidiBuffer& midi, int channelFilter, bool pcEnabled)
{
    const auto* tbl = table.get();
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (channelFilter > 0 && msg.getChannel() != channelFilter) continue;
        if (msg.isController())
        {
            const int cc = msg.getControllerNumber(), val = msg.getControllerValue();
            if (learning.load()) { push ({ 2, cc, 0.f, msg.getChannel() }); continue; }
            if (tbl == nullptr) continue;
            for (auto& m : *tbl)
            {
                if (m.cc != cc || (m.channel > 0 && m.channel != msg.getChannel())) continue;
                const float t = m.toggle ? (val >= 64 ? 1.f : 0.f) : (float) val / 127.f;
                push ({ 0, m.paramIndex, m.lo + (m.hi - m.lo) * t, 0 });
            }
        }
        else if (msg.isProgramChange() && pcEnabled)
            push ({ 1, msg.getProgramChangeNumber(), 0.f, 0 });
    }
}

juce::String MidiMapper::dispatch (juce::AudioProcessorValueTreeState& state, const std::function<void (int)>& onProgramChange)
{
    juce::String learned;
    const auto& defs = paramDefs();
    int s1, n1, s2, n2;
    fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
    auto handle = [&] (const Event& e)
    {
        if (e.type == 0 && e.a >= 0 && e.a < (int) defs.size())
        {
            if (auto* p = state.getParameter (defs[(size_t) e.a].id)) p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, e.b));
        }
        else if (e.type == 1) { if (onProgramChange) onProgramChange (e.a); }
        else if (e.type == 2 && learning.load())
        {
            auto m = current();
            m.erase (std::remove_if (m.begin(), m.end(), [&] (const Mapping& x) { return x.cc == e.a && (x.channel == 0 || x.channel == e.c); }), m.end());
            Mapping x; x.cc = e.a; x.channel = 0; x.paramId = learnTarget;
            const int idx = indexOfParam (learnTarget);
            x.toggle = idx >= 0 && defs[(size_t) idx].type == ParamType::Bool;
            m.push_back (x);
            learning.store (false);
            learned = "CC " + juce::String (e.a) + " now controls " + (idx >= 0 ? defs[(size_t) idx].name : learnTarget);
            learnTarget = {};
            install (std::move (m));
        }
    };
    for (int i = 0; i < n1; ++i) handle (events[s1 + i]);
    for (int i = 0; i < n2; ++i) handle (events[s2 + i]);
    fifo.finishedRead (n1 + n2);
    return learned;
}
} // namespace obs
