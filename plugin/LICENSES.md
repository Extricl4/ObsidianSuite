# Third-party code in Obsidian Suite

Downloaded at build time (see `cmake/Dependencies.cmake`), not stored in this repo.

| Library | Used for | Licence |
|---|---|---|
| [JUCE 8](https://juce.com) | plugin/app framework, audio devices, UI host | JUCE 8 licence (free Starter tier for personal use) or AGPLv3 |
| [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) | playing `.nam` captures | MIT |
| [AudioDSPTools](https://github.com/sdatkinson/AudioDSPTools) | resampling captures to your sample rate | MIT |
| [Eigen](https://eigen.tuxfamily.org) | maths for NAM | MPL 2.0 |
| [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) | HQ transpose | MIT |
| [Signalsmith Linear](https://github.com/Signalsmith-Audio/linear) | FFT for Signalsmith Stretch | MIT |
| [Steinberg ASIO SDK](https://www.steinberg.net/developers/) | ASIO in the standalone app | GPLv3 or Steinberg licence |
| Microsoft WebView2 SDK | showing the interface | Microsoft (BSD-style) |

VST is a registered trademark of Steinberg Media Technologies GmbH.
This build is for personal use. Selling it would need a paid JUCE licence and the Steinberg ASIO licence.

Pedal pack (`Content/`): captures and IRs made by other people; creators listed in `Content/README.md`.
