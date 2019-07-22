let selectRomBtn = document.querySelector('#rom-select');
let realRomInput = document.querySelector('#real-input');
let romHeading = document.querySelector('#romHeading');

selectRomBtn.addEventListener('click', () => {
  realRomInput.click();
});

realRomInput.addEventListener('change', (event) => {
  const name = realRomInput.value.split(/\\|\//).pop();
  const truncated = (name.length > 13)
    ? name.slice(0, 11) + '...'
    : name;

  selectRomBtn.textContent = truncated;
  romHeading.textContent = name.substring(0, name.indexOf('.'));

  console.log(event.target.files[0].name);
  loadFile(event.target, 'rom.gb');
});

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
    // if (!FS.analyzePath('ram.sav').exists) return;
    // document.getElementById('ram').labels[0].innerHTML = lastFilename + '.sav';
  });
};

const loadFile = (input, filename) => {
  if (input.files.length == 0) return;
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
    Module.ccall('load', null, ['string'], ['rom.gb']);
  };
  lastFilename = file.name.replace(/\.[^/.]+$/, '');

  // storing name of file as filename.txt
  FS.writeFile('filename.txt', lastFilename);
};

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

// Pause play button
let pauseMenu = document.querySelector('.pauseMenu');
let pausePlayButton = document.querySelector('.pauseMenu button');
let currentState = document.querySelector('#state');
pauseMenu.addEventListener('click', () => {
  pausePlayButton.classList.toggle('paused');
  if (currentState.textContent === 'PLAY') {
    currentState.textContent = 'PAUSE';
    Module.ccall('togglePause', null, null, null);
  } else {
    currentState.textContent = 'PLAY'
    Module.ccall('togglePause', null, null, null);
  }
});