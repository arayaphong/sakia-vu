export class AppView {
  constructor({ document, window, layout }) {
    this.document = document;
    this.window = window;
    this.layout = layout;

    const requiredElement = id => {
      const element = document.getElementById(id);
      if (!element) throw new Error(`Missing required element: #${id}`);
      return element;
    };

    this.elements = {
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

    this.handlersBound = false;
  }

  get canvas() {
    return this.elements.canvas;
  }

  get captureMode() {
    return this.elements.mode.value;
  }

  get deviceId() {
    return this.elements.device.value;
  }

  get gain() {
    return Number(this.elements.gain.value);
  }

  bindHandlers(handlers) {
    if (this.handlersBound) return;
    this.handlersBound = true;

    this.elements.listen.addEventListener('click', handlers.onListen);
    this.elements.mode.addEventListener('change', handlers.onModeChange);
    this.elements.device.addEventListener('change', handlers.onDeviceChange);
    this.elements.gain.addEventListener('input', () => handlers.onGainInput(this.gain));
    this.elements.peak.addEventListener('click', handlers.onPeakToggle);
    this.elements.physics.addEventListener('click', handlers.onPhysicsToggle);
    this.elements.dropBall.addEventListener('click', handlers.onDropBall);
    this.elements.dropBox.addEventListener('click', handlers.onDropBox);
    this.elements.clearObjects.addEventListener('click', handlers.onClearObjects);
    this.elements.lowGravity.addEventListener('click', handlers.onLowGravityToggle);

    this.canvas.addEventListener('click', event => {
      handlers.onCanvasPrimary(this.canvasLogicalCoordinates(event));
    });
    this.canvas.addEventListener('contextmenu', event => {
      event.preventDefault();
      handlers.onCanvasSecondary(this.canvasLogicalCoordinates(event));
    });
    this.window.addEventListener('resize', handlers.onResize);
  }

  canvasLogicalCoordinates(event) {
    const bounds = this.canvas.getBoundingClientRect();
    return {
      x: (event.clientX - bounds.left) / bounds.width * this.layout.logicalWidth,
      y: (event.clientY - bounds.top) / bounds.height * this.layout.logicalHeight,
    };
  }

  setStatus(kind, text) {
    this.elements.status.className = kind;
    this.elements.status.textContent = text;
  }

  setListening(running) {
    this.elements.listen.textContent = running ? '⏹ Stop' : '▶ Listen';
  }

  setGainLabel(gain) {
    this.elements.gainValue.textContent = Number(gain).toFixed(1) + 'x';
  }

  setPeakHoldEnabled(enabled) {
    this.elements.peak.classList.toggle('active', enabled);
  }

  setPhysicsEnabled(enabled) {
    this.elements.physics.classList.toggle('active', enabled);
    this.elements.physicsControls.classList.toggle('hidden', !enabled);
  }

  setLowGravityEnabled(enabled) {
    this.elements.lowGravity.classList.toggle('active', enabled);
  }

  setPhysicsAvailable(available) {
    this.elements.physics.disabled = !available;
    if (available) {
      this.elements.physics.removeAttribute('title');
    } else {
      this.elements.physics.title = 'matter-js failed to load (CDN unreachable)';
    }
  }

  setDeviceOptions(options, { disabled = false } = {}) {
    const select = this.elements.device;
    select.disabled = disabled;
    select.innerHTML = '';
    for (const { label, value } of options) {
      const option = this.document.createElement('option');
      option.textContent = label;
      option.value = value;
      select.append(option);
    }
  }
}
