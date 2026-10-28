# Ray Tracing - SharedInstances

Uses the [preliminaries (#55)](https://github.com/houmain/gpupad/pull/55)
and requires [shared-BLAS instance controls (#57)](https://github.com/houmain/gpupad/pull/57).
Startup reports an error if those instance controls are unavailable; it does
not reduce the scene to its geometry owner or duplicate geometry as a substitute.
Tracing stays disabled until dependency checks and mesh setup succeed.

Derived from the HLSL Ray Tracing sample: trefoil mesh, vertex layout and camera.
One diffuse hit, sky background, no accumulation or external texture.
Open raytracing.gpjs, reset evaluation and inspect OutputImage. Move the mouse
to orbit; use steady evaluation for animation. Mesh generation uses GPUpad's
bundled GenerateMesh library directly.

Requires shared-BLAS instancing support. Three instances share the owner
instance's mesh allocation, with independent transforms and IDs0/1/2.
The closest-hit shader selects diffuse color by InstanceID; normals use the
inverse-transpose transform. The image alone does not prove allocation sharing.
