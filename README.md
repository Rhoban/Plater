# Plater

![Plater](plater.jpg)

## [Demo video »](https://www.youtube.com/watch?v=WTK5fVQNPsI)

## [Video tutorial »](https://www.youtube.com/watch?v=_MwIhBSeAHM)

Plater is a 3D printer plates placer and optimizer. It takes a few STL files
and some parameters such as the plate dimension, part orientation and spacing,
and it tries to generates 3D model to print your parts with at least as possible
plates.

It uses a really simple configuration file that contains the list of parts with
their quantities and dimension. You can then build your STL plate(s) and release
it with your part, or simply release the `plater.conf` file and let people do
their own plates using their own settings.

Note that Plater will *not* choose the best orientation of a part for you, this is
up to the user because it depends on a lot of things.

Download binaries from [Release page](https://github.com/Rhoban/Plater/releases)

Alternate mirror:

* [Get Windows binaries (v1.1)](http://gregwar.com/plater/plater-win32-1.1.zip)
* [Get Windows command line tool (plater.exe, v1.0)](http://gregwar.com/plater/plater-win32-tool-1.0.zip)
* [Get Linux binaries (v1.0), Qt 4.8 and OpenGL required](http://gregwar.com/plater/plater-linux-1.0.zip)
* [Get OSX dmg (v1.0), thanks to Toby Tomkins](http://gregwar.com/plater/plater-osx-1.0.dmg)

# Using

## With the GUI

To make a plate, first load your STL files. Click "Add Part(s)" button and select
one or more `.stl` file.

This will open a wizard, you'll be able to select the orientation and the quantity
of the part.

Then, enter your plate dimension (in mm). You can also adjust the spacing of the
parts and the precision (internally, plater use a discrete representation of parts,
lower is better).

Hit "Run", this will generate you the STL file(s) corresponding to your plates.

All the settings you change can be saved to a `config.json` file using the
"File > config.json" menu. This file will be created in the same folder as
plater executable and will be loaded on startup.

## With the command line

Plater usage is:

```
plater [options] plater.conf
```

Where `plater.conf` is the name of your configuration file. If `-` is given as a
file name, the configuration will be read from standard input.

Here are the options:

* `-v`, increase the verbosity, this will output more things on `stderr` during
  the placing of the parts
* The size of the bedplate (in 2D, top view)
    * `-W width`, width of the plate, in mm (default `150`)
    * `-H height`, height of the plate, in mm (default `150`)
* `-j precision`, precision, in mm (default `0.5`)
* `-s spacing`, parts spacing, in mm (default `2`)
* `-d delta`, sets the spacing of the brute forcing (see below), default `2`mm
* `-r rotation`, sets the angle of the brute forcing, default `90`°
* `-p`, will output .ppm files instead of STLs
* `-m`, will output a single `.3mf` file containing every plate instead of one
  STL per plate, using the OrcaSlicer / BambuStudio multi-plate project layout.
  Each placed part becomes its own object named after the original part file, and
  every plate becomes a separate plate in the slicer (plate membership is stored
  in `Metadata/model_settings.config`, and a `Metadata/project_settings.config`
  declares the bed). Plates are arranged on the slicer's plate grid, so set
  `-W`/`-H` to match your printer's bed for the grid to line up. The output file
  name is derived from `-o` with any `%`-placeholder stripped (e.g. the default
  `plate_%03d` produces `plate.3mf`).

  **Open this file in OrcaSlicer with File > Open Project (or double-click it) —
  not File > Import.** Orca only restores the separate plates when opening it as
  a project; importing loads the geometry onto a single plate. The file declares
  a plain rectangular bed matching `-W`/`-H`, so re-select your printer preset
  before slicing if you need its specific settings.
* `-o pattern`, sets the pattern of output files, default is `plate_%03d`, this
  means that the first plate will be named plate_001.stl, the second plate_002.stl
  and so on.

## Fit search (ideal plate size)

Instead of placing on a single fixed plate size, Plater can search for the
smallest plate that still fits your parts in as few plates as possible. In this
mode `-W`/`-H` are the *physical maximum* plate size (your bed), and you give an
*ideal* (smallest preferred) size to start from:

* `-i ideal`, the ideal/smallest plate size, in mm. Setting this enables the fit
  search. Plater starts at this size and grows toward `-W`/`-H`.
* `-g step`, the growth increment, in mm (default `10`).
* `-N plates`, the number of plates to target first (default `1`).

The search grows the plate size from the ideal up to the bed size in `-g` steps,
and stops at the smallest size that fits the parts in `-N` plates (or fewer). If
even the full bed size cannot fit them in `-N` plates, it uses one more plate and
keeps the smallest size that achieves that fewest-plates result.

For example, with a 300x300 bed but a preferred 250x250 area:

    plater -W 300 -H 300 -i 250 -g 5 -m -o job project.conf

This tries 250, 255, ... up to 300 on a single plate; if nothing fits on one
plate it moves to two plates (again preferring the smallest size), and so on.
The chosen size is what feeds the 3MF plate grid, so combine it freely with `-m`.

# The plater.conf file

The configuration file looks like this:

```
# This is an example of plater.conf file
part.stl 1
other_part.stl 3 back
yet-another-part.stl 8 left
```

Each line begins with a part name, followed by the quantity, and optionally the side
that should be on the plate. The side can be `bottom` (default, you can also put
nothing), `front`, `top`, `back`, `left` or `right.

You can put comment lines beginning with `#`, it can be useful to add some advices
on how generate your plate or some copyrights.

The file should be described relatively to the `plater.conf` file.

If a filename contains a space (` `), you can escape it with the \ character (like 
`some\ plate.stl`).

# How does it works?

The problem of placing parts is quite hard and very likely NP-complete. Plater is
an heuristic based on greedy algorithm that use brute force.

Each part is first pixelized into a bitmap (each pixel is white for free space and 
black for used space), with a given precision. Then, this bitmap is dilatated with
a given spacing.

Then, the placer tries to put each part one by one on the plate, brute forcing
position to maximize a score based on a simple gravity-like property, which tend to
pack the parts. If it can't place the part, it try adding a new plate and so on.

It is running multiple times with multiple parameters, and the best solution, i.e the
one with the less plates is kept.

Note that the result can be bad in some limit cases, and it will not replace your
expert brain! However, it is useful to do the placing automatically and quickly.

# Building

## Plater

To build plater, go in the `plater/` directory and then use the cmake:

```
mkdir build
cd build
cmake ..
make
```

This will create for you the libplater and the plater command tool

## GUI

First, install Qt 4.8.0. Then, be sure you built plater in `plater/build/`.

### Using QtCreator

You can simply run QtCreator on `gui.pro` and build it.

### Using command line

Go in the `gui/` directory, and do:

```
mkdir build
cd build
qmake ..
make
sudo make install
```

This will create the `plater-gui` binary file.
