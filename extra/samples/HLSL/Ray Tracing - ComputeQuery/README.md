# Ray Tracing - ComputeQuery

Requires [DXC profile selection (#55)](https://github.com/houmain/gpupad/pull/55)
and [compute TLAS binding (#56)](https://github.com/houmain/gpupad/pull/56).
These are host requirements; the sample does not implement them. A host without
the required profile/reflection/binding support reports compilation/evaluation
errors. No legacy rendering path is selected.

Derived from the HLSL Ray Tracing sample: trefoil mesh, vertex layout and camera.
One diffuse hit, sky background, no accumulation or external texture.
Open raytracing.gpjs, reset evaluation and inspect OutputImage. Move the mouse
to orbit; use steady evaluation for animation. Mesh generation uses GPUpad's
bundled GenerateMesh library directly.

Requires Direct3D compute acceleration-structure binding and DXC SM6.5.
The compute shader uses inline triangle traversal and committed hit data.
There is no ray-generation pipeline or hit/miss shader callback.
