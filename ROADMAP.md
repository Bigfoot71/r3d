# R3D Roadmap

## **v0.10**

* [x] **Implement Auto Exposure**
  Can be done by downsampling the scene texture and basing the effect on the final 1x1 mip level.
  ~~We could also expose the downsampled version of the scene for custom screen shaders.~~

* [x] **Add a render priority index to `R3D_Material`**
  Add a signed integer indicating render priority in materials, in order to control whether certain objects should be rendered before or after others.
  It should integrate easily with the existing material-based sorting.

* [x] **Add layer mask support for shadows**
  Allows rendering an object in the scene without rendering its shadows.
  Or rendering only the object's shadows without rendering the object itself, using the camera layer mask.
  _(in addition to the existing shadow casting mode, this will allow for more different configurations per light)_

* [x] **Add shadow opacity parameter**
  Allows controlling the transparency of shadows relative to the light.

* [ ] **Add specular parameter in the ORM map**
  Allows controlling specularity per material (in relation to metalness) instead of only through the lights.

## **Ideas (Not Planned Yet)**

* [ ] Investigate the integration of a velocity buffer and frame history, mainly for TAA and motion blur. The current begin/end rendering model makes this non-trivial.
* [ ] Improve support for shadow/transparency interaction (e.g., colored shadows).
* [ ] Implement Cascaded Shadow Maps (or alternative) for directional lights.
* [ ] Make wiki pages for the repo, consider it for the release.
