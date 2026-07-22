export class AppController {
  constructor({
    view,
    audioCapture,
    analyzer,
    physicsWorld,
    renderer,
    requestAnimationFrame,
    initialSampleRate = 48000,
  }) {
    this.view = view;
    this.audioCapture = audioCapture;
    this.analyzer = analyzer;
    this.physicsWorld = physicsWorld;
    this.renderer = renderer;
    this.requestAnimationFrame = requestAnimationFrame;
    this.initialSampleRate = initialSampleRate;

    this.peakHoldEnabled = true;
    this.physicsEnabled = false;
    this.lowGravityEnabled = false;
    this.lastFrameMs = 0;
    this.started = false;
    this.boundFrame = nowMs => this.frame(nowMs);
  }

  start() {
    if (this.started) return;
    this.started = true;

    this.view.bindHandlers({
      onListen: () => this.toggleCapture(),
      onModeChange: () => this.handleModeChange(),
      onDeviceChange: () => this.handleDeviceChange(),
      onGainInput: gain => this.view.setGainLabel(gain),
      onPeakToggle: () => this.togglePeakHold(),
      onPhysicsToggle: () => this.togglePhysics(),
      onDropBall: () => this.physicsWorld.spawnBall(-1, -1),
      onDropBox: () => this.physicsWorld.spawnBox(-1, -1),
      onClearObjects: () => this.physicsWorld.clear(),
      onLowGravityToggle: () => this.toggleLowGravity(),
      onCanvasPrimary: position => this.spawnFromCanvas('ball', position),
      onCanvasSecondary: position => this.spawnFromCanvas('box', position),
      onResize: () => this.renderer.resize(),
    });

    this.view.setListening(false);
    this.view.setStatus('stopped', '● STOPPED');
    this.view.setGainLabel(this.view.gain);
    this.view.setPeakHoldEnabled(this.peakHoldEnabled);
    this.view.setPhysicsEnabled(this.physicsEnabled);
    this.view.setLowGravityEnabled(this.lowGravityEnabled);
    this.view.setPhysicsAvailable(this.physicsWorld.available);

    this.renderer.resize();
    this.analyzer.buildBandEdges(this.initialSampleRate);
    void this.refreshDeviceList();
    this.requestAnimationFrame(this.boundFrame);
  }

  toggleCapture() {
    if (this.audioCapture.running) this.stopCapture();
    else void this.startCapture();
  }

  async startCapture() {
    try {
      const { analyserNode, sampleRate } = await this.audioCapture.start({
        mode: this.view.captureMode,
        deviceId: this.view.deviceId,
        onEnded: () => {
          if (this.audioCapture.running) this.stopCapture();
        },
      });
      this.analyzer.attach(analyserNode, sampleRate);
      this.view.setListening(true);
      this.view.setStatus('live', '● LIVE');
      void this.refreshDeviceList();
    } catch (error) {
      this.audioCapture.stop();
      this.view.setListening(false);
      const detail = error && error.message ? ` — ${error.message}` : '';
      this.view.setStatus('error', '▲ CAPTURE ERROR' + detail);
    }
  }

  stopCapture() {
    this.audioCapture.stop();
    this.analyzer.reset();
    this.view.setListening(false);
    this.view.setStatus('stopped', '● STOPPED');
  }

  async refreshDeviceList() {
    if (this.view.captureMode === 'tab') {
      this.view.setDeviceOptions([
        { label: 'Browser share picker', value: '' },
      ], { disabled: true });
      return;
    }

    const defaultOption = { label: 'Default input', value: '' };
    this.view.setDeviceOptions([defaultOption]);
    try {
      const devices = await this.audioCapture.listInputDevices();
      this.view.setDeviceOptions([
        defaultOption,
        ...devices.map(device => ({
          label: device.label || 'Microphone',
          value: device.deviceId,
        })),
      ]);
    } catch (_error) {
      // Device labels may require permission; the default input still works.
    }
  }

  handleModeChange() {
    void this.refreshDeviceList();
    if (this.audioCapture.running) void this.startCapture();
  }

  handleDeviceChange() {
    if (this.audioCapture.running) void this.startCapture();
  }

  togglePeakHold() {
    this.peakHoldEnabled = !this.peakHoldEnabled;
    this.view.setPeakHoldEnabled(this.peakHoldEnabled);
    if (!this.peakHoldEnabled) this.analyzer.resetPeaks();
  }

  togglePhysics() {
    if (!this.physicsWorld.available) return;
    this.physicsEnabled = !this.physicsEnabled;
    this.view.setPhysicsEnabled(this.physicsEnabled);
  }

  toggleLowGravity() {
    this.lowGravityEnabled = !this.lowGravityEnabled;
    this.view.setLowGravityEnabled(this.lowGravityEnabled);
    this.physicsWorld.setLowGravity(this.lowGravityEnabled);
  }

  spawnFromCanvas(kind, { x, y }) {
    if (!this.physicsEnabled) return;
    if (kind === 'ball') this.physicsWorld.spawnBall(x, y);
    else this.physicsWorld.spawnBox(x, y);
  }

  frame(nowMs) {
    const fixedDt = 1 / 60;
    const dt = this.lastFrameMs ? (nowMs - this.lastFrameMs) / 1000 : fixedDt;
    this.lastFrameMs = nowMs;

    if (this.audioCapture.running) {
      this.analyzer.update(this.view.gain, this.peakHoldEnabled);
    }

    const meterState = this.analyzer.frameState(this.peakHoldEnabled);
    let physicsObjects = [];
    if (this.physicsEnabled) {
      this.physicsWorld.step(dt, meterState);
      physicsObjects = this.physicsWorld.snapshot();
    }

    this.renderer.render(meterState, physicsObjects);
    this.requestAnimationFrame(this.boundFrame);
  }
}
