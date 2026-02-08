# R3D Roadmap

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
