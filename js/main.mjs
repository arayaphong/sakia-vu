import { AppController } from './app/AppController.mjs';
import { AudioCaptureService } from './audio/AudioCaptureService.mjs';
import { MicrophoneSource } from './audio/sources/MicrophoneSource.mjs';
import { TabAudioSource } from './audio/sources/TabAudioSource.mjs';
import { SpectrumAnalyzer } from './audio/SpectrumAnalyzer.mjs';
import { MeterLayout } from './core/MeterLayout.mjs';
import { MatterPhysicsWorld } from './physics/MatterPhysicsWorld.mjs';
import { MeterCanvasRenderer } from './rendering/MeterCanvasRenderer.mjs';
import { AppView } from './ui/AppView.mjs';

const {
  AudioContext,
  document,
  navigator: { mediaDevices },
} = window;

const layout = new MeterLayout();
const view = new AppView({
  document,
  window,
  layout,
});

const audioCapture = new AudioCaptureService({
  mediaDevices,
  audioContextFactory: () => new AudioContext(),
  sources: new Map([
    ['mic', new MicrophoneSource({ mediaDevices })],
    ['tab', new TabAudioSource({ mediaDevices })],
  ]),
});

const analyzer = new SpectrumAnalyzer({ layout });
const renderer = new MeterCanvasRenderer({
  canvas: view.canvas,
  layout,
  devicePixelRatio: () => window.devicePixelRatio || 1,
});
const physicsWorld = new MatterPhysicsWorld({
  Matter: globalThis.Matter,
  layout,
});

const controller = new AppController({
  view,
  audioCapture,
  analyzer,
  physicsWorld,
  renderer,
  requestAnimationFrame: window.requestAnimationFrame.bind(window),
});

controller.start();
