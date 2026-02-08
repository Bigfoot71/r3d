# R3D Roadmap

## **v0.8**

* [x] **Offer more flexibility for model loading**
  A flag system could be added to indicate additional operations during import via Assimp,
  or to specify whether to keep a copy of the mesh data in RAM.

* [x] **Add basic functions for collisions / rudimentary physics**
  Functions for collision checks / ray casting would be nice, like those provided in base raylib,
  possibly with some additional functions for sweeps and similar operations.

* [x] **Add Support for Custom Screen-Space Shaders (Post-Processing)**
  Allow custom shaders to be used in screen-space for post-processing effects.

* [x] **Add Support for Custom Material Shaders**
  Allow custom shaders per material. This will likely require a different approach for deferred vs. forward rendering. Deferred mode will probably offer fewer possibilities than forward mode for custom material shaders.

* [x] **Better logs**
  Add better logs for initialization, shutdown, loading operations, and failures.

## **v0.9**

* [ ] **Explore alternative SSGI approaches**
  The current SSIL-VB used as a cheap SSGI solution is far from ideal and difficult to tune.
  Investigating more stable and production-ready SSGI techniques would be beneficial.

* [ ] **Add more anti-aliasing options**
  Provide higher-quality anti-aliasing solutions in addition to FXAA.
  SMAA appears to be the most suitable next step. Techniques such as TAA currently require
  deeper architectural changes...

* [ ] **Material loading/import helpers**
  Similar to existing skeleton and animation loading utilities, it would be useful to provide
  helper functions to import materials from files when needed.

## **Ideas (Not Planned Yet)**

* [ ] Investigate the integration of a velocity buffer and frame history, mainly for TAA and motion blur. The current begin/end rendering model makes this non-trivial.
* [ ] Improve support for shadow/transparency interaction (e.g., colored shadows).
* [ ] Implement Cascaded Shadow Maps (or alternative) for directional lights.
* [ ] Make wiki pages for the repo, consider it for the release.
