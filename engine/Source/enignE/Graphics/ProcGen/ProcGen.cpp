#include "Graphics/ProcGen/ProcGen.h"

namespace
{
	DirectX::XMFLOAT3 CalculateFaceNormal(
		const DirectX::XMFLOAT3& a,
		const DirectX::XMFLOAT3& b,
		const DirectX::XMFLOAT3& c)
	{
		const DirectX::XMVECTOR av = DirectX::XMLoadFloat3(&a);
		const DirectX::XMVECTOR bv = DirectX::XMLoadFloat3(&b);
		const DirectX::XMVECTOR cv = DirectX::XMLoadFloat3(&c);
		const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(
			DirectX::XMVector3Cross(
				DirectX::XMVectorSubtract(bv, av),
				DirectX::XMVectorSubtract(cv, av)));

		DirectX::XMFLOAT3 result;
		DirectX::XMStoreFloat3(&result, normal);
		return result;
	}

	void PushQuad(
		MeshData& mesh,
		const DirectX::XMFLOAT3& normal,
		const DirectX::XMFLOAT3& a,
		const DirectX::XMFLOAT3& b,
		const DirectX::XMFLOAT3& c,
		const DirectX::XMFLOAT3& d)
	{
		const uint32_t start = static_cast<uint32_t>(mesh.vertices.size());

		mesh.vertices.push_back({{a.x, a.y, a.z}, {normal.x, normal.y, normal.z}, {0.0f, 0.0f}});
		mesh.vertices.push_back({{b.x, b.y, b.z}, {normal.x, normal.y, normal.z}, {1.0f, 0.0f}});
		mesh.vertices.push_back({{c.x, c.y, c.z}, {normal.x, normal.y, normal.z}, {1.0f, 1.0f}});
		mesh.vertices.push_back({{d.x, d.y, d.z}, {normal.x, normal.y, normal.z}, {0.0f, 1.0f}});

		mesh.indices.push_back(start + 0);
		mesh.indices.push_back(start + 1);
		mesh.indices.push_back(start + 2);
		mesh.indices.push_back(start + 2);
		mesh.indices.push_back(start + 3);
		mesh.indices.push_back(start + 0);
	}

	void PushTriangle(
		MeshData& mesh,
		const DirectX::XMFLOAT3& a,
		const DirectX::XMFLOAT3& b,
		const DirectX::XMFLOAT3& c)
	{
		const uint32_t start = static_cast<uint32_t>(mesh.vertices.size());
		const DirectX::XMFLOAT3 normal = CalculateFaceNormal(a, b, c);

		mesh.vertices.push_back({{a.x, a.y, a.z}, {normal.x, normal.y, normal.z}, {0.0f, 1.0f}});
		mesh.vertices.push_back({{b.x, b.y, b.z}, {normal.x, normal.y, normal.z}, {1.0f, 1.0f}});
		mesh.vertices.push_back({{c.x, c.y, c.z}, {normal.x, normal.y, normal.z}, {0.5f, 0.0f}});

		mesh.indices.push_back(start + 0);
		mesh.indices.push_back(start + 1);
		mesh.indices.push_back(start + 2);
	}
}

MeshData ProcGen::CreateCube(float s)
{
	const float h = s * 0.5f;
	MeshData m;
	m.vertices.reserve(24);
	m.indices.reserve(36);

	// +Z (front)
	PushQuad(m, {0.0f, 0.0f, 1.0f},
		{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h});

	// -Z (back)
	PushQuad(m, {0.0f, 0.0f, -1.0f},
		{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h});

	// +X (right)
	PushQuad(m, {1.0f, 0.0f, 0.0f},
		{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h});

	// -X (left)
	PushQuad(m, {-1.0f, 0.0f, 0.0f},
		{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h});

	// +Y (top)
	PushQuad(m, {0.0f, 1.0f, 0.0f},
		{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h});

	// -Y (bottom)
	PushQuad(m, {0.0f, -1.0f, 0.0f},
		{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h});

	return m;
}

MeshData ProcGen::CreatePyramid(float size)
{
	const float h = size * 0.5f;
	const DirectX::XMFLOAT3 apex = {0.0f, h, 0.0f};
	MeshData m;
	m.vertices.reserve(16);
	m.indices.reserve(18);

	PushQuad(m, {0.0f, -1.0f, 0.0f},
		{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h});

	PushTriangle(m, {-h, -h, h}, {h, -h, h}, apex);
	PushTriangle(m, {h, -h, -h}, {-h, -h, -h}, apex);
	PushTriangle(m, {h, -h, h}, {h, -h, -h}, apex);
	PushTriangle(m, {-h, -h, -h}, {-h, -h, h}, apex);

	return m;
}

