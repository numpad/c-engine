# C-Engine

A nice and simple low-level engine for simple game development in C99.


## Building

The engine is being built using C99, OpenGL ES2, [SDL2](https://www.libsdl.org/) and a few other great libraries.
Supported Platforms are Linux and the Browser (WebAssembly) – Windows and native Android will be implemented eventually and should in theory work without too many changes.

```bash
# Linux:
$ make

# WebAssembly:
$ make CC=emcc
```

Afterwards, run the game using `$ ./cengine` or `$ emrun cengine.html`, depending on your platform.

To serve the game as a Progressive Webapp, build using `CC=emcc` and copy `src/web/pwa/service-worker.js` in the same directory as `cengine.html`. The directory `src/web/pwa/` needs to be accessible.


### Compiling the Server

```bash
$ make -f src/server/Makefile
```


