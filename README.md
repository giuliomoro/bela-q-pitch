# Using Q on Bela

This is a  program that uses the Q library from within a Bela project. It uses the Q `pitch_detector` and `envelope_follower` classes to track the pitch and envelope of an input signal and re-synthesise it with a square oscillator

## Pre-requisites

This program assumes that you have the Q library installed in /root/q on your Bela board. You can get it [here](git@github.com:cycfi/q.git). This was tested on bela image v0.5.0alpha2. You may be able to make it work on earlier versions by tweaking the compiler version and C++ standard in use as explained e.g.: [here](https://github.com/giuliomoro/Bela-Q/), but that is not supported.

## Build options

The compiler needs to know where to find the header files for the Q library. This is achieved by passing the following options to `make`:

```
CPPFLAGS="-I/root/q/q_lib/include -I /root/q/infra/include -std=c++20"
```

These options are already saved in this project's `settings.json`, so if you run it from the IDE, it should build and run out of the box (assuming the pre-requisites are met). If you are building the program from the command-line by invoking `make`, you should do:

```
make CPPFLAGS="-I/root/q/q_lib/include -I /root/q/infra/include -std=c++20" PROJECT=bela-q-pitch run
```

If you are using one of the Bela scripts, you should pass these options after the `-m` flag (and be careful with the use of quotes). For instance

```
build_project.sh -m 'CPPFLAGS="-I/root/q/q_lib/include -I /root/q/infra/include -std=c++20"' bela-q-pitch
```

## Run options

The project should run with at least 128 samples per block or you may get underruns. This is due to the inner workings of Q's pitch detector. This was tested with 8 audio inputs. It's possible that for lower channel counts a smaller block size could be used, but be wary of the underlying uneven CPU load.

## Project content

As it is, the program will process live audio input. If you `#define INPUT_PLAYBACK` at the top of the `render.cpp` file, it will process a multi-channel audio file instead. If you `#define OUTPUT_WRITE` at the top of the `render.cpp` file, the output will be saved to disk as well as being played back in real-time and the program will stop automatically.
The boilerplate code that provides these features comes from `examples/Multichannel/multichannel-player` and `examples/Audio/record-audio`
