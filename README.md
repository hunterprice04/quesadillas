# Quesadillas

A lightweight web-based platform for OSPRay renderer, built on top of the RASTY library.

Currently depends on:

- OSPRay
- Embree
- RASTY
- Pistache (https://github.com/seelabutk/pistache)

## Setup

### Automatic installation of the distributed version

You can use the distributed version that can be installed easily using the Tapestry project (https://github.com/seelabutk/tapestry).

### Building from source

- `git clone` this repo.
- Make a `resources` directory and clone rasty (https://github.com/seelabutk/rasty).
- Also clone the Pistache repo from SeelabUTK(https://github.com/seelabutk/pistache) in `resources`. Specifically, we've tested revision 9d5a4132a58e4685b2a01da1496447c2e8625717.
- Make a build directory in the root and run `cmake ../` inside it.
- Run ccmake . and set ospray_DIR to the install directory that contains the osprayConfig file (e.g. /home/username/ospray/install/lib/cmake/ospray-1.3.1). 
- Set embree_DIR to Embree's built directory
- Configure and make.

## Plugins

At the moment, you can use the Quesadillas HTTP server for running custom scripts and programs through the `extern` request. 
A request such as `/extern/foo/bar` runs the program `foo` in the `plugins` directory with arguments `bar`. The results are then routed back to the client. 

If the `plugins` directory doesn't exist, you can create it in your `build` directory. Make sure that your plugin programs are executable before using them. 

## Documentation 

You can find more information in the [Wiki](https://github.com/seelabutk/quesadillas/wiki).
