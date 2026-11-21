#pragma once
#include "Module.h"
#include "../NamEngine.h"
#include "../../Core/Params.h"

namespace obs::dsp
{
/** Every pedal / effect the plugin knows. Built once, never changes while running. */
const std::vector<ModuleType>& moduleTypes();
const ModuleType* findModule (const juce::String& key);

/** One rack (PEDALS, FX or SPECIAL FX tab): 5 slots run in order. */
class SlotRack
{
public:
    struct Instance
    {
        std::unique_ptr<Module> module;
        const ModuleType* type = nullptr;
    };

    void attach (ParamCache& cache, int rackIndex);
    void prepare (const ModuleContext& ctx);
    void reset();

    /** Message thread: put a module into a slot (nullptr / empty key = empty slot). Returns the new instance's type. */
    const ModuleType* setModule (int slot, const juce::String& key, const ModuleContext& ctx);
    juce::String moduleKey (int slot) const;
    Module* moduleAt (int slot) const;          // message thread only (for loadFile)

    void process (float* const* ch, int numCh, int n, const ModuleContext& ctx, bool rackOn);
    void collectGarbage (bool force)            { for (auto& s : slots) s.holder.collectGarbage (force); }
    int latency() const;

private:
    struct Slot
    {
        RtSlot<Instance> holder;
        std::atomic<float>* on = nullptr;
        std::atomic<float>* knobs[kSlotParams] {};
        Smoother fade;
        const void* lastInstance = nullptr;
        juce::String key;                       // message thread copy
    };
    Slot slots[kSlotsPerRack];
    std::vector<float> dry[2];
    int rack = 0;
};
} // namespace obs::dsp
