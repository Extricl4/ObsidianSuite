# Plugin (to come)

This is where the JUCE VST3 build of Obsidian will live. It's written in the **Program** chat.

Plan:
- Load NAM captures with NeuralAmpModelerCore (MIT-licensed).
- Load cab IRs with convolution.
- `rig.json` maps each amp to a `.nam` file and each cab to an IR:
  ```json
  { "amps": { "nyx": "captures/peavey-6505.nam" }, "cabs": { "nyx_cab": "irs/4x12-v30.wav" } }
  ```
- Amp knobs: input drive into the capture, a post tone-stack EQ, and presence/depth. Gain can blend between several captures.
- Pedals: drive pedals can be NAM captures; delay, reverb, modulation, pitch and glitch are coded DSP.
- Graphics come from each approved item's asset pack in `visuals/`: a face with no knobs, a knob filmstrip and a layout JSON.
