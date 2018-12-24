## objzero

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

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
