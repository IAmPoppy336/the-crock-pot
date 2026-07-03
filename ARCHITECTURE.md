# The Crock-Pot — Architecture

## The shape
```
Ableton Live 12 (host)
        │  audio + params
        ▼
┌─────────────────────────────────────────────┐
│ CrockPotProcessor (JUCE AudioProcessor)      │
│  APVTS parameter tree  ·  preset manager     │
│                                              │
│  AUDIO THREAD (realtime-safe, no alloc/lock):│
│   input                                      │
│    → [MonoMaker <110Hz]                       │
│    → dsp::ProcessorChain of FX blocks:       │
│        Saturation(OS) → Resampler(OS) → Tape │
│        → Chorus → Tremolo → Reverse → Delay  │
│        → Reverb    (order user-reorderable)  │
│    → Simmer macro fans out to block params   │
│    → output                                  │
│                                              │
│  MESSAGE/BACKGROUND THREAD (non-realtime):   │
│   Splitter: audio→temp WAV→HPSS/transient    │
│     →stems→Leftovers one-shot export         │
│   FX Generator (Shake the Pot) randomizer    │
└─────────────────────────────────────────────┘
        ▲  param binding (atomics / value tree)
        │
┌─────────────────────────────────────────────┐
│ CrockPotEditor — NATIVE JUCE Components      │
│  (custom LookAndFeel, no web tech)           │
│  Simple mode · Advanced mode · preset browser│
│  Simmer dial · steam/heat meter · Shake btn  │
└─────────────────────────────────────────────┘

Registers as an audio-effect device (VST3 `Fx` / AU `aufx`) → shows up in
Ableton's device chain and nests inside an Audio Effect Rack. Full state
save/restore so rack macros + presets recall.
```

## Module map (src/)
- `src/PluginProcessor.*` — AudioProcessor, APVTS, block-buffer plumbing.
- `src/PluginEditor.*` — native JUCE editor: builds the Component tree, owns the custom LookAndFeel.
- `src/dsp/` — one file per FX block, all sharing a `CrockBlock` interface (`prepare/reset/process/setBypass/dryWet`). Plus `MonoMaker`, `SimmerMacro`, oversampling helpers.
- `src/ui/` — native JUCE Components (knobs, meters, panels), the `CrockPotLookAndFeel`, and theme art (custom-drawn steam/dial graphics). No html/css/js.
- `src/splitter/` — offline `DrumSplitter` (HPSS + transient + band-split) and `LeftoversExporter`. Runs off the audio thread. ML "Slow-Cook" (ONNX/DrumSep) slots in here at M6.

## Hard invariants (from research — do not violate)
1. Audio thread: no alloc, no locks, no I/O, no system calls. [Beat B]
2. All DSP-loop params smoothed; denormals flushed on reverb/IIR tails. [Beat B]
3. Nonlinear blocks oversampled (2–8x). Mono-maker protects sub. [Beat B]
4. Splitter is 100% offline/background. [Beat C]
5. No LarsNet weights in any build (license). DrumSep/Demucs/Spleeter = MIT OK. [Beat C]
6. Build with CMake (from pamplejuce), validate with pluginval before any Live test. [Beat A/E]
7. Register as an **audio effect** (VST3 `Fx` / AU `aufx`), NOT an instrument — so Ableton drops it into the device chain / Audio Effect Rack. Implement full `getStateInformation`/`setStateInformation` so rack presets + macros recall. [D9]
8. **Native UI only** — no WebView, no bundled browser, no web assets. [D3]

## Cross-platform notes
One codebase. Mac universal (arm64+x86_64) + Windows via CMake + GitHub Actions. VST3 both OSes, AU on Mac. Install paths + bypass instructions in Beat E.
