// Pause play button
let pauseMenu = document.querySelector('.pauseMenu');
let pausePlayButton = document.querySelector('.pauseMenu button');
let currentState = document.querySelector('#state');

// Rom file input
let selectRomBtn = document.querySelector('#rom-select');
let romInput = document.querySelector('#rom-input');

// Save file input
let selectSaveBtn = document.querySelector('#save-select');
let saveInput = document.querySelector('#save-input');

// Load button
let loadGameBtn = document.querySelector('#load-game');
// Download save button
let downloadBtn = document.querySelector('#download');

let romHeading = document.querySelector('#romHeading');

let lastFilename = '';
// Set up file system
const setupFS = () => {
  // load files that were stored in indexedDB from previous sessions
  FS.mkdir('/rom');
  FS.mount(IDBFS, {}, '/rom');
  FS.syncfs(true, (err) => {
    FS.currentPath = '/rom';
    if (!FS.analyzePath('filename.txt').exists) {
      console.log('no roms found!');
      return;
    }

    // Reading name of the previously used file from filename.txt
    lastFilename = FS.readFile('filename.txt', { encoding: 'utf8' });
    document.querySelector('#rom-select').textContent = lastFilename + '.gb';
    if (!FS.analyzePath('savefile.sav').exists) return;
    document.querySelector('#save-select').textContent = lastFilename + '.sav';
  });
};

// Loading rom and save files
const loadFile = (input, filename) => {
  if (input.files.length == 0) return;
  
  // Dealing with persisted data
  // if (FS.analyzePath('ram.sav').exists) {
  //   FS.writeFile('ram.sav', '');
  //   document.getElementById('ram').labels[0].innerHTML = 'Select Save';
  // }

  const file = input.files[0];
  let fr = new FileReader();
  fr.readAsArrayBuffer(file);
  
  // Writing the file to system
  fr.onload = () => {
    const data = new Uint8Array(fr.result);
    FS.writeFile(filename, data);
  };
  lastFilename = file.name.replace(/\.[^/.]+$/, '');

  // storing name of file as filename.txt
  FS.writeFile('filename.txt', lastFilename);
};

// Load through Module
const load = () => {
  console.log("load called");
  // If game is not paused, pause before loading
  if (pausePlayButton.classList.contains('pause')) {
    // if button contains pause class, game is playing
    pauseMenu.click();
  }
  if (!FS.analyzePath('rom.gb').exists) return;
  Module.ccall('load', null, ['string'], ['rom.gb']);
  console.log("rom file exists");

  if (FS.analyzePath('savefile.sav').exists) {
    Module.ccall('loadState', null, ['string'], ['savefile.sav']);
    console.log("save file exists");
  }
  pauseMenu.click();
};

const save = () => {
  console.log("save called");
  // If game is not paused, pause before saving
  if (pausePlayButton.classList.contains('pause')) {
    // if buttton contains pause class, game is playing
    pauseMenu.click();
  }
  Module.ccall('saveState', null, ['string'], ['savefile']);
  // if (!FS.analyzePath('savefile.sav').exists) {
  //   console.log('save file does not exist');
  //   return;
  // }
  console.log("save file exists");
  const data = FS.readFile('savefile');
  const blob = new Blob([data.buffer], {type: 'application/octet-binary'});
  saveAs(blob, lastFilename + '.sav');
};


pauseMenu.addEventListener('click', () => {
  pausePlayButton.classList.toggle('pause');
  if (pausePlayButton.classList.contains('pause')) {
    // Game is currently running
    currentState.textContent = 'PAUSE';
    Module.ccall('togglePause', null, null, null);
  } else {
    // Game is currently paused
    currentState.textContent = 'PLAY'
    Module.ccall('togglePause', null, null, null);
  }
});

selectRomBtn.addEventListener('click', () => {
  if (pausePlayButton.classList.contains('pause')) {
    pauseMenu.click();
  }
  romInput.click();
});

romInput.addEventListener('change', (event) => {
  const name = romInput.value.split(/\\|\//).pop();
  const truncated = (name.length > 13)
    ? name.slice(0, 11) + '...'
    : name;

  // If user does not give a file input
  if (truncated.length === 0) {
    return;
  }
  selectRomBtn.textContent = truncated;
  romHeading.textContent = name.substring(0, name.indexOf('.'));

  console.log(event.target.files[0].name);
  loadFile(event.target, 'rom.gb');
});

selectSaveBtn.addEventListener('click', () => {
  if (pausePlayButton.classList.contains('pause')) {
    pauseMenu.click();
  }
  saveInput.click();
});

saveInput.addEventListener('change', (event) => {
  const name = saveInput.value.split(/\\|\//).pop();
  const truncated = (name.length > 13)
    ? name.slice(0, 11) + '...'
    : name;

  // If user does not give a file input
  if (truncated.length === 0) {
    return;
  }
  selectSaveBtn.textContent = truncated;

  console.log(event.target.files[0].name);
  loadFile(event.target, 'savefile.sav');
});

loadGameBtn.addEventListener('click', load);
downloadBtn.addEventListener('click', save);

var Module = {
  canvas: (function () {
    var canvas = document.getElementById('canvas');

    // As a default initial behavior, pop up an alert when webgl context is lost. To make your
    // application robust, you may want to override this behavior before shipping!
    // See http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
    canvas.addEventListener("webglcontextlost", function (e) { alert('WebGL context lost. You will need to reload the page.'); e.preventDefault(); }, false);

    return canvas;
  })(),
  onRuntimeInitialized: setupFS
};