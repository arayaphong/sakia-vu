/**
 * Browser adapter for microphone capture.
 *
 * The source has one responsibility: translate the application's capture
 * request into the exact MediaDevices constraints used by SakiaVU.
 */
export class MicrophoneSource {
  static #AUDIO_CONSTRAINTS = Object.freeze({
    echoCancellation: false,
    noiseSuppression: false,
    autoGainControl: false,
  });

  #mediaDevices;

  constructor({ mediaDevices }) {
    this.#mediaDevices = mediaDevices;
  }

  async open({ deviceId } = {}) {
    return this.#mediaDevices.getUserMedia({
      audio: {
        deviceId: deviceId ? { exact: deviceId } : undefined,
        ...MicrophoneSource.#AUDIO_CONSTRAINTS,
      },
    });
  }
}

export default MicrophoneSource;
