# RAND

Ein rauer Percussion-Synth für das M-Vave FM-1, angelehnt an das Konzept des Behringer EDGE / Moog DFAM.
Eine einzige Stimme, ein 8-Step-Sequencer mit zwei Reihen, und ein Takt, der bis in den hörbaren Bereich hochgedreht werden kann.

by swtdr34mz

## Aufbau

```
core/    Klangkern – ohne Bezug zu FM-1 oder VST, wird von beiden genutzt
tools/   Demo-Renderer zum Probehören am PC
```

**Stimme:** 2 Oszillatoren (Dreieck/Puls), FM 1>2, Hard Sync, Rauschen (weiß↔pink),
24-dB-Ladder-Filter (LP/HP), Pitch-/Filter-/VCA-Hüllkurve, Drive.

**Sequencer:** 8 Steps mit Pitch + Velocity + An/Aus, Länge 1–8,
Clock-Multiplier /4 … x128 (ab ca. x32 wird aus dem Rhythmus ein Ton), MIDI-Clock-Sync.

**Presets:** RAND KICK, EBM BASS, METAL HATS, IRON TOMS, CRACK, ZAP, AUDIO RATE, BREAKCORE.

## Demo bauen

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/render_demo ausgabeordner
```

## Geplant

- FM-1-Firmware (Bedienung, Display, MIDI)
- VST3-Plugin mit demselben Kern
