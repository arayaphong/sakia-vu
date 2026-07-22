export class AppView {
  #document;
  #window;
  #layout;
  #elements;
  #handlersBound = false;

  constructor({ document, window, layout }) {
    this.#document = document;
    this.#window = window;
    this.#layout = layout;

    const requiredElement = id => {
      const element = document.getElementById(id);
      if (!element) throw new Error(`Missing required element: #${id}`);
      return element;
    };

    this.#elements = {
      canvas: requiredElement('meter'),
      listen: requiredElement('listen'),
      mode: requiredElement('mode'),
      device: requiredElement('device'),
      gain: requiredElement('gain'),
      gainValue: requiredElement('gainVal'),
      peak: requiredElement('peak'),
      physics: requiredElement('physics'),
      physicsControls: requiredElement('physicsControls'),
      dropBall: requiredElement('dropBall'),
      dropBox: requiredElement('dropBox'),
      clearObjects: requiredElement('clearObjs'),
      lowGravity: requiredElement('lowGrav'),
      status: requiredElement('status'),
    };
  }

  get canvas() {
    return this.#elements.canvas;
  }

  get captureMode() {
    return this.#elements.mode.value;
  }

  get deviceId() {
    return this.#elements.device.value;
  }

  get gain() {
    return Number(this.#elements.gain.value);
  }

  bindHandlers(handlers) {
    if (this.#handlersBound) return;
    this.#handlersBound = true;

    const {
      listen,
      mode,
      device,
      gain,
      peak,
      physics,
      dropBall,
      dropBox,
      clearObjects,
      lowGravity,
    } = this.#elements;

    listen.addEventListener('click', handlers.onListen);
    mode.addEventListener('change', handlers.onModeChange);
    device.addEventListener('change', handlers.onDeviceChange);
    gain.addEventListener('input', () => handlers.onGainInput(this.gain));
    peak.addEventListener('click', handlers.onPeakToggle);
    physics.addEventListener('click', handlers.onPhysicsToggle);
    dropBall.addEventListener('click', handlers.onDropBall);
    dropBox.addEventListener('click', handlers.onDropBox);
    clearObjects.addEventListener('click', handlers.onClearObjects);
    lowGravity.addEventListener('click', handlers.onLowGravityToggle);

    this.canvas.addEventListener('click', event => {
      handlers.onCanvasPrimary(this.canvasLogicalCoordinates(event));
    });
    this.canvas.addEventListener('contextmenu', event => {
      event.preventDefault();
      handlers.onCanvasSecondary(this.canvasLogicalCoordinates(event));
    });
    this.#window.addEventListener('resize', handlers.onResize);
  }

  canvasLogicalCoordinates(event) {
    const { left, top, width, height } = this.canvas.getBoundingClientRect();
    const { logicalWidth, logicalHeight } = this.#layout;

    return {
      x: ((event.clientX - left) / width) * logicalWidth,
      y: ((event.clientY - top) / height) * logicalHeight,
    };
  }

  setStatus(kind, text) {
    this.#elements.status.className = kind;
    this.#elements.status.textContent = text;
  }

  setListening(running) {
    this.#elements.listen.textContent = running ? '⏹ Stop' : '▶ Listen';
  }

  setGainLabel(gain) {
    this.#elements.gainValue.textContent = `${Number(gain).toFixed(1)}x`;
  }

  setPeakHoldEnabled(enabled) {
    this.#elements.peak.classList.toggle('active', enabled);
  }

  setPhysicsEnabled(enabled) {
    this.#elements.physics.classList.toggle('active', enabled);
    this.#elements.physicsControls.classList.toggle('hidden', !enabled);
  }

  setLowGravityEnabled(enabled) {
    this.#elements.lowGravity.classList.toggle('active', enabled);
  }

  setPhysicsAvailable(available) {
    const { physics } = this.#elements;
    physics.disabled = !available;

    if (available) {
      physics.removeAttribute('title');
    } else {
      physics.title = 'matter-js failed to load (CDN unreachable)';
    }
  }

  setDeviceOptions(options, { disabled = false } = {}) {
    const select = this.#elements.device;
    select.disabled = disabled;

    const optionElements = options.map(({ label, value }) => {
      const option = this.#document.createElement('option');
      option.textContent = label;
      option.value = value;
      return option;
    });
    select.replaceChildren(...optionElements);
  }
}
