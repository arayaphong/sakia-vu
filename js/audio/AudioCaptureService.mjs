/**
 * Owns the Web Audio graph and the lifecycle of the active capture stream.
 * Capture policy stays outside this class; concrete sources are injected by
 * mode and only need to implement `open({ deviceId })`.
 */
export class AudioCaptureService {
  static #ANALYSER_CONFIG = Object.freeze({
    fftSize: 4096,
    smoothingTimeConstant: 0.6,
    minDecibels: -100,
    maxDecibels: -30,
  });

  #mediaDevices;
  #audioContextFactory;
  #audioContext = null;
  #analyserNode = null;
  #mediaStream = null;
  #mediaStreamSource = null;

  running = false;
  sources;

  static get ANALYSER_CONFIG() {
    return AudioCaptureService.#ANALYSER_CONFIG;
  }

  constructor({ mediaDevices, audioContextFactory, sources }) {
    this.#mediaDevices = mediaDevices;
    this.#audioContextFactory = audioContextFactory;
    this.sources = sources;
  }

  async start({ mode, deviceId, onEnded } = {}) {
    this.stop();

    const audioContext = (
      this.#audioContext ??= this.#audioContextFactory()
    );
    if (audioContext.state === 'suspended') {
      await audioContext.resume?.();
    }

    const captureSource = this.sources.get(mode);
    if (!captureSource) {
      throw new Error(`unknown audio capture mode: ${mode}`);
    }

    const stream = await captureSource.open({ deviceId });
    this.#mediaStream = stream;

    const analyserNode = (
      this.#analyserNode ??= Object.assign(
        audioContext.createAnalyser(),
        AudioCaptureService.#ANALYSER_CONFIG,
      )
    );

    this.#mediaStreamSource = audioContext.createMediaStreamSource(stream);
    this.#mediaStreamSource.connect(analyserNode);

    const audioTrack = stream.getAudioTracks().at(0);
    const handleEnded = typeof onEnded === 'function' ? onEnded : undefined;
    audioTrack?.addEventListener('ended', () => {
      if (this.running && this.#mediaStream === stream) handleEnded?.();
    });

    this.running = true;
    return {
      analyserNode,
      sampleRate: audioContext.sampleRate,
    };
  }

  stop() {
    // Flip the state first so a track-ended event caused during teardown
    // cannot be observed as an externally initiated stop.
    this.running = false;

    const mediaStreamSource = this.#mediaStreamSource;
    this.#mediaStreamSource = null;
    try {
      mediaStreamSource?.disconnect();
    } catch {
      // A browser may throw when an already-disconnected node is detached.
    }

    const stream = this.#mediaStream;
    this.#mediaStream = null;
    (stream?.getTracks() ?? []).forEach(track => track.stop());
  }

  async listInputDevices() {
    const devices = await this.#mediaDevices.enumerateDevices();
    return devices.filter(({ kind }) => kind === 'audioinput');
  }
}

export default AudioCaptureService;
