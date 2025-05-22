
random = Math.random

function length(v) {
  return Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

function sub(a, b) {
  return [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
}

function Lambertian(diffuse) {
	return [diffuse[0], diffuse[1], diffuse[2], 1, 0.0, 0.0, 0];
}

function Metallic(diffuse, fuzziness) {
	return [diffuse[0], diffuse[1], diffuse[2], 1, fuzziness, 0.0, 1];
}

function Dielectric(refractionIndex) {
	return [0.7, 0.7, 1.0, 1, 0.0, refractionIndex, 2];
}

function Isotropic(diffuse) {
	return [diffuse[0], diffuse[1], diffuse[2], 1, 0.0, 0.0, 3];
}

function DiffuseLight(diffuse) {
	return [diffuse[0], diffuse[1], diffuse[2], 1, 0.0, 0.0, 4];
}


let spheres = [];
let materials = [];

function addSphere(center, radius, material) {
  spheres.push([center[0], center[1], center[2], radius]);
  materials.push(material);
}

addSphere([0, -1000, 0], 1000, Lambertian([0.5, 0.5, 0.5]));
addSphere([0, 1, 0], 1.0, Dielectric(1.5));
addSphere([-4, 1, 0], 1.0, Lambertian([0.4, 0.2, 0.1]));
addSphere([4, 1, 0], 1.0, Metallic([0.7, 0.6, 0.5], 0.0));

for (let i = -11; i < 11; ++i)
{
	for (let j = -11; j < 11; ++j)
	{
		const chooseMat = random();
		const center_y = j + 0.9 * random();
		const center_x = i + 0.9 * random();
		const center = [center_x, 0.2, center_y];

		if (length(sub(center, [4, 0.2, 0])) > 0.9)
		{
			if (chooseMat < 0.8) // Diffuse
			{
				const b = random() * random();
				const g = random() * random();
				const r = random() * random();

				addSphere(center, 0.2, Lambertian([r, g, b]));
			}
			else if (chooseMat < 0.95) // Metal
			{
				const fuzziness = 0.5 * random();
				const b = 0.5 * (1 + random());
				const g = 0.5 * (1 + random());
				const r = 0.5 * (1 + random());

				addSphere(center, 0.2, Metallic([r, g, b], fuzziness));
			}
			else // Glass
			{
				addSphere(center, 0.2, Dielectric(1.5));
			}
		}
	}
}

aabbs = []
for (let s of spheres) {
  const x = s[0]
  const y = s[1]
  const z = s[2]
  const rad = s[3]
  aabbs.push([x - rad, y - rad, z - rad, x + rad, y + rad, z + rad])
}

// convert to single array
spheres = [].concat.apply([], spheres)
materials = [].concat.apply([], materials)
aabbs = [].concat.apply([], aabbs)

app.session.setBufferData("Materials", materials)
app.session.setBufferData("Spheres", spheres)
app.session.setBufferData("AABBs", aabbs)

totalSamples = 0

