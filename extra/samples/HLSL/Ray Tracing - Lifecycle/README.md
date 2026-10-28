# Ray Tracing - Lifecycle

Uses [preliminaries (#55)](https://github.com/houmain/gpupad/pull/55),
[shared instances (#57)](https://github.com/houmain/gpupad/pull/57),
[shader records/local constants (#58)](https://github.com/houmain/gpupad/pull/58),
[TLAS lifecycle (#59)](https://github.com/houmain/gpupad/pull/59) and
[BLAS lifecycle (#60)](https://github.com/houmain/gpupad/pull/60).
All lifecycle facilities are required even for the static mode. Startup reports
an error when a required host property is unavailable; no alternate path is used.
Tracing and deformation stay disabled until checks and mesh setup succeed.

Derived from the HLSL Ray Tracing sample: trefoil mesh, vertex layout and camera.
One diffuse hit, sky background, no accumulation or external texture.
Open raytracing.gpjs, reset evaluation and inspect OutputImage. Move the mouse
to orbit; use steady evaluation for animation. Mesh generation uses GPUpad's
bundled GenerateMesh library directly.

Requires shared instances, explicit shader records/local constants and TLAS/BLAS
lifecycle support. Three hit records supply colors through one shared local-root
schema; all instances deliberately use ID0. Edit mode in generateScene.js and
reset evaluation: 0 static records, 1 animated TLAS rebuild, 2 animated TLAS
update, 3 GPU-deformed BLAS rebuild, 4 GPU-deformed BLAS update (default).
Modes3/4 deform the rest mesh on the GPU with matching normal transformation;
layout, indices and primitive activity remain unchanged. This demonstrates
visible behavior; the image alone does not establish rebuild/update execution.
