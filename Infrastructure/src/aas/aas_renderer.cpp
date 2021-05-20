
#include <infrastructure/logging.h>

#include <graphics/materials.h>
#include <graphics/buffers.h>
#include <graphics/bufferbinding.h>
#include <graphics/shaperenderer2d.h>
#include <graphics/shaperenderer3d.h>
#include <graphics/device.h>
#include <graphics/shaders.h>
#include <graphics/mdfmaterials.h>
#include <graphics/dynamictexture.h>

#include "aas/aas_renderer.h"

using namespace gfx;
using namespace DirectX;

namespace aas {

// Data to be stored for an AAS model so it can be rendered 
// more efficiently
struct AasRenderSubmeshData {	
	bool created = false;
	VertexBufferPtr posBuffer;
	VertexBufferPtr normalsBuffer;
	VertexBufferPtr uvBuffer;
	IndexBufferPtr idxBuffer;
	BufferBinding binding;

	AasRenderSubmeshData(RenderingDevice &device) : binding(device.CreateMdfBufferBinding()) {
	}

	NO_COPY_OR_MOVE(AasRenderSubmeshData);
};

struct AasRenderData : public gfx::IRenderState {
	std::array<std::unique_ptr<AasRenderSubmeshData>, 16> submeshes;

	AasRenderData() = default;
	NO_COPY_OR_MOVE(AasRenderData);
};

Renderer::Renderer(gfx::AnimatedModelFactory &aasFactory,
						 RenderingDevice& device, 
						 ShapeRenderer2d &shapeRenderer2d,
						 ShapeRenderer3d &shapeRenderer3d,
						 MdfMaterialFactory &mdfFactory) 
	: mDevice(device), 
	  mMdfFactory(mdfFactory), 
	  mShapeRenderer2d(shapeRenderer2d),
	  mShapeRenderer3d(shapeRenderer3d),
	  mAasFactory(aasFactory),
	  mGeometryShadowMaterial(CreateGeometryShadowMaterial(device)),
	  mShadowTarget(device.CreateRenderTargetTexture(BufferFormat::A8R8G8B8, ShadowMapWidth, ShadowMapHeight)),
	  mShadowTargetTmp(device.CreateRenderTargetTexture(BufferFormat::A8R8G8B8, ShadowMapWidth, ShadowMapHeight)),
	  mShadowMapMaterial(CreateShadowMapMaterial(device)),
	  mGaussBlurHor(CreateGaussBlurMaterial(device, mShadowTarget, true)),
	  mGaussBlurVer(CreateGaussBlurMaterial(device, mShadowTargetTmp, false)) {
}

Renderer::~Renderer() = default;

AasRenderSubmeshData& Renderer::GetSubmeshData(AasRenderData& renderData, int submeshId, Submesh & submesh)
{
	if (!renderData.submeshes[submeshId]) {
		renderData.submeshes[submeshId] = std::make_unique<AasRenderSubmeshData>(mDevice);
	}

	auto& submeshData = *renderData.submeshes[submeshId];
	if (!submeshData.created) {
		submeshData.posBuffer = mDevice.CreateVertexBuffer(submesh.GetPositions(), false);
		submeshData.normalsBuffer = mDevice.CreateVertexBuffer(submesh.GetNormals(), false);
		submeshData.uvBuffer = mDevice.CreateVertexBuffer(submesh.GetUV());
		submeshData.idxBuffer = mDevice.CreateIndexBuffer(submesh.GetIndices());

		submeshData.binding.AddBuffer(submeshData.posBuffer, 0, sizeof(DirectX::XMFLOAT4))
			.AddElement(VertexElementType::Float4, VertexElementSemantic::Position);
		submeshData.binding.AddBuffer(submeshData.normalsBuffer, 0, sizeof(DirectX::XMFLOAT4))
			.AddElement(VertexElementType::Float4, VertexElementSemantic::Normal);
		submeshData.binding.AddBuffer(submeshData.uvBuffer, 0, sizeof(DirectX::XMFLOAT2))
			.AddElement(VertexElementType::Float2, VertexElementSemantic::TexCoord);

		submeshData.created = true;
	} else {
		submeshData.posBuffer->Update(submesh.GetPositions());
		submeshData.normalsBuffer->Update(submesh.GetNormals());
	}

	return submeshData;
}

Material Renderer::CreateGeometryShadowMaterial(gfx::RenderingDevice &device)
{
	BlendSpec blendState;
	blendState.blendEnable = true;
	blendState.srcBlend = BlendOperand::SrcAlpha;
	blendState.destBlend = BlendOperand::InvSrcAlpha;
	RasterizerSpec rasterizerState;
	rasterizerState.cullMode = CullMode::None;
	DepthStencilSpec depthStencilState;
	depthStencilState.depthWrite = false;

	auto vs{ device.GetShaders().LoadVertexShader("shadow_geom_vs") };
	auto ps{ device.GetShaders().LoadPixelShader("diffuse_only_ps") };

	return device.CreateMaterial(blendState, depthStencilState, rasterizerState, {}, vs, ps);
}

Material Renderer::CreateShadowMapMaterial(gfx::RenderingDevice &device) {
	DepthStencilSpec depthStencilState;
	depthStencilState.depthEnable = false;
	auto vs{ device.GetShaders().LoadVertexShader("shadowmap_geom_vs") };
	auto ps{ device.GetShaders().LoadPixelShader("diffuse_only_ps") };

	BlendSpec blendSpec;
	blendSpec.blendEnable = true;
	blendSpec.destBlend = BlendOperand::Zero;
	blendSpec.srcBlend = BlendOperand::SrcAlpha;
	blendSpec.destAlphaBlend = BlendOperand::Zero;
	blendSpec.srcAlphaBlend = BlendOperand::One;

	return device.CreateMaterial(blendSpec, depthStencilState, {}, {}, vs, ps);
}

Material Renderer::CreateGaussBlurMaterial(gfx::RenderingDevice &device, const gfx::RenderTargetTexturePtr &texturePtr, bool horizontal) {
	BlendSpec blendState;
	SamplerSpec samplerState;
	samplerState.addressU = TextureAddress::Clamp;
	samplerState.addressV = TextureAddress::Clamp;
	samplerState.magFilter = TextureFilterType::Linear;
	samplerState.minFilter = TextureFilterType::Linear;
	samplerState.mipFilter = TextureFilterType::Linear;
	RasterizerSpec rasterizerState;
	rasterizerState.cullMode = CullMode::None;
	DepthStencilSpec depthStencilState;
	depthStencilState.depthEnable = false;

	auto vs(device.GetShaders().LoadVertexShader("gaussian_blur_vs"));
	Shaders::ShaderDefines horDefines;
	if (horizontal) {
		horDefines["HOR"] = "1";
	}
	auto ps(device.GetShaders().LoadPixelShader("gaussian_blur_ps", horDefines));

	std::vector<MaterialSamplerSpec> samplers {
		{ texturePtr, samplerState }
	};
	return device.CreateMaterial(blendState, depthStencilState, rasterizerState, samplers, vs, ps);
}

void Renderer::RecalcNormals(int vertexCount, const XMFLOAT4* pos, XMFLOAT4* normals, int primCount, const uint16_t* indices) {
	static XMFLOAT4 recalcNormalsBuffer[0x8000];
	memset(recalcNormalsBuffer, 0, vertexCount * sizeof(XMFLOAT4));

	// Process every TRI we have
	auto curIdx = indices;
	for (auto tri = 0; tri < primCount; ++tri) {
		// Indices of the three vertices making up this triangle
		auto idx1 = *curIdx++;
		auto idx2 = *curIdx++;
		auto idx3 = *curIdx++;

		auto pos1 = XMLoadFloat4(&pos[idx1]);
		auto pos2 = XMLoadFloat4(&pos[idx2]);
		auto pos3 = XMLoadFloat4(&pos[idx3]);

		auto v1to2(pos2 - pos1);
		auto v1to3(pos3 - pos1);

		// Calculate the surface normal of the surface defined 
		// by the two directional vectors
		auto surfNormal(XMVector3Cross(v1to2, v1to3) * -1);

		// The surface normal contributes to all three vertex normals
		XMStoreFloat4(&normals[idx1], surfNormal);
		XMStoreFloat4(&normals[idx2], surfNormal);
		XMStoreFloat4(&normals[idx3], surfNormal);
	}

	// Re-Normalize the normals we calculated
	for (auto i = 0; i < vertexCount; ++i) {
		auto normal(XMVector3Normalize(XMLoadFloat4(&normals[i])));
		XMStoreFloat4(&normals[i], normal);
	}
}

void Renderer::Render(gfx::AnimatedModel *model,
	const gfx::AnimatedModelParams& params,
	gsl::span<Light3d> lights,
	const MdfRenderOverrides *materialOverrides) {

	auto &renderData = model->GetOrCreateRenderState<AasRenderData>();
	
	auto materialIds(model->GetSubmeshes());
	for (size_t i = 0; i < materialIds.size(); ++i) {
		auto materialId = materialIds[i];
		auto submesh(params.rotation3d ? model->GetSubmeshForParticles(params, i) : model->GetSubmesh(params, i));
		
		// Remove special material marker in the upper byte and only 
		// use the actual shader registration id
		materialId &= 0x00FFFFFF;

		// Usually this should not happen, since it means there's  
		// an unbound replacement material
		if (materialId == 0) {
			continue;
		}

		// if material was not found
		if (materialId == 0x00FFFFFF) {
			continue;
		}

		auto material = mMdfFactory.GetById(materialId);
		
		if (!material) {
			logger->error("Legacy shader with id {} wasn't found.", materialId);
			continue;
		}

		material->Bind(mDevice, lights, materialOverrides);

		// Do we have to recalculate the normals?
		if (material->GetSpec()->recalculateNormals) {
			RecalcNormals(
				submesh->GetVertexCount(),
				submesh->GetPositions().data(),
				submesh->GetNormals().data(),
				submesh->GetPrimitiveCount(),
				submesh->GetIndices().data()
			);
		}

		auto &submeshData = GetSubmeshData(renderData, i, *submesh);
		submeshData.binding.Bind();

		mDevice.SetIndexBuffer(*submeshData.idxBuffer);
		mDevice.DrawIndexed(gfx::PrimitiveType::TriangleList, 
			submesh->GetVertexCount(), 
			submesh->GetPrimitiveCount() * 3);
	}

}

void Renderer::RenderWithoutMaterial(gfx::AnimatedModel *model, 
	const gfx::AnimatedModelParams& params) {

	auto &renderData = model->GetOrCreateRenderState<AasRenderData>();

	auto materialIds(model->GetSubmeshes());
	for (size_t i = 0; i < materialIds.size(); ++i) {
		auto submesh(model->GetSubmesh(params, i));

		auto &submeshData = GetSubmeshData(renderData, i, *submesh);
		submeshData.binding.Bind();

		mDevice.SetIndexBuffer(*submeshData.idxBuffer);
		mDevice.DrawIndexed(
			gfx::PrimitiveType::TriangleList,
			submesh->GetVertexCount(), 
			submesh->GetPrimitiveCount() * 3
		);
	}
}

struct ShadowGlobals {
	XMFLOAT4X4 projMatrix;
	XMFLOAT4 globalLightDir;
	XMFLOAT4 offsetZ;
	XMFLOAT4 alpha;
};

void Renderer::RenderGeometryShadow(gfx::AnimatedModel * model, 
	const gfx::AnimatedModelParams & params, 
	const gfx::Light3d & globalLight,
	float alpha)
{
	
	auto &renderData = model->GetOrCreateRenderState<AasRenderData>();

	mDevice.SetMaterial(mGeometryShadowMaterial);

	ShadowGlobals globals;
	globals.projMatrix = mDevice.GetCurrentCamera().GetViewProj();
	globals.globalLightDir = globalLight.dir;
	globals.offsetZ = { params.offsetZ, 0, 0, 0 };
	globals.alpha = { alpha, 0, 0, 0 };
	mDevice.SetVertexShaderConstants(0, globals);

	auto materialIds(model->GetSubmeshes());
	for (size_t i = 0; i < materialIds.size(); ++i) {
		auto submesh(model->GetSubmesh(params, i));
		
		auto &submeshData = GetSubmeshData(renderData, i, *submesh);
		submeshData.binding.Bind();
			
		mDevice.SetIndexBuffer(*submeshData.idxBuffer);
		mDevice.DrawIndexed(gfx::PrimitiveType::TriangleList, 
			submesh->GetVertexCount(), 
			submesh->GetPrimitiveCount() * 3
		);
	}

}

struct ShadowShaderGlobals {
	XMFLOAT4 shadowMapWorldPos;
	XMFLOAT4 lightDir;
	XMFLOAT4 height;
	XMFLOAT4 color;
};

void Renderer::RenderShadowMapShadow(gsl::span<gfx::AnimatedModel*> models,
										gsl::span<const gfx::AnimatedModelParams*> modelParams,
										const XMFLOAT3 &center,
										float radius,
										float height,
										const XMFLOAT4 &lightDir,
										float alpha,
										bool softShadows) {

	Expects(models.size() == modelParams.size());

	float shadowMapWorldX, shadowMapWorldWidth,
		shadowMapWorldZ, shadowMapWorldHeight;

	if (lightDir.x < 0.0) {
		shadowMapWorldX = center.x - 2 * radius + lightDir.x * height;
		shadowMapWorldWidth = 4 * radius - lightDir.x * height;
	} else {
		shadowMapWorldX = center.x - 2 * radius;
		shadowMapWorldWidth = lightDir.x * height + 4 * radius;
	}

	if (lightDir.z < 0.0) {
		shadowMapWorldZ = center.z - 2 * radius + lightDir.z * height;
		shadowMapWorldHeight = 4 * radius - lightDir.z * height;
	} else {
		shadowMapWorldZ = center.z - 2 * radius;
		shadowMapWorldHeight = lightDir.z + height + 4 * radius;
	}

	mDevice.PushRenderTarget(mShadowTarget, nullptr);

	mDevice.SetMaterial(mShadowMapMaterial);

	// Set shader params
	ShadowShaderGlobals globals;
	globals.shadowMapWorldPos = XMFLOAT4(
		shadowMapWorldX,
		shadowMapWorldZ,
		shadowMapWorldWidth,
		shadowMapWorldHeight
	);
	globals.lightDir = lightDir;
	globals.height.x = center.y;
	globals.color = XMFLOAT4(0, 0, 0, 0.5f);
	mDevice.SetVertexShaderConstants(0, globals);

	mDevice.ClearCurrentColorTarget(XMCOLOR(0, 0, 0, 0));

	for (int i = 0; i < models.size(); ++i) {
		RenderWithoutMaterial(models[i], *modelParams[i]);
	}
		
	if (softShadows) {
		mDevice.SetMaterial(mGaussBlurHor);
		mDevice.PushRenderTarget(mShadowTargetTmp, nullptr);
		mShapeRenderer2d.DrawFullScreenQuad();
		mDevice.PopRenderTarget();

		mDevice.PushRenderTarget(mShadowTarget, nullptr);
		mDevice.SetMaterial(mGaussBlurVer);
		mShapeRenderer2d.DrawFullScreenQuad();
		mDevice.PopRenderTarget();
	}
	
	mDevice.PopRenderTarget();

	auto shadowMapWorldBottom = shadowMapWorldZ + shadowMapWorldHeight;
	auto shadowMapWorldRight = shadowMapWorldX + shadowMapWorldWidth;

	std::array<gfx::ShapeVertex3d, 4> corners;
	corners[0].pos = { shadowMapWorldX, center.y, shadowMapWorldZ, 1 };
	corners[1].pos = { shadowMapWorldX, center.y, shadowMapWorldBottom, 1 };
	corners[2].pos = { shadowMapWorldRight, center.y, shadowMapWorldBottom, 1 };
	corners[3].pos = { shadowMapWorldRight, center.y, shadowMapWorldZ, 1 };
	corners[0].uv = { 0, 0 };
	corners[1].uv = { 0, 1 };
	corners[2].uv = { 1, 1 };
	corners[3].uv = { 1, 0 };

	mShapeRenderer3d.DrawQuad(corners, 0xFFFFFFFF, mShadowTarget);	

}

}
