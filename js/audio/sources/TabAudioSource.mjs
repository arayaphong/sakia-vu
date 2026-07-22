/** Browser adapter for audio selected through the display-share picker. */
export class TabAudioSource {
  static #DISPLAY_CONSTRAINTS = Object.freeze({
    video: true,
    audio: true,
  });

  #mediaDevices;

  constructor({ mediaDevices }) {
    this.#mediaDevices = mediaDevices;
  }

  async open(_options = {}) {
    const stream = await this.#mediaDevices.getDisplayMedia(
      TabAudioSource.#DISPLAY_CONSTRAINTS,
    );

    // A video track is required to present the share picker, but SakiaVU only
    // consumes audio after the user has chosen a tab or screen.
    for (const track of stream.getVideoTracks()) track.stop();

    if (stream.getAudioTracks().length === 0) {
      throw new Error('no audio shared with the tab/screen');
    }

    return stream;
  }
}

export default TabAudioSource;
