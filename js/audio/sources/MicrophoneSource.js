/**
 * Browser adapter for microphone capture.
 *
 * The source has one responsibility: translate the application's capture
 * request into the exact MediaDevices constraints used by SakiaVU.
 */
export class MicrophoneSource {
  constructor({ mediaDevices }) {
    this.mediaDevices = mediaDevices;
  }

  async open({ deviceId } = {}) {
    return this.mediaDevices.getUserMedia({
      audio: {
        deviceId: deviceId ? { exact: deviceId } : undefined,
        echoCancellation: false,
        noiseSuppression: false,
        autoGainControl: false,
      },
    });
  }
}

export default MicrophoneSource;
