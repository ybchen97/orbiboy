let selectRomBtn = document.querySelector('#rom-select');
let realRomInput = document.querySelector('#real-input');
let romHeading = document.querySelector('#romHeading');

selectRomBtn.addEventListener('click', () => {
  realRomInput.click();
});

realRomInput.addEventListener('change', (event) => {
  const name = realRomInput.value.split(/\\|\//).pop();
  const truncated = name.length > 15 
    ? name.slice(0, 14) + '...' 
    : name;
  
  selectRomBtn.textContent = truncated;

  romHeading.textContent = "Now playing " + name;

  console.log(event.target.files[0].name);
  loadFile(event.target, 'rom.gb');
  Module.ccall('load', null, ['string'], ['rom.gb']);
});

// Set up file system
const setupFS = () => {
  // load files from indexedDB
  FS.mkdir('/rom');
  FS.mount(IDBFS, {}, '/rom');
  FS.syncfs(true, (err) => {
    FS.currentPath = '/rom';
    if (!FS.analyzePath('filename.txt').exists) {
      console.log('no roms found!');
      return;
    }
    lastFilename = FS.readFile('filename.txt', {encoding: 'utf8'});
    // wtf
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
  fr.onload = () => {
    const data = new Uint8Array(fr.result);
    FS.writeFile(filename, data);
  };
  document.querySelector('#rom-select').textContent = file.name;
  lastFilename = file.name.replace(/\.[^/.]+$/, '');
  FS.writeFile('filename.txt', lastFilename);
};

var Module = {
  canvas: () => document.querySelector('#canvas'),
  onRuntimeInitialized: setupFS
}