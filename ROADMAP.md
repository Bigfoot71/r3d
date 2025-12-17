# R3D Roadmap

## **v0.6**

* [ ] **Modular State System and On-Demand Resource Loading**
  Replace `r3d_state.h` with a modular system, split by responsibility, with internal on-demand loading management.
  For example, introduce a *shader cache* that loads all shaders lazily when they are first requested during rendering.
  Apply the same approach to render targets and any other applicable resources.
  Note: some "details" like `details/r3d_light.c` or `details/r3d_primitives.c` could also become modules with lazy loading.

* [ ] **Light Module Refactor and Optimization**
  Alongside the modularization of lights:

  * Simplify their update system, including the flag indicating whether shadows need updating.
  * Compute and store transformations (`matVP`) and AABBs whenever the light's spatial state changes, and retain them for future use.
  * Simplify AABB calculation for spotlights (slightly oversized bounding boxes are acceptable).
  * Revise directional light projection management: remove "scene bounds" from the environment, compute the projection relative to the camera, and use the light's `range` as the projection distance for directional shadows.
  * Manage the list of visible light indices (from camera) within the light module itself, instead of in `r3d_draw.c` (see light batch).
  * Add helper functions for testing light influence, e.g. checking if a bounding box is illuminated by a light.
  * Consider adding a real frustum culling by light for shadows, rather than the current crude tests.

* [ ] **Draw Call System Refactor**
  Migrate all content from `details/r3d_drawcall.h` into `r3d_draw.c`.

* [ ] **Unified Draw Call Storage and Index-Based Sorting**
  Replace multiple draw call arrays and in-place object sorting with:

  * a single array containing all draw calls
  * multiple index arrays per category
    Perform sorting and culling on the index arrays rather than directly on the draw call structures themselves.

## **v0.7**

* [ ] Adding the `R3D_InstanceBuffer` type to manage instances.
  See: https://github.com/Bigfoot71/r3d/discussions/121

* [ ] **Merge Scene Vertex Shaders**
  Merge all vertex shaders used by the `scene` module into a single unified vertex shader.

* [ ] **Environment API Refactor**
  Replace all environment-related functions with a single `R3D_Environment` structure, while keeping a global environment state.
  See: https://github.com/Bigfoot71/r3d/discussions/117

* [ ] **Skybox Revision and Reflection Probes**
  Revise the skybox system and add support for reflection probes, including probe blending.

## **v0.8**

* [ ] **Add Support for Custom Screen-Space Shaders (Post-Processing)**
  Allow custom shaders to be used in screen-space for post-processing effects.

* [ ] **Add Support for Custom Material Shaders**
  Allow custom shaders per material. This will likely require a different approach for deferred vs. forward rendering. Deferred mode will probably offer fewer possibilities than forward mode for custom material shaders.

* [ ] **Better logs**
  Add better logs for initialization, shutdown, loading operations, and failures.

## **v0.9**

* [ ] **Improve Shadow Map Management**
  Instead of using one shadow map per light, implement an internal batching system with a 2D texture array and `Sample2DArray` in shaders.
  *Note: This would remove per-light independent shadow map resolutions.*

* [ ] **Implement Cascaded Shadow Maps (CSM)**
  Add CSM support for directional lights.

* [ ] **Make a Wiki**
  Add wiki pages to the Github repository.

*Note: v0.9 features are still unsure and/or incomplete.*

## **Ideas (Not Planned Yet)**

* [ ] After implementing `Sampler2DArray` for shadow maps, explore using UBOs for lights, but also for environment and matrices/camera data.
* [ ] Provide an easy way to configure the number of lights in forward shaders (preferably runtime-configurable).
* [ ] Implement a system to save loaded skyboxes together with their generated irradiance and prefiltered textures for later reloading.
* [ ] Improve support for shadow/transparency interaction (e.g., colored shadows).
