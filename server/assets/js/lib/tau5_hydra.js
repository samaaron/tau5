import Hydra from "../../vendor/hydra-synth.js";

class Tau5Hydra {
  #hydra;

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


// Hydra example tropical juice
// by Ritchse
// instagram.com/ritchse
// https://hydra.ojack.xyz/?sketch_id=ritchse_2
hydra.voronoi(2,0.3,0.2).shift(0.5)
.modulatePixelate(hydra.voronoi(4,0.2),32,2)
.scale(()=>1+(Math.sin(hydra.time*2.5)*0.05))
.diff(hydra.voronoi(3).shift(0.6))
.diff(hydra.osc(2,0.15,1.1).rotate())
.brightness(0.1).contrast(1.2).saturate(1.2)
	.out()
hydra.speed = 0.8
    this.#hydra = hydra;
  }
}

export default Tau5Hydra;
