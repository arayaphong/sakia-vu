const ANALYSER_CONFIG = Object.freeze({
  fftSize: 4096,
  smoothingTimeConstant: 0.6,
  minDecibels: -100,
  maxDecibels: -30,
});

/**
 * Owns the Web Audio graph and the lifecycle of the active capture stream.
 * Capture policy stays outside this class; concrete sources are injected by
 * mode and only need to implement `open({ deviceId })`.
 */
export class AudioCaptureService {
  constructor({ mediaDevices, audioContextFactory, sources }) {
    this.mediaDevices = mediaDevices;
    this.audioContextFactory = audioContextFactory;
    this.sources = sources;

    this.running = false;
    this.audioContext = null;
    this.analyserNode = null;
    this.mediaStream = null;
    this.mediaStreamSource = null;
  }

  async start({ mode, deviceId, onEnded } = {}) {
    this.stop();

    if (!this.audioContext) {
      this.audioContext = this.audioContextFactory();
    }
    if (this.audioContext.state === 'suspended') {
      await this.audioContext.resume();
    }

    const captureSource = this.sources.get(mode);
    if (!captureSource) {
      throw new Error(`unknown audio capture mode: ${mode}`);
    }

    const stream = await captureSource.open({ deviceId });
    this.mediaStream = stream;

    if (!this.analyserNode) {
      this.analyserNode = this.audioContext.createAnalyser();
      Object.assign(this.analyserNode, ANALYSER_CONFIG);
    }

    this.mediaStreamSource = this.audioContext.createMediaStreamSource(stream);
    this.mediaStreamSource.connect(this.analyserNode);

    const [audioTrack] = stream.getAudioTracks();
    if (audioTrack) {
      audioTrack.addEventListener('ended', () => {
        if (
          this.running
          && this.mediaStream === stream
          && typeof onEnded === 'function'
        ) {
          onEnded();
        }
      });
    }

    this.running = true;
    return {
      analyserNode: this.analyserNode,
      sampleRate: this.audioContext.sampleRate,
    };
  }

  stop() {
    // Flip the state first so a track-ended event caused during teardown
    // cannot be observed as an externally initiated stop.
    this.running = false;

    if (this.mediaStreamSource) {
      try {
        this.mediaStreamSource.disconnect();
      } catch {
        // A browser may throw when an already-disconnected node is detached.
      }
      this.mediaStreamSource = null;
    }

    if (this.mediaStream) {
      for (const track of this.mediaStream.getTracks()) track.stop();
      this.mediaStream = null;
    }
  }

  async listInputDevices() {
    const devices = await this.mediaDevices.enumerateDevices();
    return devices.filter(({ kind }) => kind === 'audioinput');
  }
}

export default AudioCaptureService;
