The goal of this effort is to develop an isolated model of SVS processor
for the project of the full scale SVS simulator.

The CPU implementation was borrowed from the SIMH project: https://github.com/besm6/simh

# Build

On Mac:
```
$ brew install cmocka
$ make
```

On Ubuntu:
```
$ sudo apt install libcmocka-dev
$ make
```

Run tests:
```
$ make test
```
