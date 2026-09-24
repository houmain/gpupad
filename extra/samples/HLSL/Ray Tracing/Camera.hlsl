
cbuffer Camera : register(b0) {
    row_major float4x4 ModelView;
    row_major float4x4 Projection;
    row_major float4x4 ModelViewInverse;
    row_major float4x4 ProjectionInverse;
    float Aperture;
    float FocusDistance;
    float HeatmapScale;
    uint TotalNumberOfSamples;
    uint NumberOfSamples;
    uint NumberOfBounces;
    uint RandomSeed;
    uint HasSky;
};
