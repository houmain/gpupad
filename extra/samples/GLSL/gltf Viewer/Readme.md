
glTF Viewer Sample
------------------

  * The sample uses sources and assets from the Khronos glTF 2.0 Sample Viewer
https://github.com/KhronosGroup/glTF-Sample-Viewer.
  * The HDRI set was obtained from http://www.hdrlabs.com/sibl/archive.html
and converted to irradiance/radiance cube maps using
cmft https://github.com/dariomanesku/cmft with these arguments:

```
cmft --input "PaperMill_E_3k.hdr"      \
     --filter irradiance               \
     --srcFaceSize 256                 \
     --dstFaceSize 256                 \
     --outputNum 1                     \
     --output0 "papermill_irr"         \
     --output0params ktx,rgba16f,cubemap
```

```
cmft --input "PaperMill_E_3k.hdr"      \
     ::Filter options                  \
     --filter radiance                 \
     --srcFaceSize 512                 \
     --excludeBase false               \
     --mipCount 9                      \
     --glossScale 14                   \
     --glossBias 3                     \
     --lightingModel phongbrdf         \
     --edgeFixup none                  \
     --dstFaceSize 256                 \
     ::Processing devices              \
     --numCpuProcessingThreads 4       \
     --useOpenCL true                  \
     --clVendor anyGpuVendor           \
     --deviceType gpu                  \
     --deviceIndex 0                   \
     ::Aditional operations            \
     --inputGammaNumerator 2.2         \
     --inputGammaDenominator 1.0       \
     --outputGammaNumerator 1.0        \
     --outputGammaDenominator 2.2      \
     --generateMipChain false          \
     ::Output                          \
     --outputNum 1                     \
     --output0 "papermill_pmrem"       \
     --output0params ktx,rgba16f,cubemap
```