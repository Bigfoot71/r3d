# R3D Roadmap

## **v0.7**

* [x] **Add support for sRGB textures**
  Provide a parameter (enabled by default) for loading textures,
  albedo being the most important, in sRGB.

* [x] **Adding the `R3D_InstanceBuffer` type to manage instances.**
  See: https://github.com/Bigfoot71/r3d/discussions/121

* [x] **Merge Scene Vertex Shaders**
  Merge all vertex shaders used by the `scene` module into a single unified vertex shader.

* [x] **Skybox Revision and Reflection Probes**
  Revise the skybox system and add support for reflection probes, including probe blending.

* [x] **Skybox Generation Support**
  Add support for procedural skybox generation.

* [x] **Improve Shadow Map Management**
  Instead of using one shadow map per light, implement an internal batching system with a 2D texture array and `Sample2DArray` in shaders.
  *Note: This would remove per-light independent shadow map resolutions.*

## **v0.8**

* [ ] **Add Support for Custom Screen-Space Shaders (Post-Processing)**
  Allow custom shaders to be used in screen-space for post-processing effects.

* [ ] **Add Support for Custom Material Shaders**
  Allow custom shaders per material. This will likely require a different approach for deferred vs. forward rendering. Deferred mode will probably offer fewer possibilities than forward mode for custom material shaders.

* [ ] **Better logs**
  Add better logs for initialization, shutdown, loading operations, and failures.

## **Ideas (Not Planned Yet)**

* [ ] Implement a system to save loaded skyboxes together with their generated irradiance and prefiltered textures for later reloading.
* [ ] Improve support for shadow/transparency interaction (e.g., colored shadows).
* [ ] Implement Cascaded Shadow Maps (or alternative) for directional lights.
* [ ] Make wiki pages for the repo, consider it for the release.
