let amenLoaded = false;

async function playAmen(sonic) {
  if (!amenLoaded) {
    console.log('[Demo] Loading amen break sample and synthdef...');
    await sonic.loadSynthDefs(['sonic-pi-basic_stereo_player']);
    await sonic.send('/b_allocRead', 0, 'loop_amen.flac');
    amenLoaded = true;
    console.log('[Demo] Amen break ready');
  }

  console.log('[Demo] Playing amen break');
  sonic.send('/s_new', 'sonic-pi-basic_stereo_player', -1, 0, 0, 'buf', 0);
}

export { playAmen };
