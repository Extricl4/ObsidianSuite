#include "ModuleRegistry.h"

namespace obs::dsp
{
void registerDriveModules (std::vector<ModuleType>&);
void registerPitchModules (std::vector<ModuleType>&);
void registerTimeModules (std::vector<ModuleType>&);
void registerModModules (std::vector<ModuleType>&);
void registerPackModules (std::vector<ModuleType>&);
void registerCelestialModules (std::vector<ModuleType>&);

const std::vector<ModuleType>& moduleTypes()
{
    static const std::vector<ModuleType> types = []
    {
        std::vector<ModuleType> t;
        registerDriveModules (t);
        registerPitchModules (t);
        registerTimeModules (t);
        registerModModules (t);
        registerPackModules (t);
        registerCelestialModules (t);
        for (auto& m : t) { juce::ignoreUnused (m); jassert (m.params.size() <= (size_t) kSlotParams); }   // a module may have at most 8 knobs
        return t;
    }();
    return types;
}

const ModuleType* findModule (const juce::String& key)
{
    for (auto& m : moduleTypes()) if (m.key == key) return &m;
    return nullptr;
}

void SlotRack::attach (ParamCache& cache, int rackIndex)
{
    rack = rackIndex;
    for (int s = 0; s < kSlotsPerRack; ++s)
    {
        slots[s].on = cache.ptr (slotOnId (rack, s));
        for (int k = 0; k < kSlotParams; ++k) slots[s].knobs[k] = cache.ptr (slotParamId (rack, s, k));
    }
}

void SlotRack::prepare (const ModuleContext& ctx)
{
    for (auto& d : dry) d.assign ((size_t) ctx.maxBlock, 0.f);
    for (auto& s : slots)
    {
        s.fade.reset (ctx.sampleRate, 20.f, 1.f);
        if (auto* inst = s.holder.get()) if (inst->module) inst->module->prepare (ctx);
    }
}

void SlotRack::reset()
{
    for (auto& s : slots) if (auto* inst = s.holder.get()) if (inst->module) inst->module->reset();
}

const ModuleType* SlotRack::setModule (int slot, const juce::String& key, const ModuleContext& ctx)
{
    if (slot < 0 || slot >= kSlotsPerRack) return nullptr;
    auto& s = slots[slot];
    const ModuleType* type = key.isEmpty() ? nullptr : findModule (key);
    if (type == nullptr)
    {
        s.key = {};
        s.holder.set (nullptr);
        return nullptr;
    }
    auto inst = std::make_unique<Instance>();
    inst->type = type;
    inst->module = type->create();
    inst->module->prepare (ctx);
    s.key = key;
    s.holder.set (std::move (inst));
    return type;
}

juce::String SlotRack::moduleKey (int slot) const   { return slot >= 0 && slot < kSlotsPerRack ? slots[slot].key : juce::String(); }
Module* SlotRack::moduleAt (int slot) const
{
    if (slot < 0 || slot >= kSlotsPerRack) return nullptr;
    auto* inst = slots[slot].holder.get();
    return inst != nullptr ? inst->module.get() : nullptr;
}

int SlotRack::latency() const
{
    int l = 0;
    for (auto& s : slots) if (auto* inst = s.holder.get()) if (inst->module) l += inst->module->latency();
    return l;
}

void SlotRack::process (float* const* ch, int numCh, int n, const ModuleContext& ctx, bool rackOn)
{
    n = juce::jmin (n, (int) dry[0].size());
    numCh = juce::jmin (numCh, 2);

    for (auto& s : slots)
    {
        Instance* inst = s.holder.get();
        if (inst == nullptr || inst->module == nullptr) { s.lastInstance = inst; s.holder.blockDone(); continue; }

        if (inst != s.lastInstance) { s.lastInstance = inst; s.fade.snap (0.f); }
        const bool on = s.on->load (std::memory_order_relaxed) > 0.5f && rackOn;
        s.fade.set (on ? 1.f : 0.f);
        if (! on && ! s.fade.isSmoothing() && s.fade.current() <= 0.f) { s.holder.blockDone(); continue; }

        // Knob values in real units.
        float p[kSlotParams] {};
        const auto& specs = inst->type->params;
        for (size_t k = 0; k < specs.size() && k < (size_t) kSlotParams; ++k)
            p[k] = specs[k].fromNorm (s.knobs[k]->load (std::memory_order_relaxed));

        const bool fading = s.fade.isSmoothing() || s.fade.current() < 1.f;
        if (fading) for (int c = 0; c < numCh; ++c) std::copy (ch[c], ch[c] + n, dry[c].data());

        inst->module->process (ch, numCh, n, p, ctx);

        // Safety: a module can never pass NaN/Inf or crazy levels downstream.
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                float v = ch[c][i];
                if (! std::isfinite (v)) { v = 0.f; inst->module->reset(); }
                ch[c][i] = clampf (v, -8.f, 8.f);
            }

        if (fading)
        {
            for (int i = 0; i < n; ++i)
            {
                const float g = s.fade.next();
                for (int c = 0; c < numCh; ++c) ch[c][i] = dry[c][(size_t) i] + (ch[c][i] - dry[c][(size_t) i]) * g;
            }
        }
        s.holder.blockDone();
    }
    // Turning a whole rack off fades every slot to its dry signal (handled per slot above).
}
} // namespace obs::dsp
