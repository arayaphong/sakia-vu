import { AppController } from './app/AppController.js';
import { AudioCaptureService } from './audio/AudioCaptureService.js';
import { MicrophoneSource } from './audio/sources/MicrophoneSource.js';
import { TabAudioSource } from './audio/sources/TabAudioSource.js';
import { SpectrumAnalyzer } from './audio/SpectrumAnalyzer.js';
import { MeterLayout } from './core/MeterLayout.js';
import { MatterPhysicsWorld } from './physics/MatterPhysicsWorld.js';
import { MeterCanvasRenderer } from './rendering/MeterCanvasRenderer.js';
import { AppView } from './ui/AppView.js';

const layout = new MeterLayout();
const view = new AppView({
  document: window.document,
  window,
  layout,
});

const mediaDevices = window.navigator.mediaDevices;
const audioCapture = new AudioCaptureService({
  mediaDevices,
  audioContextFactory: () => new window.AudioContext(),
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
  requestAnimationFrame: callback => window.requestAnimationFrame(callback),
});

controller.start();
