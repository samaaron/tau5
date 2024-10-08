import Hydra from "../../vendor/hydra-synth.js";

class Tau5Hydra {
  #hydra;
  #hydraFunctions;
  #time = 0;

  constructor(container) {
    const hydra = new Hydra({
      makeGlobal: false,
      detectAudio: false,
      canvas: container,
    }).synth;

    hydra.setResolution(container.clientWidth, container.clientHeight);
    container.addEventListener(
      "resize",
      function () {
        hydra.setResolution(container.clientWidth, container.clientHeight);
      },
      true
    );

    // Destructure Hydra functions and output buffers, store them for reuse
    this.#hydraFunctions = {
      get speed() {
        return hydra.speed;
      },
      set speed(value) {
        hydra.speed = value;
      },
      get bpm() {
        return hydra.bpm;
      },
      set bpm(value) {
        hydra.bpm = value;
      },
      // Source
      noise: hydra.noise,
      voronoi: hydra.voronoi,
      osc: hydra.osc,
      shape: hydra.shape,
      gradient: hydra.gradient,
      src: hydra.src,
      solid: hydra.solid,
      prev: hydra.prev,

      // Geometry
      rotate: hydra.rotate,
      scale: hydra.scale,
      pixelate: hydra.pixelate,
      repeat: hydra.repeat,
      repeatX: hydra.repeatX,
      repeatY: hydra.repeatY,
      kaleid: hydra.kaleid,
      scroll: hydra.scroll,
      scrollX: hydra.scrollX,
      scrollY: hydra.scrollY,

      // Color
      posterize: hydra.posterize,
      shift: hydra.shift,
      invert: hydra.invert,
      contrast: hydra.contrast,
      brightness: hydra.brightness,
      luma: hydra.luma,
      thresh: hydra.thresh,
      color: hydra.color,
      saturate: hydra.saturate,
      hue: hydra.hue,
      colorama: hydra.colorama,
      sum: hydra.sum,
      rgba: hydra.rgba,

      // Blend
      add: hydra.add,
      sub: hydra.sub,
      layer: hydra.layer,
      blend: hydra.blend,
      mult: hydra.mult,
      diff: hydra.diff,
      mask: hydra.mask,

      // Modulate
      modulateRepeat: hydra.modulateRepeat,
      modulateRepeatX: hydra.modulateRepeatX,
      modulateRepeatY: hydra.modulateRepeatY,
      modulateKaleid: hydra.modulateKaleid,
      modulateScrollX: hydra.modulateScrollX,
      modulateScrollY: hydra.modulateScrollY,
      modulate: hydra.modulate,
      modulateScale: hydra.modulateScale,
      modulatePixelate: hydra.modulatePixelate,
      modulateRotate: hydra.modulateRotate,
      modulateHue: hydra.modulateHue,

      // External Sources
      initCam: hydra.initCam,
      initImage: hydra.initImage,
      initVideo: hydra.initVideo,
      init: hydra.init,
      initStream: hydra.initStream,
      initScreen: hydra.initScreen,

      // Synth Settings
      render: hydra.render,
      update: hydra.update,
      setResolution: hydra.setResolution,
      hush: hydra.hush,
      setFunction: hydra.setFunction,

      width: hydra.width,
      height: hydra.height,
      time: () => this.#time, // Use a function to access the updated time
      mouse: hydra.mouse,

      // Array
      fast: hydra.fast,
      smooth: hydra.smooth,
      ease: hydra.ease,
      offset: hydra.offset,
      fit: hydra.fit,

      // Audio
      fft: hydra.fft,
      setSmooth: hydra.setSmooth,
      setCut: hydra.setCut,
      offsetBins: hydra.offsetBins,
      setScale: hydra.setScale,
      hide: hydra.hide,
      show: hydra.show,

      // Output Buffers
      o0: hydra.o0,
      o1: hydra.o1,
      o2: hydra.o2,
      o3: hydra.o3,

      s0: hydra.s0,
      s1: hydra.s1,
      s2: hydra.s2,
      s3: hydra.s3,
    };

    this.#hydra = hydra;
    this.startRenderLoop();
    this.injectGlobalSketch(`


// licensed with CC BY-NC-SA 4.0 https://creativecommons.org/licenses/by-nc-sa/4.0/

//clouds of passage
//by Nesso
//www.nesso.xyz

shape([4,5,6].fast(0.1).smooth(1),0.000001,[0.2,0.7].smooth(1))
.color(0.2,0.4,0.3)
.scrollX(()=>Math.sin(time*0.27))
.add(
  shape([4,5,6].fast(0.1).smooth(1),0.000001,[0.2,0.7,0.5,0.3].smooth(1))
  .color(0.6,0.2,0.5)
  .scrollY(0.35)
  .scrollX(()=>Math.sin(time*0.33)))
.add(
  shape([4,5,6].fast(0.1).smooth(1),0.000001,[0.2,0.7,0.3].smooth(1))
  .color(0.2,0.4,0.6)
  .scrollY(-0.35)
  .scrollX(()=>Math.sin(time*0.41)*-1))
.add(
      src(o0).shift(0.001,0.01,0.001)
      .scrollX([0.05,-0.05].fast(0.1).smooth(1))
      .scale([1.05,0.9].fast(0.3).smooth(1),[1.05,0.9,1].fast(0.29).smooth(1))
      ,0.85)
.modulate(voronoi(10,2,2))
.out()

speed = 1


`);
  }

  startRenderLoop() {
    const update = () => {
      this.#time += 0.01;

      requestAnimationFrame(update);
    };
    update();
  }

  injectGlobalSketch(sketchCode) {
    // Replace occurrences of 'time' with 'this.time()' in the sketch code
    const modifiedSketchCode = sketchCode
      .replace(
        /\([ \t\r\n]*{[ \t\r\n]*time[ \t\r\n]*}[ \t\r\n]*\)/g,
        "({___time___})"
      )
      .replace(/\btime\b/g, "this.time()")
      .replace(/\bspeed\b/g, "this.speed")
      .replace(/\bbpm\b/g, "this.bpm");

    // Use the destructured functions and buffers from initialization
    const sketchFunction = new Function(`
      const {
        noise, voronoi, osc, shape, gradient, src, solid, prev,
        rotate, scale, pixelate, repeat, repeatX, repeatY, kaleid, scroll, scrollX, scrollY,
        posterize, shift, invert, contrast, brightness, luma, thresh, color, saturate, hue, colorama, sum, rgba,
        add, sub, layer, blend, mult, diff, mask,
        modulateRepeat, modulateRepeatX, modulateRepeatY, modulateKaleid, modulateScrollX, modulateScrollY, modulate, modulateScale, modulatePixelate, modulateRotate, modulateHue,
        initCam, initImage, initVideo, init, initStream, initScreen,
        render, update, setResolution, hush, setFunction, speed, bpm, width, height, time, mouse,
        fast, smooth, ease, offset, fit,
        fft, setSmooth, setCut, offsetBins, setScale, hide, show,
        o0, o1, o2, o3, s0, s1, s2, s3
      } = this;
      ${modifiedSketchCode}
    `);

    // Execute the sketch in the context of the non-global hydra instance
    sketchFunction.call(this.#hydraFunctions);
  }
}

export default Tau5Hydra;
