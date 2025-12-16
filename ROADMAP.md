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

* [ ] **Add Support for Custom Screen-Space Shaders (Post-Processing)**
  Allow custom shaders to be used in screen-space for post-processing effects.

* [ ] **Add Support for Custom Material Shaders**
  Allow custom shaders per material. This will likely require a different approach for deferred vs. forward rendering. Deferred mode will probably offer fewer possibilities than forward mode for custom material shaders.

- [ ] **Rework Scene Management for Directional Lights**  
  Currently, shadow projection for directional lights relies on scene bounds defined by `R3D_SetSceneBounds(bounds)`. This function only exists for that purpose, which makes the system unclear and inflexible. It would be better to design a more natural way to handle shadow projection for directional lights.

* [ ] **Better logs**
  Add better logs for initialization, shutdown, loading operations, and failures.

## **v0.7**

* [ ] **Improve Shadow Map Management**
  Instead of using one shadow map per light, implement an internal batching system with a 2D texture array and `Sample2DArray` in shaders.
  *Note: This would remove per-light independent shadow map resolutions.*

* [ ] **Implement Cascaded Shadow Maps (CSM)**
  Add CSM support for directional lights.

* [ ] **Make a Wiki**
  Add wiki pages to the Github repository.

*Note: v0.7 features are still unsure and incomplete.*

## **Ideas (Not Planned Yet)**

* [ ] After implementing `Sampler2DArray` for shadow maps, explore using UBOs for lights. Evaluate whether this is more efficient than the current approach.
* [ ] Provide an easy way to configure the number of lights in forward shaders (preferably runtime-configurable).
* [ ] Implement a system to save loaded skyboxes together with their generated irradiance and prefiltered textures for later reloading.
* [ ] Improve support for shadow/transparency interaction (e.g., colored shadows).
