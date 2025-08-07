# RAI bare code

This repo contains core sources related to Robotics & AI. Users are
not recommended to use this repo alone.  Please have a look at example
projects that use this bare code as a submodule and expose and explain
some subset of functionalities. Esp. the
[robotic python lib](https://github.com/MarcToussaint/robotic/), which
now co-installs C++ headers and a compiled shared lib.

'bare code' means that this repo contains only sources, a minimal
Ubuntu-specific build system, and development tests. It is mostly used
as submodule in other integrated projects, with their own
out-of-source build system.

## Brief history

Parts of the code have there origin at around 2004 (Edinburgh). The
code grew over the years to a large repo with many projects from all
lab members, but a somewhat consistent scope of code shared between
projects. This repo includes a selection of the code shared between
projects and contains a set of representations and methods for
Robotics, ML and AI. As the functionality is diverse I don't even try
to explain.

## Repos wrapping rai:

* [Robotic Python Lib](https://pypi.org/project/robotic/)
* [BotOp](https://github.com/MarcToussaint/botop)

## Documentation

The there is no proper documentation of the full rai code. I recommend starting with 
* The [robotic python lib documentation](https://marctoussaint.github.io/robotic/), which explains core features (but certainly not the underlying code base),
* With Doxygen (see [rai-maintenence help](https://github.com/MarcToussaint/rai-maintenance/tree/master/help)) you can get an API.
* The [Wiki page](../../wiki) contains an older introduction to KOMO. There is also an older KOMO tech report on arxiv: <https://arxiv.org/abs/1407.0414>
* Eventually, the [test main.cpp files](test/) help really understanding the use of the C++ code base.

## Building

```sh
git clone git@github.com:squarra/rai.git
cmake -B build
cmake --build build -j 6
```

## Running tests

- gnuplot is required (sudo dnf install gnuplot)
- rai-robotModels is required next to where you cloned this repo
- shapenet data is going to be required for the shapenet tests
- there are some hardcoded paths to other repos which I could not find online

```sh
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j 6
cd build
ctest # run all tests
ctest -R test_name # run a specific test
```

On Fedora I have to change my XDG_SESSION_TYPE

```sh
XDG_SESSION_TYPE=x11 ctest
```

## Building for python

```sh
cmake -B build -DUSE_PYBIND=ON
cmake --build build -j 6
cd build
stubgen -m _robotic -o . --include-docstrings
```
