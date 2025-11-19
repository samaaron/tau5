import { SuperSonic } from "../../vendor/supersonic/dist/supersonic.js";

// Global SuperSonic instance - initialized once when app loads
let globalSuperSonic = null;

async function initSuperSonic() {
  if (!globalSuperSonic) {
    console.log('[SuperSonic] Initializing global engine...');
    globalSuperSonic = new SuperSonic({
      workerBaseURL: '/supersonic/dist/workers/',
      wasmBaseURL: '/supersonic/dist/wasm/',
      sampleBaseURL: '/supersonic/samples/',
      synthdefBaseURL: '/supersonic/synthdefs/'
    });

    try {
      await globalSuperSonic.init();
      console.log('[SuperSonic] Global engine ready');

      // Expose globally for easy access from console, Monaco, etc.
      window.supersonic = globalSuperSonic;
    } catch (error) {
      console.error('[SuperSonic] Initialization failed:', error);
      throw error;
    }
  }
  return globalSuperSonic;
}

export { initSuperSonic };
