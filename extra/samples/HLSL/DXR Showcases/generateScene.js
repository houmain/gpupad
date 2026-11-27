function latticeTransform(index) {
    if (LatticeProbe)
        return [1, 0, 0, 4 * (index - 1.5), 0, 1, 0, 0, 0, 0, 1, 0];

    const row = Math.floor(index / 16);
    const column = index % 16;
    const time = typeof latticeTick === "undefined" ? PlaybackTime : Number(latticeTick) / 24;
    const phase = time * Math.PI / 5;
    const angle = column * Math.PI / 8 + row * 0.19 + 0.14 * Math.sin(phase + row * 0.5);
    const radius = 3.15 + 0.26 * Math.sin(row * 0.8 + phase);
    const pitch = 0.58 * Math.sin(phase + column * 0.55 + row * 0.32);
    const roll = angle + 0.45 * Math.sin(2 * phase + row * 0.43);
    const scale = 0.64;
    const c = Math.cos(roll) * scale;
    const s = Math.sin(roll) * scale;
    const cp = Math.cos(pitch);
    const sp = Math.sin(pitch);
    return [c, -s * cp, s * sp, radius * Math.cos(angle),
        s, c * cp, -c * sp, radius * Math.sin(angle),
        0, scale * sp, scale * cp, 2.4 * row + 0.32 * Math.sin(phase + column)];
}

function trefoilTransform(x, y, z, scale, angle, phase) {
    const time = FixtureMode ? 0 : PlaybackTime;
    const cycle = time * Math.PI / 5;
    const yaw = angle + 0.78 * Math.sin(cycle + phase);
    const pitch = 0.34 * Math.sin(2 * cycle + phase);
    const c = scale * Math.cos(yaw);
    const s = scale * Math.sin(yaw);
    const cp = Math.cos(pitch);
    const sp = Math.sin(pitch);
    return [c, s * sp, s * cp, x,
        0, scale * cp, -scale * sp, y + 0.065 * Math.sin(cycle + phase),
        -s, c * sp, c * cp, z];
}

(function () {
    const stacks = 96;
    const slices = 12;
    const vertices = [];
    const indices = [];
    for (let stack = 0; stack < stacks; ++stack) {
        const u = stack * 2 * Math.PI / stacks;
        const c = Math.cos(2 * u);
        const s = Math.sin(2 * u);
        const radius = 0.65 + 0.24 * Math.cos(3 * u);
        const dr = -0.72 * Math.sin(3 * u);
        const z = 0.24 * Math.sin(3 * u);
        const dz = 0.72 * Math.cos(3 * u);
        const binormalLength = Math.hypot(2 * radius, dz);

        const bx = -s * dz / binormalLength;
        const by = c * dz / binormalLength;
        const bz = -2 * radius / binormalLength;
        const tangentLength = Math.hypot(dr, binormalLength);
        const tx = (dr * c - 2 * radius * s) / tangentLength;
        const ty = (dr * s + 2 * radius * c) / tangentLength;
        const tz = dz / tangentLength;
        const nx = by * tz - bz * ty;
        const ny = bz * tx - bx * tz;
        const nz = bx * ty - by * tx;

        for (let slice = 0; slice < slices; ++slice) {
            const v = slice * 2 * Math.PI / slices;
            const cv = Math.cos(v);
            const sv = Math.sin(v);
            const x = cv * nx + sv * bx;
            const y = cv * ny + sv * by;
            const normalZ = cv * nz + sv * bz;
            vertices.push(radius * c + 0.13 * x, radius * s + 0.13 * y,
                z + 0.13 * normalZ, x, y, normalZ);

            const row = stack * slices;
            const nextRow = ((stack + 1) % stacks) * slices;
            const nextSlice = (slice + 1) % slices;
            indices.push(row + slice, row + nextSlice, nextRow + slice,
                row + nextSlice, nextRow + nextSlice, nextRow + slice);
        }
    }

    const session = app.session;
    const shared = session.getItem("Shared Scene");
    shared.getItem("Rest Vertices").setData(vertices);
    shared.getItem("Indices").setData(indices);

    if (ActiveShowcase !== 0)
        return;

    const latticeScene = session.getItem("Infinity Lattice/Scene & Buffers");
    const palette = [
        [0.80, 0.88, 0.98, 0.82],
        [0.96, 0.57, 0.25, 0.75],
        [0.20, 0.62, 0.78, 0.70],
        [0.65, 0.39, 0.68, 0.67]
    ];
    palette.forEach((material, i) => {
        latticeScene.getItem("Metal " + i).setData(material.concat([(i + 1) * 100, 0, 0, 0]));
    });

    const swap = typeof latticeSwapRecords !== "undefined" && Number(latticeSwapRecords) === 1;
    const records = LatticeProbe && swap ? [0, 2, 3, 1] : [2, 0, 3, 1];
    const probeMaterials = [1, 3, 0, 2];
    const geometrySetId = shared.getItem("Static Trefoil").id;
    const instances = [];
    const materialIndices = [];
    for (let i = 0; i < (LatticeProbe ? 4 : 256); ++i) {
        instances.push({
            type: "Instance",
            name: "Trefoil " + i,
            // shared blas address in the dxr instance descriptor
            // d3d12_raytracing_instance_desc.accelerationstructure
            geometrySetId,
            transform: "latticeTransform(" + i + ")",
            shaderInstanceId: String(i),
            hitGroupRecordOffset: String(records[(i + Math.floor(i / 16)) % 4])
        });
        materialIndices.push(LatticeProbe ? probeMaterials[i]
            : (Math.floor(i / 4) + Math.floor(i / 16) + i % 2) % 4);
    }
    latticeScene.getItem("Material Indices").setData(materialIndices);
    latticeScene.getItem("Lattice AS").replaceItems(instances);
})();
