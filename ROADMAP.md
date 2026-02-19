# R3D Roadmap

## **v0.9**

* [x] **Refactor internal `r3d_draw` module**
  Centralize all drawing-related functionality and drawable objects, including VAO management, which is currently scattered.

* [x] **Redesign internal shader macro system**
  ~~Rethink the current macro-based approach for managing shaders to improve clarity and maintainability.~~
  The subject involves much more complexity than expected, it will probably remain as it is, the naming has at least been made clearer.

* [x] **Explore alternative SSGI approaches**
  The current SSIL-VB used as a cheap SSGI solution is far from ideal and difficult to tune.
  Investigating more stable and production-ready SSGI techniques would be beneficial.

* [x] **Add more anti-aliasing options**
  Provide higher-quality anti-aliasing solutions in addition to FXAA.
  SMAA appears to be the most suitable next step. Techniques such as TAA currently require
  deeper architectural changes...

* [x] **Material loading/import helpers**
  Similar to existing skeleton and animation loading utilities, it would be useful to provide
  helper functions to import materials from files when needed.

* [ ] **Review skybox generation**
  Improve parameter structure and naming, add an optional debanding setting, and support shaders for skyboxes.

* [ ] **Consider `R3D_Camera` type (maybe)**
  Think about introducing a dedicated camera type that handles `cullMask` per camera (instead of using global state) and manages `near/far` parameters directly, removing the last dependency on `rlgl`.

## **Ideas (Not Planned Yet)**

* [ ] Investigate the integration of a velocity buffer and frame history, mainly for TAA and motion blur. The current begin/end rendering model makes this non-trivial.
* [ ] Improve support for shadow/transparency interaction (e.g., colored shadows).
* [ ] Implement Cascaded Shadow Maps (or alternative) for directional lights.
* [ ] Make wiki pages for the repo, consider it for the release.
