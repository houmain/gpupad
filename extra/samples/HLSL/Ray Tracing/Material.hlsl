
static const uint MaterialLambertian = 0;
static const uint MaterialMetallic = 1;
static const uint MaterialDielectric = 2;
static const uint MaterialIsotropic = 3;
static const uint MaterialDiffuseLight = 4;

struct Material {
    float4 Diffuse;
    int DiffuseTextureId;
    float Fuzziness;
    float RefractionIndex;
    uint MaterialModel;
};
