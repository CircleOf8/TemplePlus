
#include <platform/d3d.h>
#include <graphics/device.h>

#include "particles/render.h"
#include "particles/render_point.h"
#include "particles/instances.h"

using namespace DirectX;

namespace particles {

	void ParticleRenderer::ExtractScreenSpaceUnitVectors(const XMFLOAT4X4& projWorldMatrix) {
		// inverse, so screen->world?
		XMFLOAT4X4 invProj;
		XMStoreFloat4x4(&invProj, XMMatrixTranspose(XMLoadFloat4x4(&projWorldMatrix)));
		XMStoreFloat4(&screenSpaceUnitX, XMVector4Normalize(XMLoadFloat3((XMFLOAT3*)&invProj._11)));
		XMStoreFloat4(&screenSpaceUnitY, XMVector4Normalize(XMLoadFloat3((XMFLOAT3*)&invProj._21)));
		XMStoreFloat4(&screenSpaceUnitZ, XMVector4Normalize(XMLoadFloat3((XMFLOAT3*)&invProj._31)));
	}

	// No idea why this is a separate function
	void ParticleRenderer::ExtractScreenSpaceUnitVectors2(const XMFLOAT4X4& projWorldMatrix) {
		screenSpaceUnitX.x = projWorldMatrix._11;
		screenSpaceUnitX.y = projWorldMatrix._12;
		screenSpaceUnitX.z = projWorldMatrix._13;
		screenSpaceUnitX.w = 0;
		screenSpaceUnitY.x = projWorldMatrix._31;
		screenSpaceUnitY.y = projWorldMatrix._32;
		screenSpaceUnitY.z = projWorldMatrix._33;
		screenSpaceUnitY.w = 0;
		screenSpaceUnitZ.x = projWorldMatrix._21;
		screenSpaceUnitZ.y = projWorldMatrix._22;
		screenSpaceUnitZ.z = projWorldMatrix._23;
		screenSpaceUnitZ.w = 0;
		
		XMStoreFloat4(&screenSpaceUnitX, XMVector4Normalize(XMLoadFloat4(&screenSpaceUnitX)));
		XMStoreFloat4(&screenSpaceUnitY, XMVector4Normalize(XMLoadFloat4(&screenSpaceUnitY)));
		XMStoreFloat4(&screenSpaceUnitZ, XMVector4Normalize(XMLoadFloat4(&screenSpaceUnitZ)));
	}

	class ParticleRendererManager::Impl {
	public:
		explicit Impl(RenderingDevice& device, 
			AnimatedModelFactory &modelFactory, 
			AnimatedModelRenderer &modelRenderer)
			: mSpriteRenderer(device), 
			  mDiscRenderer(device),
			  mModelRenderer(device, modelFactory, modelRenderer) {
		}

		SpriteParticleRenderer mSpriteRenderer;
		DiscParticleRenderer mDiscRenderer;
		ModelParticleRenderer mModelRenderer;
	};

