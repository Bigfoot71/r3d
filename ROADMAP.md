# R3D Roadmap

## **v0.7**

* [ ] Adding the `R3D_InstanceBuffer` type to manage instances.
  See: https://github.com/Bigfoot71/r3d/discussions/121

* [ ] **Merge Scene Vertex Shaders**
  Merge all vertex shaders used by the `scene` module into a single unified vertex shader.

* [ ] **Skybox Revision and Reflection Probes**
  Revise the skybox system and add support for reflection probes, including probe blending.

* [ ] **Skybox Generation Support**
  Add support for procedural skybox generation.

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
