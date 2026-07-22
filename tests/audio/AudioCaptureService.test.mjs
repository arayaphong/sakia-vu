import assert from 'node:assert/strict';
import test from 'node:test';

import { AudioCaptureService } from '../../js/audio/AudioCaptureService.mjs';
import { MicrophoneSource } from '../../js/audio/sources/MicrophoneSource.mjs';
import { TabAudioSource } from '../../js/audio/sources/TabAudioSource.mjs';

class FakeTrack {
  constructor(kind) {
    this.kind = kind;
    this.stopCalls = 0;
    this.listeners = new Map();
  }

  stop() {
    this.stopCalls += 1;
  }

  addEventListener(type, listener) {
    this.listeners.set(type, listener);
  }

  dispatch(type) {
    this.listeners.get(type)?.();
  }
}

class FakeStream {
  constructor({ audioTracks = [], videoTracks = [], otherTracks = [] } = {}) {
    this.audioTracks = audioTracks;
    this.videoTracks = videoTracks;
    this.otherTracks = otherTracks;
  }

  getAudioTracks() {
    return this.audioTracks;
  }

  getVideoTracks() {
    return this.videoTracks;
  }

  getTracks() {
    return [...this.audioTracks, ...this.videoTracks, ...this.otherTracks];
  }
}

function createAudioContext() {
  const analyserNode = {};
  const mediaStreamSources = [];
  const context = {
    state: 'running',
    sampleRate: 48000,
    analyserNode,
    analyserCreateCalls: 0,
    resumeCalls: 0,
    createAnalyser() {
      this.analyserCreateCalls += 1;
      return analyserNode;
    },
    createMediaStreamSource(stream) {
      const node = {
        stream,
        connectedTo: [],
        disconnectCalls: 0,
        connect(target) {
          this.connectedTo.push(target);
        },
        disconnect() {
          this.disconnectCalls += 1;
        },
      };
      mediaStreamSources.push(node);
      return node;
    },
    async resume() {
      this.resumeCalls += 1;
      this.state = 'running';
    },
  };

  return { context, analyserNode, mediaStreamSources };
}

test('MicrophoneSource preserves the microphone constraints and exact device selection', async () => {
  const stream = new FakeStream();
  const calls = [];
  const source = new MicrophoneSource({
    mediaDevices: {
      async getUserMedia(constraints) {
        calls.push(constraints);
        return stream;
      },
    },
  });

  assert.equal(await source.open({ deviceId: 'studio-mic' }), stream);
  assert.deepEqual(calls[0], {
    audio: {
      deviceId: { exact: 'studio-mic' },
      echoCancellation: false,
      noiseSuppression: false,
      autoGainControl: false,
    },
  });

  await source.open();
  assert.deepEqual(calls[1], {
    audio: {
      deviceId: undefined,
      echoCancellation: false,
      noiseSuppression: false,
      autoGainControl: false,
    },
  });
});

test('TabAudioSource requests display audio and stops every picker video track', async () => {
  const audioTrack = new FakeTrack('audio');
  const videoTracks = [new FakeTrack('video'), new FakeTrack('video')];
  const stream = new FakeStream({ audioTracks: [audioTrack], videoTracks });
  const calls = [];
  const source = new TabAudioSource({
    mediaDevices: {
      async getDisplayMedia(constraints) {
        calls.push(constraints);
        return stream;
      },
    },
  });

  assert.equal(await source.open(), stream);
  assert.deepEqual(calls, [{ video: true, audio: true }]);
  assert.deepEqual(videoTracks.map(({ stopCalls }) => stopCalls), [1, 1]);
  assert.equal(audioTrack.stopCalls, 0);
});

test('TabAudioSource rejects a share without audio after stopping its video tracks', async () => {
  const videoTrack = new FakeTrack('video');
  const source = new TabAudioSource({
    mediaDevices: {
      async getDisplayMedia() {
        return new FakeStream({ videoTracks: [videoTrack] });
      },
    },
  });

  await assert.rejects(
    source.open(),
    new Error('no audio shared with the tab/screen'),
  );
  assert.equal(videoTrack.stopCalls, 1);
});

