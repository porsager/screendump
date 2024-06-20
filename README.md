# 📺 Screendump

Take a screenshot of all displays or a specific one and receive it as a jpg buffer.

Windows, MacOS and Linux (X11) supported

# Usage

```bash
npm i screendump
```

```js
import screendump from 'screendump'

// Saves a screenshot of all screens to disk
fs.writeFileSync('wat.jpg', await capture())
```

## Options

The screendump function takes an object with options:
```js
await screendump({
  display: -1,  // Index of display to capture. Default -1 means all displays,
  scale: 1,     // A scale multiplier for the output image size,
  quality: 75   // The JPG quality
})
```

## Build requirements

Linux build requires `libx11-dev`, `libxinerama-dev` and `libjpeg-turbo8-dev`.

Eg.
```bash
sudo apt install libx11-dev libxinerama-devlibjpeg-turbo8-dev
```
