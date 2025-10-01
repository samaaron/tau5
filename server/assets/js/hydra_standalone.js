// Standalone Hydra bundle for iframe usage
// This creates a separate bundle that exposes Hydra globally

import Hydra from "../vendor/hydra-synth.js";

// Make Hydra available globally for the iframe context
window.Hydra = Hydra;

// Log that Hydra is loaded for debugging
console.log("Hydra standalone loaded and available as window.Hydra");