test('AudioCaptureService configures one analyser and connects capture only to it', async () => {
  const track = new FakeTrack('audio');
  const stream = new FakeStream({ audioTracks: [track] });
  const sourceCalls = [];
  const { context, analyserNode, mediaStreamSources } = createAudioContext();
  context.state = 'suspended';
  let contextFactoryCalls = 0;
  const service = new AudioCaptureService({
    mediaDevices: {},
    audioContextFactory() {
      contextFactoryCalls += 1;
      return context;
    },
    sources: new Map([['mic', {
      async open(options) {
        sourceCalls.push(options);
        return stream;
      },
    }]]),
  });

  const result = await service.start({ mode: 'mic', deviceId: 'input-1' });

  assert.equal(contextFactoryCalls, 1);
  assert.equal(context.resumeCalls, 1);
  assert.deepEqual(sourceCalls, [{ deviceId: 'input-1' }]);
  assert.equal(context.analyserCreateCalls, 1);
  assert.deepEqual(analyserNode, {
    fftSize: 4096,
    smoothingTimeConstant: 0.6,
    minDecibels: -100,
    maxDecibels: -30,
  });
  assert.deepEqual(mediaStreamSources[0].connectedTo, [analyserNode]);
  assert.deepEqual(result, { analyserNode, sampleRate: 48000 });
  assert.equal(service.running, true);

  const nextTrack = new FakeTrack('audio');
  const nextStream = new FakeStream({ audioTracks: [nextTrack] });
  service.sources.set('mic', { async open() { return nextStream; } });
  await service.start({ mode: 'mic' });

  assert.equal(contextFactoryCalls, 1);
  assert.equal(context.analyserCreateCalls, 1);
  assert.deepEqual(mediaStreamSources[1].connectedTo, [analyserNode]);
});

test('AudioCaptureService stop disconnects and stops every track idempotently', async () => {
  const tracks = [
    new FakeTrack('audio'),
    new FakeTrack('video'),
    new FakeTrack('other'),
  ];
  const stream = new FakeStream({
    audioTracks: [tracks[0]],
    videoTracks: [tracks[1]],
    otherTracks: [tracks[2]],
  });
  const { context, mediaStreamSources } = createAudioContext();
  const service = new AudioCaptureService({
    mediaDevices: {},
    audioContextFactory: () => context,
    sources: new Map([['tab', { async open() { return stream; } }]]),
  });
  await service.start({ mode: 'tab' });

  service.stop();
  service.stop();

  assert.equal(service.running, false);
  assert.equal(mediaStreamSources[0].disconnectCalls, 1);
  assert.deepEqual(tracks.map(({ stopCalls }) => stopCalls), [1, 1, 1]);
});

test('AudioCaptureService invokes onEnded only for the currently running stream', async () => {
  const firstTrack = new FakeTrack('audio');
  const firstStream = new FakeStream({ audioTracks: [firstTrack] });
  const secondTrack = new FakeTrack('audio');
  const secondStream = new FakeStream({ audioTracks: [secondTrack] });
  const streams = [firstStream, secondStream];
  const { context } = createAudioContext();
  const service = new AudioCaptureService({
    mediaDevices: {},
    audioContextFactory: () => context,
    sources: new Map([['mic', { async open() { return streams.shift(); } }]]),
  });
  let endedCalls = 0;

  await service.start({ mode: 'mic', onEnded: () => { endedCalls += 1; } });
  service.stop();
  firstTrack.dispatch('ended');
  assert.equal(endedCalls, 0);

  await service.start({ mode: 'mic', onEnded: () => { endedCalls += 1; } });
  firstTrack.dispatch('ended');
  assert.equal(endedCalls, 0);
  secondTrack.dispatch('ended');
  assert.equal(endedCalls, 1);
});

test('AudioCaptureService lists only audio input devices', async () => {
  const devices = [
    { kind: 'audioinput', deviceId: 'mic-1' },
    { kind: 'audiooutput', deviceId: 'speakers-1' },
    { kind: 'videoinput', deviceId: 'camera-1' },
    { kind: 'audioinput', deviceId: 'mic-2' },
  ];
  const service = new AudioCaptureService({
    mediaDevices: { async enumerateDevices() { return devices; } },
    audioContextFactory() {
      throw new Error('not needed when listing devices');
    },
    sources: new Map(),
  });

  assert.deepEqual(await service.listInputDevices(), [devices[0], devices[3]]);
});
