## objzero

[![Appveyor CI Build Status](https://ci.appveyor.com/api/projects/status/github/jpcy/objzero?branch=master&svg=true)](https://ci.appveyor.com/project/jpcy/objzero) [![Travis CI Build Status](https://travis-ci.org/jpcy/objzero.svg?branch=master)](https://travis-ci.org/jpcy/objzero) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A Wavefront .obj file loader written in C (C99). The API is simple. The output data is ready for graphics APIs.

## Features
* Vertex attributes are interleaved and not indexed separately.
* Normals are generated if missing. Obeys smoothing groups.
* Faces are triangulated.
* Per-object faces are batched by material into meshes.

## TODO
* More material parsing.

## Links
[tinyobjloader](https://github.com/syoyo/tinyobjloader)

[Meshes - McGuire Computer Graphics Archive](https://casual-effects.com/data)
