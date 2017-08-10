## Build Requirements
- Libraries
    * [Simple and Fast Multimedia Library](http://sfml-dev.org/)
    * [JSON for modern C++](https://github.com/nlohmann/json)
    * [Fastest Fourier Transform in the West](http://www.fftw.org/index.html)
- Environment Variables
    * SFML_HOME - must point to the home directory of your built SFML library, contining the bin, lib, and include directories
    * JSON_HOME - home directory of nlohmann/json installation, which only contains an include directory with a single hpp file
    * FFTW_HOME - directory containing fftw3.h, as well as fftw's lib & dll files, all in the same directory.  It's the way they deploy their windows binaries, apparently.
- Visual Studio installed :^)

## Run Requirements
- Environment Variables
    * `$(Path)` must include `$(SFML_HOME)\bin;$(FFTW_HOME)`
- You must pass the map json file to be loaded into the simulator via the -map option. Example: `-map assets/map.json`

> Note: you can set these runtime requirements up locally in Visual Studio by going into `Project` -> `sfml-wave-sim Properties...` -> `Debugging`

## Controls
- Keyboard
    * F1: toggle voxel grid display
    * F2: toggle partition outline display
- Mouse
    * Left Click: generates pressure inside partitions
    * Right Click: hold & move mouse to pan
    * Scroll Wheel: zoom in/out
    * Middle Click: reset zoom