	bool ParticleRenderer::GetEmitterWorldMatrix(const PartSysEmitter& emitter, XMFLOAT4X4& worldMatrix) {

		auto spec = emitter.GetSpec();
		auto particleSpace = spec->GetParticleSpace();
		auto emitterSpace = spec->GetSpace();
		if (particleSpace == PartSysParticleSpace::SameAsEmitter) {

			if (emitterSpace == PartSysEmitterSpace::ObjectPos || emitterSpace == PartSysEmitterSpace::ObjectYpr) {
				XMMATRIX localMat;
				if (emitterSpace == PartSysEmitterSpace::ObjectYpr) {
					auto angle = emitter.GetObjRotation() + XM_PI;
					localMat = XMMatrixRotationY(angle);
				} else {
					localMat = XMMatrixIdentity();
				}

				// Set the translation component of the transformation matrix
				localMat.r[3] = XMVectorSet(emitter.GetObjPos().x,
					emitter.GetObjPos().y,
					emitter.GetObjPos().z,
					1
				);

				XMStoreFloat4x4(
					&worldMatrix,
					localMat * XMLoadFloat4x4(&mDevice.GetCurrentCamera().GetViewProj())
				);
				ExtractScreenSpaceUnitVectors(worldMatrix);
				return true;
			}
			if (emitterSpace == PartSysEmitterSpace::NodePos || emitterSpace == PartSysEmitterSpace::NodeYpr) {
				auto external = IPartSysExternal::GetCurrent();

				if (emitterSpace == PartSysEmitterSpace::NodeYpr) {
					XMFLOAT4X4 boneMatrix;
					if (!external->GetBoneWorldMatrix(emitter.GetAttachedTo(), spec->GetNodeName(), boneMatrix)) {
						// This effectively acts as a fallback if the bone doesn't exist
						auto x = emitter.GetObjPos().x;
						auto y = emitter.GetObjPos().y;
						auto z = emitter.GetObjPos().z;
						XMStoreFloat4x4(
							&boneMatrix, 
							XMMatrixTranslation(x, y, z)
						);
					}

					XMStoreFloat4x4(
						&worldMatrix,
						XMLoadFloat4x4(&boneMatrix) * XMLoadFloat4x4(&mDevice.GetCurrentCamera().GetViewProj())
						);
					ExtractScreenSpaceUnitVectors(worldMatrix);
					return true;
				}

				XMFLOAT4X4 boneMatrix;
				if (external->GetBoneWorldMatrix(emitter.GetAttachedTo(), spec->GetNodeName(), (Matrix4x4&)boneMatrix)) {
					auto x = boneMatrix._41;
					auto y = boneMatrix._42;
					auto z = boneMatrix._43;

					XMStoreFloat4x4(
						&worldMatrix,
						XMMatrixTranslation(x, y, z) * XMLoadFloat4x4(&mDevice.GetCurrentCamera().GetViewProj())
						);

					ExtractScreenSpaceUnitVectors(worldMatrix);
					return true;
				}

				return false;
			}

			worldMatrix = mDevice.GetCurrentCamera().GetViewProj();
			ExtractScreenSpaceUnitVectors(worldMatrix);
			return true;
		}

		if (particleSpace == PartSysParticleSpace::World) {
			worldMatrix = mDevice.GetCurrentCamera().GetViewProj();
			ExtractScreenSpaceUnitVectors(worldMatrix);
			return true;
		}

		if (emitterSpace != PartSysEmitterSpace::ObjectPos && emitterSpace != PartSysEmitterSpace::ObjectYpr) {
			if (emitterSpace != PartSysEmitterSpace::NodePos && emitterSpace != PartSysEmitterSpace::NodeYpr)
				return true;

			auto external = IPartSysExternal::GetCurrent();

			XMFLOAT4X4 boneMatrix;
			if (emitterSpace == PartSysEmitterSpace::NodeYpr) {
				// Use the entire bone matrix if possible
				external->GetBoneWorldMatrix(emitter.GetAttachedTo(), spec->GetNodeName(), boneMatrix);
			} else {
				// Only use the bone translation part
				if (!external->GetBoneWorldMatrix(emitter.GetAttachedTo(), spec->GetNodeName(), (Matrix4x4&) boneMatrix))
					return false;
				XMStoreFloat4x4(
					&boneMatrix,
					XMMatrixTranslation(boneMatrix._41, 
						boneMatrix._42,
						boneMatrix._43)
					); // TODO: This might not be needed...
			}
			worldMatrix = mDevice.GetCurrentCamera().GetViewProj();
			ExtractScreenSpaceUnitVectors2(boneMatrix);
			return true;
		}

		XMFLOAT4X4 matrix;
		if (emitterSpace == PartSysEmitterSpace::ObjectYpr) {
			auto angle = emitter.GetObjRotation() + XM_PI;
			XMStoreFloat4x4(&matrix, XMMatrixRotationY(angle));
		} else {
			XMStoreFloat4x4(&matrix, XMMatrixIdentity());
		}
		worldMatrix = mDevice.GetCurrentCamera().GetViewProj();
		ExtractScreenSpaceUnitVectors2(matrix);
		return true;
	}

	ParticleRendererManager::ParticleRendererManager(RenderingDevice &device, AnimatedModelFactory& modelFactory,
		AnimatedModelRenderer &modelRenderer) : mImpl(new Impl(device, modelFactory, modelRenderer)) {
	}

	ParticleRendererManager::~ParticleRendererManager() = default;

	ParticleRenderer& ParticleRendererManager::GetRenderer(PartSysParticleType type) {
		switch (type) {
		case PartSysParticleType::Point:
		case PartSysParticleType::Sprite:
			return mImpl->mSpriteRenderer;
		case PartSysParticleType::Disc:
			return mImpl->mDiscRenderer;
		case PartSysParticleType::Model:
			return mImpl->mModelRenderer;
		default:
			throw TempleException("Cannot render particle type");
		}
	}
}
