# R3D Roadmap

## **v0.5**

* [x] **Create Shader Include System**
  Implement an internal shader include system to reduce code duplication in built-in shaders. This could be integrated during the compilation phase, either in `glsl_minifier` or a dedicated pre-processing script.

* [x] **Simplify forward rendering**
  Forward rendering will now be limited to semi-transparent objects.
  This change simplifies r3d and drops OpenGL ES support.
  If you need OpenGL ES 3.2, I'm developing another project that already supports it.
  See: https://github.com/Bigfoot71/Nexium/

* [x] **Better Shadow Quality and Performance**
  Shadow rendering can currently be easily simplified and improved

* [x] **Revise the model loading system**
  The model loading system needs a review, which could take inspiration from Nexium.
  In particular, the mesh loading process needs to be fixed, and the correct bind pose should be loaded separately.
  See: https://github.com/Bigfoot71/Nexium/tree/master/source/Render/Core/Importer
  See: https://github.com/Bigfoot71/r3d/issues/98
  Fixed: https://github.com/Bigfoot71/r3d/pull/119

## **v0.6**

* [ ] **Modular State System and On-Demand Resource Loading**
  Replace `r3d_state.h` with a modular system, split by responsibility, with internal on-demand loading management.
  For example, introduce a *shader cache* that loads all shaders lazily when they are first requested during rendering.
  Apply the same approach to render targets and any other applicable resources.

* [ ] **Draw Call System Refactor**
  Migrate all content from `details/r3d_drawcall.h` into `r3d_draw.c`.

* [ ] **Unified Draw Call Storage and Index-Based Sorting**
  Replace multiple draw call arrays and in-place object sorting with:

  * a single array containing all draw calls
  * multiple index arrays per category
    Perform sorting and culling on the index arrays rather than directly on the draw call structures themselves.

* [ ] **Merge Scene Vertex Shaders**
  Merge all vertex shaders used by the `scene` module into a single unified vertex shader.

* [ ] **Environment API Refactor**
  Replace all environment-related functions with a single `R3D_Environment` structure, while keeping a global environment state.
  See: [https://github.com/Bigfoot71/r3d/discussions/117](https://github.com/Bigfoot71/r3d/discussions/117)

* [ ] **Skybox Revision and Reflection Probes**
  Revise the skybox system and add support for reflection probes, including probe blending.

## **v0.7**

* [ ] **Add Support for Custom Screen-Space Shaders (Post-Processing)**
  Allow custom shaders to be used in screen-space for post-processing effects.

* [ ] **Add Support for Custom Material Shaders**
  Allow custom shaders per material. This will likely require a different approach for deferred vs. forward rendering. Deferred mode will probably offer fewer possibilities than forward mode for custom material shaders.

* [ ] **Better logs**
  Add better logs for initialization, shutdown, loading operations, and failures.

## **v0.8**

* [ ] **Improve Shadow Map Management**
  Instead of using one shadow map per light, implement an internal batching system with a 2D texture array and `Sample2DArray` in shaders.
  *Note: This would remove per-light independent shadow map resolutions.*

* [ ] **Implement Cascaded Shadow Maps (CSM)**
  Add CSM support for directional lights.

* [ ] **Make a Wiki**
  Add wiki pages to the Github repository.

*Note: v0.8 features are still unsure and/or incomplete.*

## **Ideas (Not Planned Yet)**

* [ ] After implementing `Sampler2DArray` for shadow maps, explore using UBOs for lights, but also for environment and matrices/camera data.
* [ ] Provide an easy way to configure the number of lights in forward shaders (preferably runtime-configurable).
* [ ] Implement a system to save loaded skyboxes together with their generated irradiance and prefiltered textures for later reloading.
* [ ] Improve support for shadow/transparency interaction (e.g., colored shadows).
