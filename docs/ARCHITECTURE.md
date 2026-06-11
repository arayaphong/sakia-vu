# SakiaVU Code Structure

This project is being organized around strict SOLID boundaries and constructor-based
dependency injection. The `app` layer coordinates use cases through interfaces, while
PipeWire, FFTW, GTK, and Skia stay behind concrete adapter classes.

## Source Tree

```
src/
  app/
    main.cpp                 Composition root; constructs concrete dependencies
    AppController.{h,cpp}    GTK application flow, controls, tick callback

  core/
    interfaces/
      IAudioSource.h         Audio capture contract
      ISpectrumAnalyzer.h    DSP/analyzer contract
      IMeterWidget.h         UI meter host contract
      IMeterWidgetFactory.h  Factory for creating meter widgets after GTK activation
    models/
      MeterState.h           Analyzer-to-renderer data model

  audio/
    PipeWireAudioCapture.*   IAudioSource implementation backed by PipeWire
    FftwSpectrumAnalyzer.*   ISpectrumAnalyzer implementation backed by FFTW3

  ui/
    GtkMeterWidget.*         IMeterWidget implementation using GtkDrawingArea
    SkiaMeterRenderer.*      Skia drawing implementation for MeterState

tools/
  render_test.cpp            Offscreen smoke test: synthetic audio -> PNG
```

## Dependency Direction

Dependencies should point inward:

```
app  -> core/interfaces, core/models
audio -> core/interfaces, core/models
ui    -> core/interfaces, core/models
```

`src/app/main.cpp` is the composition root. It is the only production code that should
construct the concrete implementations and pass them into `AppController`.

Current wiring:

```
PipeWireAudioCapture -> IAudioSource
FftwSpectrumAnalyzer -> ISpectrumAnalyzer
GtkMeterWidgetFactory -> IMeterWidgetFactory
GtkMeterWidget -> IMeterWidget
```

`AppController` owns only interface pointers:

```
IAudioSource
ISpectrumAnalyzer
IMeterWidgetFactory
IMeterWidget
```

## Runtime Data Flow

```
IAudioSource::latest(sampleCount)
  -> ISpectrumAnalyzer::update
  -> MeterState
  -> IMeterWidget::updateState
  -> GtkMeterWidget
  -> SkiaMeterRenderer::draw
  -> SkSurface
  -> cairo
```

GTK lifecycle matters: the meter widget is created during application activation via
`IMeterWidgetFactory`, not in the `AppController` constructor. This keeps GTK widget
creation tied to the active application/display state while preserving dependency
injection.

## Layer Rules

- `core/` must not include GTK, PipeWire, FFTW, or Skia headers.
- `app/` should depend on abstractions, not concrete audio or UI classes.
- `audio/` may include PipeWire/FFTW headers, but should expose behavior through
  `IAudioSource` and `ISpectrumAnalyzer`.
- `ui/` may include GTK/Skia headers, but should expose widget behavior through
  `IMeterWidget`.
- Shared state crossing boundaries should use `core/models/MeterState`.
- New infrastructure should be introduced through a small interface only when the app
  layer genuinely needs to vary or test that behavior.

## Testing Structure

`render-test` intentionally avoids GTK and PipeWire. It constructs
`FftwSpectrumAnalyzer` and `SkiaMeterRenderer` directly, feeds deterministic synthetic
samples, and writes a PNG. Keep it as a fast smoke test for DSP/rendering regressions.
