#include "NamEngine.h"

#include <NAM/dsp.h>
#include <NAM/get_dsp.h>
#include <NAM/model_config.h>
#include <NAM/container.h>
#include <NAM/convnet.h>
#include <NAM/linear.h>
#include <NAM/lstm.h>
#include <NAM/sequential.h>
#include <NAM/wavenet/model.h>
#include <mutex>
#include <juce_audio_basics/juce_audio_basics.h>

// AudioDSPTools' resampler expects two constants from iPlug2; provide them here.
namespace iplug { static constexpr double PI = 3.14159265358979323846; }
#ifndef DEFAULT_BLOCK_SIZE
 #define DEFAULT_BLOCK_SIZE 512
#endif
#include <dsp/ResamplingContainer/ResamplingContainer.h>

namespace obs::dsp
{
namespace
{
/** NAM registers its model types (WaveNet, SlimmableContainer, ...) from static initialisers in their own
    source files. Linked through static archives (the plugin's shared code is one), files nothing calls are
    dropped and those types vanish: "No config parser registered for architecture: SlimmableContainer"
    (the format TONE3000 exports today). Naming every type here keeps them in, and registers any still missing. */
void ensureNamArchitectures()
{
    static std::once_flag once;
    std::call_once (once, []
    {
        using Fn = std::unique_ptr<nam::ModelConfig> (*) (const nlohmann::json&, double);
        const std::pair<const char*, Fn> types[] = {
            { "WaveNet", &nam::wavenet::create_config },  { "SlimmableContainer", &nam::container::create_config },
            { "LSTM", &nam::lstm::create_config },        { "ConvNet", &nam::convnet::create_config },
            { "Linear", &nam::linear::create_config },    { "Sequential", &nam::sequential::create_config } };
        auto& reg = nam::ConfigParserRegistry::instance();
        for (auto& [name, fn] : types)
            if (! reg.has (name)) reg.registerParser (name, fn);
    });
}
} // namespace

struct NamEngine::Impl
{
    std::unique_ptr<nam::DSP> model;
    std::unique_ptr<::dsp::ResamplingContainer<float, 1, 12>> resampler;
    std::vector<float> in, out, out2;
    int maxBlock = 0;
    int outChannels = 1;
    float gain = 1.f;            // loudness compensation (linear)
    bool failed = false;

    void runModel (float** inputs, float** outputs, int n)
    {
        // NAM may not run in place; copy through scratch buffers. Also guards odd channel counts.
        if (n > (int) in.size()) n = (int) in.size();
        std::copy (inputs[0], inputs[0] + n, in.data());
        float* ins[1] = { in.data() };
        float* outs[2] = { out.data(), out2.data() };
        model->process (ins, outs, n);
        float* dst = outputs[0];
        if (outChannels > 1)
            for (int i = 0; i < n; ++i) dst[i] = 0.5f * (out[(size_t) i] + out2[(size_t) i]);
        else
            std::copy (out.data(), out.data() + n, dst);
    }
};

NamEngine::~NamEngine() = default;

std::unique_ptr<NamEngine> NamEngine::load (const juce::File& file, double hostRate, int maxBlock, juce::String& error)
{
    try
    {
        ensureNamArchitectures();
        if (! file.existsAsFile()) { error = "File not found: " + file.getFullPathName(); return nullptr; }
        if (file.getSize() > 200 * 1024 * 1024) { error = "File is too big to be a NAM capture"; return nullptr; }

        const auto text = file.loadFileAsString();
        nlohmann::json j = nlohmann::json::parse (text.toStdString(), nullptr, false);
        if (j.is_discarded()) { error = "Not a valid .nam file (JSON could not be read)"; return nullptr; }

        nam::dspData data;
        nam::DspLoadOptions opts; opts.prewarm = false;
        auto model = nam::get_dsp (j, data, opts);
        if (model == nullptr) { error = "This capture type isn't supported"; return nullptr; }

        std::unique_ptr<NamEngine> e (new NamEngine());
        e->impl = std::make_unique<Impl>();
        e->impl->model = std::move (model);
        e->impl->outChannels = juce::jmax (1, e->impl->model->NumOutputChannels());
        e->source = file;
        e->displayName = file.getFileNameWithoutExtension();
        e->modelRate = data.expected_sample_rate > 0 ? data.expected_sample_rate : 48000.0;

        if (e->impl->model->NumInputChannels() != 1) { error = "Only mono-input captures are supported"; return nullptr; }

        if (e->impl->model->HasLoudness())
        {
            // Normalise every capture to the same perceived level (-18 dB, like the NAM plugin).
            e->loudnessDb = juce::jlimit (-24.f, 24.f, (float) (-18.0 - e->impl->model->GetLoudness()));
        }
        e->impl->gain = juce::Decibels::decibelsToGain (e->loudnessDb);

        if (! e->prepare (hostRate, maxBlock)) { error = "Could not prepare the capture for this sample rate"; return nullptr; }
        return e;
    }
    catch (const std::exception& ex)
    {
        error = juce::String ("Couldn't load capture: ") + ex.what();
    }
    catch (...)
    {
        error = "Couldn't load capture (unknown error)";
    }
    return nullptr;
}

bool NamEngine::prepare (double rate, int block)
{
    try
    {
        hostRate = rate;
        auto& m = *impl->model;
        impl->maxBlock = juce::jmax (32, block);
        const bool needsResample = ! m.SupportsArbitrarySampleRate() && std::abs (rate - modelRate) > 0.5;
        const double ratio = needsResample ? modelRate / rate : 1.0;
        const int inner = (int) std::ceil (impl->maxBlock * ratio * 1.25) + 64;
        impl->in.assign ((size_t) inner, 0.f);
        impl->out.assign ((size_t) inner, 0.f);
        impl->out2.assign ((size_t) inner, 0.f);

        m.SetPrewarmOnReset (true);
        if (needsResample)
        {
            m.Reset (modelRate, inner);
            impl->resampler = std::make_unique<::dsp::ResamplingContainer<float, 1, 12>> (modelRate);
            impl->resampler->Reset (rate, impl->maxBlock);
        }
        else
        {
            m.Reset (m.SupportsArbitrarySampleRate() ? rate : modelRate, inner);
            impl->resampler.reset();
        }
        return true;
    }
    catch (...) { impl->failed = true; return false; }
}

int NamEngine::latencySamples() const
{
    return impl != nullptr && impl->resampler != nullptr ? impl->resampler->GetLatency() : 0;
}

void NamEngine::process (float* io, int n)
{
    if (impl == nullptr || impl->failed) return;
    // Larger-than-prepared blocks are split (some hosts lie about the max block size).
    int done = 0;
    while (done < n)
    {
        const int len = juce::jmin (impl->maxBlock, n - done);
        float* p = io + done;
        if (impl->resampler != nullptr)
        {
            float* ins[1] = { p };
            float* outs[1] = { p };
            impl->resampler->ProcessBlock (ins, outs, len, [this] (float** a, float** b, int k) { impl->runModel (a, b, k); });
        }
        else
        {
            float* ins[1] = { p }; float* outs[1] = { p };
            impl->runModel (ins, outs, len);
        }
        const float g = impl->gain;
        for (int i = 0; i < len; ++i)
        {
            float v = p[i] * g;
            if (! std::isfinite (v)) v = 0.f;          // a broken capture can never blow up the output
            p[i] = v;
        }
        done += len;
    }
}
} // namespace obs::dsp