MeshData ProcGen::CreateRectangle(float width, float height, float depth)
{
	float w = width * 0.5f;
	float h = height * 0.5f;
	float d = depth * 0.5f;

	MeshData m;

	m.vertices.clear();
	m.indices.clear();

	auto pushFace = [&](DirectX::XMFLOAT3 n,
		DirectX::XMFLOAT3 a, DirectX::XMFLOAT3 b, DirectX::XMFLOAT3 c, DirectX::XMFLOAT3 d)
		{
			uint32_t start = (uint32_t)m.vertices.size();

			SimpleVertex v0{}, v1{}, v2{}, v3{};

			v0 = { {a.x, a.y, a.z}, {n.x, n.y, n.z}, {0,0} };
			v1 = { {b.x, b.y, b.z}, {n.x, n.y, n.z}, {1,0} };
			v2 = { {c.x, c.y, c.z}, {n.x, n.y, n.z}, {1,1} };
			v3 = { {d.x, d.y, d.z}, {n.x, n.y, n.z}, {0,1} };

			m.vertices.push_back(v0);
			m.vertices.push_back(v1);
			m.vertices.push_back(v2);
			m.vertices.push_back(v3);

			m.indices.push_back(start + 0);
			m.indices.push_back(start + 1);
			m.indices.push_back(start + 2);

			m.indices.push_back(start + 2);
			m.indices.push_back(start + 3);
			m.indices.push_back(start + 0);
		};

	// +Z (front)
	pushFace({ 0,0,1 },
		{ -w,-h, d }, { w,-h, d }, { w, h, d }, { -w, h, d });

	// -Z (back)
	pushFace({ 0,0,-1 },
		{ w,-h,-d }, { -w,-h,-d }, { -w, h,-d }, { w, h,-d });

	// +X (right)
	pushFace({ 1,0,0 },
		{ w,-h, d }, { w,-h,-d }, { w, h,-d }, { w, h, d });

	// -X (left)
	pushFace({ -1,0,0 },
		{ -w,-h,-d }, { -w,-h, d }, { -w, h, d }, { -w, h,-d });

	// +Y (top)
	pushFace({ 0,1,0 },
		{ -w, h, d }, { w, h, d }, { w, h,-d }, { -w, h,-d });

	// -Y (bottom)
	pushFace({ 0,-1,0 },
		{ -w,-h,-d }, { w,-h,-d }, { w,-h, d }, { -w,-h, d });

	return m;
}

MeshData ProcGen::CreatePlane(float width, float depth)
{
	const float w = width * 0.5f;
	const float d = depth * 0.5f;

	MeshData m;
	m.vertices =
	{
		{{-w, 0.0f, -d}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
		{{ w, 0.0f, -d}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
		{{ w, 0.0f,  d}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
		{{-w, 0.0f,  d}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
		{{-w, 0.0f, -d}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
		{{ w, 0.0f, -d}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
		{{ w, 0.0f,  d}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
		{{-w, 0.0f,  d}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}
	};
	m.indices = {
		0, 2, 1,
		2, 0, 3,
		4, 5, 6,
		6, 7, 4
	};

	return m;
}

MeshData ProcGen::CreateSphere(float radius, int slices, int stacks)
{
	if (slices < 3)
		slices = 3;
	if (stacks < 2)
		stacks = 2;

	MeshData m;
	m.vertices.reserve(static_cast<size_t>((stacks + 1) * (slices + 1)));
	m.indices.reserve(static_cast<size_t>(stacks * slices * 6));

	for (int stack = 0; stack <= stacks; ++stack)
	{
		const float v = static_cast<float>(stack) / static_cast<float>(stacks);
		const float phi = v * DirectX::XM_PI;
		const float y = cosf(phi);
		const float ringRadius = sinf(phi);

		for (int slice = 0; slice <= slices; ++slice)
		{
			const float u = static_cast<float>(slice) / static_cast<float>(slices);
			const float theta = u * DirectX::XM_2PI;
			const float x = ringRadius * cosf(theta);
			const float z = ringRadius * sinf(theta);

			m.vertices.push_back({
				{x * radius, y * radius, z * radius},
				{x, y, z},
				{u, v}
			});
		}
	}

	const int ringVertexCount = slices + 1;
	for (int stack = 0; stack < stacks; ++stack)
	{
		for (int slice = 0; slice < slices; ++slice)
		{
			const uint32_t a = static_cast<uint32_t>(stack * ringVertexCount + slice);
			const uint32_t b = static_cast<uint32_t>((stack + 1) * ringVertexCount + slice);
			const uint32_t c = a + 1;
			const uint32_t d = b + 1;

			m.indices.push_back(a);
			m.indices.push_back(c);
			m.indices.push_back(d);

			m.indices.push_back(d);
			m.indices.push_back(b);
			m.indices.push_back(a);
		}
	}

	return m;
}

std::shared_ptr<Model> ProcGen::BuildModel(
	const MeshData& data)
{
	auto model = std::make_shared<Model>();
	auto mesh = std::make_shared<Mesh>();

	const uint32_t vbSize = static_cast<uint32_t>(data.vertices.size() * sizeof(SimpleVertex));
	const uint32_t ibSize = static_cast<uint32_t>(data.indices.size() * sizeof(uint32_t));

	mesh->InitializeIndexed(
		data.vertices.data(),
		vbSize,
		sizeof(SimpleVertex),
		data.indices.data(),
		ibSize
	);

	model->AddMesh(mesh);

	return model;
}
