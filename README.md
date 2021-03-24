# Line following car for testing indoor tracking
It will follow a line (or you can drive it manually), it has got a long pole which a transmitter for indoor tracking can be placed on. 

## How it works
Either acts as an AP or station and connects to an AP. This is configured in the menuconfig. Connect to the AP and then go to `df-car.local` in the browser and you will get an interface for driving the car. Another option is to drive using a standard 6 channel RC transmitter.

By changing the switches it's possible to set the car in Line Follow mode, which will make the car follow a line using two IR sensors placed below the car. Might need to tweak values in `line_follower.c` depending on the line color (light or dark).

### Compiling
Follow instruction on [https://github.com/espressif/esp-idf](https://github.com/espressif/esp-idf) to set up the esp-idf, then just run `idf.py build` or use the [VSCode extension](https://github.com/espressif/vscode-esp-idf-extension). Tested with esp-idf 4.1.

## Images
<img src="/.github/car2.jpg"/>
<p float="left">
  <img src="/.github/car1.jpg" width="200"/>
</p>

