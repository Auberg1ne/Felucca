// SPDX-License-Identifier: GPL-3.0-only
// TEST ONLY: timing_test.py prepends this to a disposable worklet.js snapshot.
// Never imported/packaged by the application. No firmware memory/state export.
if (typeof window === 'undefined' && typeof globalThis.AudioWorkletProcessor === 'function' &&
    typeof globalThis.registerProcessor === 'function') {
  const register = globalThis.registerProcessor;
  globalThis.registerProcessor = (name, Base) => register.call(globalThis, name, class extends Base {
    constructor(options) {
      super(options);
      this.cgTiming = { callbacks: 0, activeCallbacks: 0, frames: 0, nonFinite: 0,
        peak: 0, minQuantum: Infinity, maxQuantum: 0, rate: sampleRate };
      this.port.addEventListener('message', ({ data: m }) => {
        if (m?.type === 'cg-timing-read' && Number.isSafeInteger(m.ticket) && m.ticket > 0)
          this.port.postMessage({ type: 'cg-timing-metrics', ticket: m.ticket, stats: { ...this.cgTiming } });
      });
    }
    process(inputs, outputs, parameters) {
      const stats = this.cgTiming, active = !!this.ex && this.renderEnabled && !this.loadFailed;
      const first = active && !stats.activeCallbacks;
      if (first && typeof console.timeStamp === 'function') console.timeStamp('CG.first-active');
      const result = super.process(inputs, outputs, parameters), out = outputs[0], n = out?.[0]?.length || 0;
      stats.callbacks++;
      stats.minQuantum = Math.min(stats.minQuantum, n);
      stats.maxQuantum = Math.max(stats.maxQuantum, n);
      if (active) {
        stats.activeCallbacks++; stats.frames += n;
        for (let c = 0; c < out.length; c++) for (let i = 0; i < n; i++) {
          const x = out[c][i];
          if (!Number.isFinite(x)) stats.nonFinite++;
          else stats.peak = Math.max(stats.peak, Math.abs(x));
        }
      }
      return result;
    }
  });
}
