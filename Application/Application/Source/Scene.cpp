#include "Scene.h"
#include <DirectXMath.h>
#include <numeric>
#include "DX/Raytracing/Helper.h"
#include "DX/Raytracing/Shader/ShaderTable.h"
#include "ImGui/ImGuiManager.h"
#include "Utility/Debug.h"
#include "Utility/IO/GLBLoader.h"
#include "Utility/IO/TextureLoader.h"
#include "Utility/Path.h"
#include "Utility/StringUtil.h"

#include "CompiledShaders/ClosestHit_Normal.hlsl.h"
#include "CompiledShaders/ClosestHit_Plane.hlsl.h"
#include "CompiledShaders/ClosestHit_Sphere.hlsl.h"
#include "CompiledShaders/Miss.hlsl.h"
#include "CompiledShaders/RayGenShader.hlsl.h"
#include "CompiledShaders/Shadow.hlsl.h"

#include "Assets/Shader/Raytracing/Util/HitGroupCompat.h"
#include "Assets/Shader/Raytracing/Util/MissCompat.h"

using namespace Framework::DX;
using namespace Framework::Utility;
using namespace DirectX;

namespace {
    static const std::wstring MISS_SHADER_NAME = L"Miss";
    static const std::wstring MISS_SHADOW_SHADER_NAME = L"Shadow";
    static const std::wstring CLOSEST_HIT_NORMAL_NAME = L"ClosestHit_Normal";
    static const std::wstring CLOSEST_HIT_PLANE_NAME = L"ClosestHit_Plane";
    static const std::wstring CLOSEST_HIT_SPHERE_NAME = L"ClosestHit_Sphere";
    static const std::wstring RAY_GEN_NAME = L"RayGenShader";
    static const std::wstring HIT_GROUP_UFO_NAME = L"HitGroup_UFO";
    static const std::wstring HIT_GROUP_QUAD_NAME = L"HitGroup_Quad";
    static const std::wstring HIT_GROUP_FLOOR_NAME = L"HitGroup_Floor";
    static const std::wstring HIT_GROUP_SPHERE_NAME = L"HitGroup_Sphere";

    static const std::unordered_map<BottomLevelASType::MyEnum, std::wstring> MODEL_NAMES = {
        { BottomLevelASType::UFO, L"UFO.glb" },
        { BottomLevelASType::Sphere, L"sphere.glb" },
        { BottomLevelASType::Floor, L"floor.glb" },
    };

    static constexpr UINT UFO_COUNT = 0;
    static constexpr UINT QUAD_COUNT = 0;
    static constexpr UINT FLOOR_COUNT = 1;
    static constexpr UINT SPHERE_COUNT = 100;
    static constexpr UINT TLAS_NUM = UFO_COUNT + QUAD_COUNT + FLOOR_COUNT + SPHERE_COUNT;

    inline void createBuffer(ID3D12Device* device, ID3D12Resource** resource, void* data,
        UINT64 size, LPCWSTR name = nullptr) {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        MY_THROW_IF_FAILED(
            device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE,
                &bufferDesc, D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(resource)));
        MY_THROW_IF_FAILED((*resource)->SetName(name));
        void* mapped;
        MY_THROW_IF_FAILED((*resource)->Map(0, nullptr, &mapped));
        memcpy(mapped, data, size);
        (*resource)->Unmap(0, nullptr);
    };

    template <class T>
    inline std::vector<T> toLinearList(const std::vector<std::vector<T>>& list) {
        std::vector<T> res;
        for (size_t i = 0; i < list.size(); i++) {
            res.insert(res.end(), list[i].begin(), list[i].end());
        }
        return res;
    }

    using PositionList = std::vector<Framework::Math::Vector3>;
    using NormalList = std::vector<Framework::Math::Vector3>;
    using UVList = std::vector<Framework::Math::Vector2>;
    using TangentList = std::vector<Framework::Math::Vector4>;

    inline std::vector<Framework::DX::Vertex> toLinearVertices(
        const std::vector<PositionList>& positions, const std::vector<NormalList>& normals = {},
        const std::vector<UVList>& uvs = {}, const std::vector<TangentList>& tangents = {}) {
        std::vector<Framework::DX::Vertex> res;
        for (size_t i = 0; i < positions.size(); i++) {
            for (size_t j = 0; j < positions[i].size(); j++) {
                Framework::DX::Vertex v;
                v.position = positions[i][j];
                v.normal = normals.empty() || normals[i].empty() ? Vec3(0, 0, 0) : normals[i][j];
                v.uv = uvs.empty() || uvs[i].empty() ? Vec2(0, 0) : uvs[i][j];
                v.tangent
                    = tangents.empty() || tangents[i].empty() ? Vec4(0, 0, 0, 0) : tangents[i][j];
                res.emplace_back(v);
            }
        }
        return res;
    }

    inline void allocateUAVBuffer(ID3D12Device* device, UINT64 bufferSize,
        ID3D12Resource** resource, D3D12_RESOURCE_STATES initResourceState,
        LPCWSTR name = nullptr) {
        auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
            bufferSize, D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        MY_THROW_IF_FAILED(
            device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE,
                &bufferDesc, initResourceState, nullptr, IID_PPV_ARGS(resource)));
        (*resource)->SetName(name);
    };

    Deg mQuadRotate = Deg(0.0f);
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPreInfo = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};

    enum ModelTextureType {
        Default_AlbedoTexture,
        Default_NormalMap,
        Default_MetallicRoughnessMap,
        Default_EmissiveMap,
        Default_OcclusionMap,

        UFO_AlbedoTexture,
        UFO_NormalMap,
        UFO_MetallicRoughness,
        UFO_EmissiveMap,
        UFO_OcclusionMap,

        Quad_AlbedoTexture,
        Quad_NormalMap,
        Quad_MetallicRoughnessMap,
        Quad_EmissiveMap,
        Quad_OcclusionMap,

        Plane_AlbedoTexture,
        Plane_NormalMap,
        Plane_MetallicRoughnessMap,
        Plane_EmissiveMap,
        Plane_OcclusionMap,

        Sphere_AlbedoTexture,
        Sphere_NormalMap,
        Sphere_MetallicRoughnessMap,
        Sphere_EmissiveMap,
        Sphere_OcclusionMap,
    };
    std::unordered_map<ModelTextureType, UINT> mTextureIDs;

    ComPtr<ID3D12Resource> mInstanceDescs;
} // namespace

Scene::Scene(Framework::DX::DeviceResource* device, Framework::Input::InputManager* inputManager,
    UINT width, UINT height)
    : mDeviceResource(device),
      mInputManager(inputManager),
      mDXRDevice(),
      mWidth(width),
      mHeight(height) {
    Framework::Desc::DescriptorTableDesc desc = { L"ResourceTable", 10000,
        Framework::Desc::HeapType::CBV_SRV_UAV, Framework::Desc::HeapFlag::ShaderVisible };
    mDescriptorTable = std::make_unique<CountingDescriptorTable>(device->getDevice(), desc);
    mCameraPosition = Vec3(0, 0, -10);
    mLightPosition = Vec3(0, 100, -100);
    mLightDiffuse = Color4(1.0f, 1.0f, 1.0f, 1.0f);
    mLightAmbient = Color4(0.1f, 0.1f, 0.1f, 1.0f);

    mTextures.resize(TEXTURE_NUM);
}
Scene::~Scene() {}

void Scene::create() {
    createDeviceDependentResources();
    createWindowDependentResources();
    {
        ID3D12Device* device = mDeviceResource->getDevice();
        UINT backBufferCount = mDeviceResource->getBackBufferCount();
        mSceneCB.create(device, backBufferCount, L"SceneConstantBuffer");
    }
}

void Scene::reset() {
    mDXRDevice.reset();
    mGpuTimer.reset();
    mGpuTimer.releaseDevice();
}

void Scene::update() {
    mTime.update();
    ImGui::Begin("Status");
    { ImGui::Text("FPS:%0.3f", mTime.getFPS()); }
    ImGui::End();

    ImGui::Begin("Parameter");
    {
        ImGui::SetNextTreeNodeOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("Camera")) {
            ImGui::SetNextTreeNodeOpen(true, ImGuiCond_::ImGuiCond_Once);
            if (ImGui::TreeNode("Position")) {
                ImGui::DragFloat("X", &mCameraPosition.x, 1.0f);
                ImGui::DragFloat("Y", &mCameraPosition.y, 1.0f);
                ImGui::DragFloat("Z", &mCameraPosition.z, 1.0f);
                ImGui ::TreePop();
            }
            ImGui::SetNextTreeNodeOpen(true, ImGuiCond_::ImGuiCond_Once);
            if (ImGui::TreeNode("Rotation")) {
                ImGui::DragFloat("X", &mCameraRotation.x, 0.1f);
                ImGui::DragFloat("Y", &mCameraRotation.y, 0.1f);
                ImGui::DragFloat("Z", &mCameraRotation.z, 0.1f);
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        ImGui::SetNextTreeNodeOpen(true, ImGuiCond_::ImGuiCond_Once);
        if (ImGui::TreeNode("LightPosition")) {
            ImGui::DragFloat("X", &mLightPosition.x, 1.0f);
            ImGui::DragFloat("Y", &mLightPosition.y, 1.0f);
            ImGui::DragFloat("Z", &mLightPosition.z, 1.0f);
            ImGui::TreePop();
        }
    }
    ImGui::End();

    mSceneCB->cameraPosition = Vec4(mCameraPosition, 1.0f);
    const float aspect = static_cast<float>(mWidth) / static_cast<float>(mHeight);

    Mat4 view = Mat4::createRotation(mCameraRotation) * Mat4::createTranslate(mCameraPosition);
    view = view.inverse();
    Mat4 proj = Mat4::createProjection(Deg(45.0f), aspect, 0.1f, 100.0f);
    Mat4 vp = view * proj;
    mSceneCB->projectionToWorld = vp.inverse();
    mSceneCB->lightPosition = Vec4(mLightPosition, 1.0f);
    mSceneCB->lightDiffuse = mLightDiffuse;
    mSceneCB->lightAmbient = mLightAmbient;

    if (mInputManager->getKeyboard()->getKey(Framework::Input::KeyCode::LShift)) {
        mQuadRotate += 1.0f;
    }
}

void Scene::render() {
    ID3D12Device* device = mDeviceResource->getDevice();
    ID3D12GraphicsCommandList5* dxrCommandList = mDXRDevice.getDXRCommandList();

    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDesc(TLAS_NUM);
    UINT offset = 0;
    for (UINT n = 0; n < UFO_COUNT; n++) {
        Rad rotX = mQuadRotate.toRadians() * 0.37f;
        Rad rotY = mQuadRotate.toRadians() * 1.45f;
        Rad rotZ = mQuadRotate.toRadians() * 0.73f;
        XMMATRIX transform
            = XMMatrixRotationRollPitchYaw(rotX.getRad(), rotY.getRad(), rotZ.getRad())
            * XMMatrixTranslation((float)n * 5, 0, 0);
        instanceDesc[n + offset].InstanceID = 0;
        instanceDesc[n + offset].InstanceMask = 0xff;
        instanceDesc[n + offset].Flags
            = D3D12_RAYTRACING_INSTANCE_FLAGS::D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        instanceDesc[n + offset].InstanceContributionToHitGroupIndex
            = LocalRootSignature::HitGroupIndex::UFO;
        instanceDesc[n + offset].AccelerationStructure
            = mBLASBuffers[BottomLevelASType::UFO].buffer->GetGPUVirtualAddress();
        XMStoreFloat3x4(
            reinterpret_cast<XMFLOAT3X4*>(instanceDesc[n + offset].Transform), transform);
    }

    offset += UFO_COUNT;
    for (UINT n = 0; n < QUAD_COUNT; n++) {
        XMMATRIX transform = XMMatrixScaling(1, 1, 1)
            * XMMatrixRotationRollPitchYaw(0, mQuadRotate.toRadians().getRad(), 0)
            * XMMatrixTranslation((float)n * 5 + 20, 3, 0);
        instanceDesc[n + offset].InstanceID = 0;
        instanceDesc[n + offset].InstanceMask = 0xff;
        instanceDesc[n + offset].Flags
            = D3D12_RAYTRACING_INSTANCE_FLAGS::D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        instanceDesc[n + offset].InstanceContributionToHitGroupIndex
            = LocalRootSignature::HitGroupIndex::Quad;
        instanceDesc[n + offset].AccelerationStructure
            = mBLASBuffers[BottomLevelASType::Quad].buffer->GetGPUVirtualAddress();
        XMStoreFloat3x4(
            reinterpret_cast<XMFLOAT3X4*>(instanceDesc[n + offset].Transform), transform);
    }
    offset += QUAD_COUNT;
    for (UINT n = 0; n < FLOOR_COUNT; n++) {
        XMMATRIX transform = XMMatrixScaling(100, 1, 100) * XMMatrixTranslation(0, -5, 0);
        instanceDesc[n + offset].InstanceID = 0;
        instanceDesc[n + offset].InstanceMask = 0xff;
        instanceDesc[n + offset].Flags
            = D3D12_RAYTRACING_INSTANCE_FLAGS::D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        instanceDesc[n + offset].InstanceContributionToHitGroupIndex
            = LocalRootSignature::HitGroupIndex::Floor;
        instanceDesc[n + offset].AccelerationStructure
            = mBLASBuffers[BottomLevelASType::Floor].buffer->GetGPUVirtualAddress();
        XMStoreFloat3x4(
            reinterpret_cast<XMFLOAT3X4*>(instanceDesc[n + offset].Transform), transform);
    }
    offset += FLOOR_COUNT;
    UINT root = Framework::Math::MathUtil::sqrt(SPHERE_COUNT);
    for (UINT n = 0; n < SPHERE_COUNT; n++) {
        XMMATRIX transform
            = XMMatrixTranslation(((n - root / 2) / root) * 20, 3, ((n - root / 2) % root) * 20);
        instanceDesc[n + offset].InstanceID = 0;
        instanceDesc[n + offset].InstanceMask = 0xff;
        instanceDesc[n + offset].Flags
            = D3D12_RAYTRACING_INSTANCE_FLAGS::D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        instanceDesc[n + offset].InstanceContributionToHitGroupIndex
            = LocalRootSignature::HitGroupIndex::Sphere;
        instanceDesc[n + offset].AccelerationStructure
            = mBLASBuffers[BottomLevelASType::Sphere].buffer->GetGPUVirtualAddress();
        XMStoreFloat3x4(
            reinterpret_cast<XMFLOAT3X4*>(instanceDesc[n + offset].Transform), transform);
    }

    createBuffer(device, mInstanceDescs.ReleaseAndGetAddressOf(), instanceDesc.data(),
        instanceDesc.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), L"InstanceDescs");
    topLevelBuildDesc.Inputs.InstanceDescs = mInstanceDescs->GetGPUVirtualAddress();

    dxrCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
    mDeviceResource->getCommandList()->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::UAV(mTLASBuffer.buffer.Get()));

    ID3D12GraphicsCommandList* commandList = mDeviceResource->getCommandList();
    UINT frameIndex = mDeviceResource->getCurrentFrameIndex();
    commandList->SetComputeRootSignature(mGlobalRootSignature->getRootSignature());
    mSceneCB.copyStatingToGPU(frameIndex);

    ID3D12DescriptorHeap* heaps[] = { mDescriptorTable->getHeap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    commandList->SetComputeRootDescriptorTable(
        GlobalRootSignature::Slot::RenderTarget, mRaytracingOutput.getGPUHandle());
    commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::AccelerationStructure,
        mTLASBuffer.buffer->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(
        GlobalRootSignature::Slot::SceneConstant, mSceneCB.gpuVirtualAddress());
    commandList->SetComputeRootDescriptorTable(
        GlobalRootSignature::Slot::IndexBuffer, mResourcesIndexBuffer.getGPUHandle());
    commandList->SetComputeRootDescriptorTable(
        GlobalRootSignature::Slot::VertexBuffer, mResourcesVertexBuffer.getGPUHandle());

    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    dispatchDesc.RayGenerationShaderRecord.StartAddress = mRayGenTable->GetGPUVirtualAddress();
    dispatchDesc.RayGenerationShaderRecord.SizeInBytes = mRayGenTable->GetDesc().Width;

    dispatchDesc.MissShaderTable.StartAddress = mMissTable->GetGPUVirtualAddress();
    dispatchDesc.MissShaderTable.SizeInBytes = mMissTable->GetDesc().Width;
    dispatchDesc.MissShaderTable.StrideInBytes = mMissStride;

    dispatchDesc.HitGroupTable.StartAddress = mHitGroupTable->GetGPUVirtualAddress();
    dispatchDesc.HitGroupTable.SizeInBytes = mHitGroupTable->GetDesc().Width;
    dispatchDesc.HitGroupTable.StrideInBytes = mHitGroupStride;

    dispatchDesc.Width = mWidth;
    dispatchDesc.Height = mHeight;
    dispatchDesc.Depth = 1;

    dxrCommandList->SetPipelineState1(mDXRStateObject->mPipelineStateObject.Get());
    dxrCommandList->DispatchRays(&dispatchDesc);

    D3D12_RESOURCE_BARRIER preCopyBarriers[2];
    preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mDeviceResource->getRenderTarget(),
        D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST);
    preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(mRaytracingOutput.getResource(),
        D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->ResourceBarrier(_countof(preCopyBarriers), preCopyBarriers);

    commandList->CopyResource(mDeviceResource->getRenderTarget(), mRaytracingOutput.getResource());

    D3D12_RESOURCE_BARRIER postCopyBarriers[2];
    postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mDeviceResource->getRenderTarget(),
        D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET);
    postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(mRaytracingOutput.getResource(),
        D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->ResourceBarrier(_countof(postCopyBarriers), postCopyBarriers);

    //mDebugWindow->draw();
}

void Scene::onWindowSizeChanged(UINT width, UINT height) {
    mWidth = width;
    mHeight = height;

    releaseWindowDependentResources();
    createWindowDependentResources();
}

void Scene::createDeviceDependentResources() {
    mDXRDevice.create(mDeviceResource->getDevice(), mDeviceResource->getCommandList());
    {
        mGpuTimer.storeDevice(mDeviceResource->getDevice(), mDeviceResource->getCommandQueue(),
            mDeviceResource->getBackBufferCount());
    }
    { //グローバルルートシグネチャ
        { CD3DX12_DESCRIPTOR_RANGE ranges[3];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

    std::vector<CD3DX12_ROOT_PARAMETER> rootParams(GlobalRootSignature::Slot::Count);
    rootParams[GlobalRootSignature::Slot::RenderTarget].InitAsDescriptorTable(1, &ranges[0]);
    rootParams[GlobalRootSignature::Slot::AccelerationStructure].InitAsShaderResourceView(0);
    rootParams[GlobalRootSignature::Slot::SceneConstant].InitAsConstantBufferView(0);
    rootParams[GlobalRootSignature::Slot::IndexBuffer].InitAsDescriptorTable(1, &ranges[1]);
    rootParams[GlobalRootSignature::Slot::VertexBuffer].InitAsDescriptorTable(1, &ranges[2]);

    std::vector<CD3DX12_STATIC_SAMPLER_DESC> sampler(1);
    sampler[0]
        = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER::D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);

    RootSignatureDesc desc{ rootParams, sampler, RootSignature::Flags::None };
    mGlobalRootSignature = std::make_unique<RootSignature>(mDeviceResource->getDevice(), desc);
}
//ミスシェーダー
{
    std::vector<CD3DX12_ROOT_PARAMETER> params(1);
    params[0].InitAsConstants(
        Framework::Math::MathUtil::alignPow2(sizeof(MissConstant), sizeof(UINT32)), 1);

    RootSignatureDesc desc{ params, {}, RootSignature::Flags::Local };
    mMissLocalRootSignature = std::make_unique<RootSignature>(mDeviceResource->getDevice(), desc);
}
//ヒットグループシェーダー
{
    CD3DX12_DESCRIPTOR_RANGE range[5];
    range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
    range[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
    range[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);
    range[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);

    std::vector<CD3DX12_ROOT_PARAMETER> params(6);
    UINT contSize = Framework::Math::MathUtil::alignPow2(sizeof(HitGroupConstant), sizeof(UINT32));

    params[0].InitAsDescriptorTable(1, &range[0]);
    params[1].InitAsDescriptorTable(1, &range[1]);
    params[2].InitAsDescriptorTable(1, &range[2]);
    params[3].InitAsDescriptorTable(1, &range[3]);
    params[4].InitAsDescriptorTable(1, &range[4]);

    params[5].InitAsConstants(contSize, 1);
    RootSignatureDesc desc{ params, {}, RootSignature::Flags::Local };
    mHitGroupLocalRootSignature
        = std::make_unique<RootSignature>(mDeviceResource->getDevice(), desc);
}
}
{
    mDXRStateObject = std::make_unique<DXRPipelineStateObject>();
    mDXRStateObject->exportShader((void*)g_pMiss, _countof(g_pMiss), MISS_SHADER_NAME);
    mDXRStateObject->exportShader(
        (void*)g_pClosestHit_Normal, _countof(g_pClosestHit_Normal), CLOSEST_HIT_NORMAL_NAME);
    mDXRStateObject->exportShader(
        (void*)g_pClosestHit_Plane, _countof(g_pClosestHit_Plane), CLOSEST_HIT_PLANE_NAME);
    mDXRStateObject->exportShader(
        (void*)g_pClosestHit_Sphere, _countof(g_pClosestHit_Sphere), CLOSEST_HIT_SPHERE_NAME);
    mDXRStateObject->exportShader((void*)g_pRayGenShader, _countof(g_pRayGenShader), RAY_GEN_NAME);
    mDXRStateObject->exportShader((void*)g_pShadow, _countof(g_pShadow), MISS_SHADOW_SHADER_NAME);

    mDXRStateObject->bindHitGroup({ HIT_GROUP_UFO_NAME,
        D3D12_HIT_GROUP_TYPE::D3D12_HIT_GROUP_TYPE_TRIANGLES, CLOSEST_HIT_NORMAL_NAME });
    mDXRStateObject->bindHitGroup({ HIT_GROUP_QUAD_NAME,
        D3D12_HIT_GROUP_TYPE::D3D12_HIT_GROUP_TYPE_TRIANGLES, CLOSEST_HIT_NORMAL_NAME });
    mDXRStateObject->bindHitGroup({ HIT_GROUP_FLOOR_NAME,
        D3D12_HIT_GROUP_TYPE::D3D12_HIT_GROUP_TYPE_TRIANGLES, CLOSEST_HIT_PLANE_NAME });
    mDXRStateObject->bindHitGroup({ HIT_GROUP_SPHERE_NAME,
        D3D12_HIT_GROUP_TYPE::D3D12_HIT_GROUP_TYPE_TRIANGLES, CLOSEST_HIT_SPHERE_NAME });
    {
        CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* config
            = mDXRStateObject->mPipelineStateObjectDesc
                  .CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
        UINT payloadSize
            = Framework::Math::MathUtil::mymax<UINT>({ sizeof(RayPayload), sizeof(ShadowPayload) });
        UINT attrSize = sizeof(float) * 2;
        config->Config(payloadSize, attrSize);
    }
    {
        auto bindLocalRootSignature = [](CD3DX12_STATE_OBJECT_DESC& pipeline,
                                          ID3D12RootSignature* rootSig,
                                          const std::wstring& exportShader) {
            CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT* local
                = pipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            local->SetRootSignature(rootSig);

            CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* asso
                = pipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            asso->SetSubobjectToAssociate(*local);
            asso->AddExport(exportShader.c_str());
        };

        bindLocalRootSignature(mDXRStateObject->mPipelineStateObjectDesc,
            mMissLocalRootSignature->getRootSignature(), MISS_SHADER_NAME);
        bindLocalRootSignature(mDXRStateObject->mPipelineStateObjectDesc,
            mHitGroupLocalRootSignature->getRootSignature(), HIT_GROUP_UFO_NAME);
        bindLocalRootSignature(mDXRStateObject->mPipelineStateObjectDesc,
            mHitGroupLocalRootSignature->getRootSignature(), HIT_GROUP_QUAD_NAME);
        bindLocalRootSignature(mDXRStateObject->mPipelineStateObjectDesc,
            mHitGroupLocalRootSignature->getRootSignature(), HIT_GROUP_FLOOR_NAME);
        bindLocalRootSignature(mDXRStateObject->mPipelineStateObjectDesc,
            mHitGroupLocalRootSignature->getRootSignature(), HIT_GROUP_SPHERE_NAME);
    }
    {
        CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* global
            = mDXRStateObject->mPipelineStateObjectDesc
                  .CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        global->SetRootSignature(mGlobalRootSignature->getRootSignature());
    }
    {
        CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* config
            = mDXRStateObject->mPipelineStateObjectDesc
                  .CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
        UINT maxRecursionDepth = MAX_RAY_RECURSION_DEPTH;
        config->Config(maxRecursionDepth);
    }

    PrintStateObjectDesc(mDXRStateObject->mPipelineStateObjectDesc);
    MY_THROW_IF_FAILED(
        mDXRDevice.getDXRDevice()->CreateStateObject(mDXRStateObject->mPipelineStateObjectDesc,
            IID_PPV_ARGS(&mDXRStateObject->mPipelineStateObject)));
}
{
    using namespace Framework::Desc;
    DescriptorTableDesc desc
        = { L"DescriptorTable", 10000, HeapType::CBV_SRV_UAV, HeapFlag::ShaderVisible };
    mDescriptorTable->create(mDeviceResource->getDevice(), desc);
}

std::array<UINT, LocalRootSignature::HitGroupIndex::Count> mIndexOffsets;
std::array<UINT, LocalRootSignature::HitGroupIndex::Count> mVertexOffsets;
auto getOffset = [&mIndexOffsets, &mVertexOffsets](LocalRootSignature::HitGroupIndex::MyEnum e) {
    if (e == 0)
        return std::make_tuple<UINT, UINT>(0, 0);
    else
        return std::make_tuple(
            std::accumulate(mIndexOffsets.begin(), mIndexOffsets.begin() + e, 0u),
            std::accumulate(mVertexOffsets.begin(), mVertexOffsets.begin() + e, 0u));
};

{
    ID3D12Device* device = mDeviceResource->getDevice();
    std::array<IBuffer, BottomLevelASType::Count> mIndexBuffer;
    std::array<IBuffer, BottomLevelASType::Count> mVertexBuffer;

    std::vector<Framework::DX::Vertex> resourceVertices;
    std::vector<Index> resourceIndices;
    {

        auto path = Framework::Utility::ExePath::getInstance()->exe();
        path = path.remove_filename();
        auto modelPath = path / "Resources" / "Model";
        auto texPath = path / "Resources" / "Texture";

        {
            auto createUnitTexture = [](const Color4& color, const std::wstring& name) {
                Framework::Desc::TextureDesc desc;
                desc.format = Framework::Desc::TextureFormat::R8G8B8A8;
                desc.width = 1;
                desc.height = 1;
                desc.pixels.resize(desc.width * desc.height * 4);
                desc.pixels[0] = static_cast<BYTE>(color.r * 255.0f);
                desc.pixels[1] = static_cast<BYTE>(color.g * 255.0f);
                desc.pixels[2] = static_cast<BYTE>(color.b * 255.0f);
                desc.pixels[3] = static_cast<BYTE>(color.a * 255.0f);
                desc.name = name;
                return desc;
            };
            auto createDefaultTexture
                = [&](const std::wstring& name, ModelTextureType texType, const Color4& col) {
                      std::shared_ptr<Texture2D> texture
                          = std::make_shared<Texture2D>(device, createUnitTexture(col, name));
                      mDescriptorTable->allocate(texture.get());
                      texture->createSRV(device);
                      mTextures[texType] = texture;
                      mTextureIDs[texType] = texType;
                  };

            createDefaultTexture(
                L"DefaultAlbedo", ModelTextureType::Default_AlbedoTexture, Color4(1, 1, 1, 1));
            createDefaultTexture(L"DefaultNormal", ModelTextureType::Default_NormalMap,
                Color4(0.5f, 0.5f, 1.0f, 1.0f));
            createDefaultTexture(L"DefaultMetallicRoughness",
                ModelTextureType::Default_MetallicRoughnessMap, Color4(0, 0, 0, 1));
            createDefaultTexture(
                L"DefaultEmissive", ModelTextureType::Default_EmissiveMap, Color4(0, 0, 0, 1));
            createDefaultTexture(
                L"DefaultOcculusion", ModelTextureType::Default_OcclusionMap, Color4(1, 1, 1, 1));
        }

        auto loadTextures = [&](const GlbMaterial& material,
                                const std::vector<Framework::Desc::TextureDesc>& textureDatas,
                                ModelTextureType idOffset) {
            ModelTextureType nextID = idOffset;
            if (textureDatas.empty()) {
                mTextureIDs[nextID] = mTextureIDs[ModelTextureType::Default_AlbedoTexture];
            } else {
                auto albedo = textureDatas[0];
                auto texture = std::make_shared<Texture2D>(device, albedo);
                mDescriptorTable->allocate(texture.get());
                texture->createSRV(device);
                mTextures[nextID] = texture;
                mTextureIDs[nextID] = nextID;
            }
            nextID = ModelTextureType(nextID + 1);
            if (material.normalMapID == -1) {
                mTextureIDs[nextID] = mTextureIDs[ModelTextureType::Default_NormalMap];
            } else {
                auto normal = textureDatas[material.normalMapID];
                auto texture = std::make_shared<Texture2D>(device, normal);
                mDescriptorTable->allocate(texture.get());
                texture->createSRV(device);
                mTextures[nextID] = texture;
                mTextureIDs[nextID] = nextID;
            }

            nextID = ModelTextureType(nextID + 1);
            if (material.metallicRoughnessMapID == -1) {
                mTextureIDs[nextID] = mTextureIDs[ModelTextureType::Default_MetallicRoughnessMap];
            } else {
                auto metalRough = textureDatas[material.metallicRoughnessMapID];
                auto texture = std::make_shared<Texture2D>(device, metalRough);
                mDescriptorTable->allocate(texture.get());
                texture->createSRV(device);
                mTextures[nextID] = texture;
                mTextureIDs[nextID] = nextID;
            }

            nextID = ModelTextureType(nextID + 1);
            if (material.emissiveMapID == -1) {
                mTextureIDs[nextID] = mTextureIDs[ModelTextureType::Default_EmissiveMap];
            } else {
                auto emissive = textureDatas[material.emissiveMapID];
                auto texture = std::make_shared<Texture2D>(device, emissive);
                mDescriptorTable->allocate(texture.get());
                texture->createSRV(device);
                mTextures[nextID] = texture;
                mTextureIDs[nextID] = nextID;
            }
            nextID = ModelTextureType(nextID + 1);
            if (material.occlusionMapID == -1) {
                mTextureIDs[nextID] = mTextureIDs[ModelTextureType::Default_OcclusionMap];
            } else {
                auto occlusion = textureDatas[material.occlusionMapID];
                auto texture = std::make_shared<Texture2D>(device, occlusion);
                mDescriptorTable->allocate(texture.get());
                texture->createSRV(device);
                mTextures[nextID] = texture;
                mTextureIDs[nextID] = nextID;
            }
        };

        {
            Framework::Utility::GLBLoader loader(
                modelPath / MODEL_NAMES.at(BottomLevelASType::UFO));
            auto indices = toLinearList(loader.getIndicesPerSubMeshes());
            auto vertices = toLinearVertices(loader.getPositionsPerSubMeshes(),
                loader.getNormalsPerSubMeshes(), loader.getUVsPerSubMeshes(),
                loader.getTangentsPerSubMeshes());

            createBuffer(mDeviceResource->getDevice(),
                &mIndexBuffer[BottomLevelASType::UFO].mResource, indices.data(),
                indices.size() * sizeof(indices[0]), L"IndexBuffer");
            createBuffer(mDeviceResource->getDevice(),
                &mVertexBuffer[BottomLevelASType::UFO].mResource, vertices.data(),
                vertices.size() * sizeof(vertices[0]), L"VertexBuffer");

            resourceIndices.insert(resourceIndices.end(), indices.begin(), indices.end());
            resourceVertices.insert(resourceVertices.end(), vertices.begin(), vertices.end());
            mIndexOffsets[LocalRootSignature::HitGroupIndex::UFO] = (UINT)indices.size();
            mVertexOffsets[LocalRootSignature::HitGroupIndex::UFO] = (UINT)vertices.size();

            auto materialList = loader.getMaterialDatas();
            GlbMaterial material = {};
            if (!materialList.empty()) material = loader.getMaterialDatas()[0];
            auto descs = loader.getImageDatas();

            loadTextures(material, descs, ModelTextureType::UFO_AlbedoTexture);
        }
        {
            std::vector<Index> indices = { 0, 1, 2, 0, 2, 3 };
            createBuffer(mDeviceResource->getDevice(),
                &mIndexBuffer[BottomLevelASType::Quad].mResource, indices.data(),
                indices.size() * sizeof(indices[0]), L"IndexBuffer");
            std::vector<Framework::DX::Vertex> vertices = {
                { Vec3(-1, 1, 0), Vec3(0, 0, -1), Vec2(0, 0) },
                { Vec3(1, 1, 0), Vec3(0, 0, -1), Vec2(1, 0) },
                { Vec3(1, -1, 0), Vec3(0, 0, -1), Vec2(1, 1) },
                { Vec3(-1, -1, 0), Vec3(0, 0, -1), Vec2(0, 1) },
            };
            createBuffer(mDeviceResource->getDevice(),
                &mVertexBuffer[BottomLevelASType::Quad].mResource, vertices.data(),
                vertices.size() * sizeof(vertices[0]), L"VertexBuffer");

            resourceIndices.insert(resourceIndices.end(), indices.begin(), indices.end());
            resourceVertices.insert(resourceVertices.end(), vertices.begin(), vertices.end());
            mIndexOffsets[LocalRootSignature::HitGroupIndex::Quad] = (UINT)indices.size();
            mVertexOffsets[LocalRootSignature::HitGroupIndex::Quad] = (UINT)vertices.size();

            Framework::Desc::TextureDesc desc
                = Framework::Utility::TextureLoader::load(texPath / "back2.png");
            auto texture = std::make_shared<Texture2D>(device, desc);
            mDescriptorTable->allocate(texture.get());
            texture->createSRV(device);
            mTextures[ModelTextureType::Quad_AlbedoTexture] = texture;
            mTextureIDs[ModelTextureType::Quad_AlbedoTexture]
                = ModelTextureType::Quad_AlbedoTexture;
            mTextureIDs[ModelTextureType::Quad_NormalMap] = ModelTextureType::Default_NormalMap;
            mTextureIDs[ModelTextureType::Quad_MetallicRoughnessMap]
                = ModelTextureType::Default_MetallicRoughnessMap;
            mTextureIDs[ModelTextureType::Quad_EmissiveMap] = ModelTextureType::Default_EmissiveMap;
            mTextureIDs[ModelTextureType::Quad_OcclusionMap]
                = ModelTextureType::Default_OcclusionMap;
        }

        {
            Framework::Utility::GLBLoader loader(
                modelPath / MODEL_NAMES.at(BottomLevelASType::Floor));
            auto indices = toLinearList(loader.getIndicesPerSubMeshes());
            auto vertices = toLinearVertices(loader.getPositionsPerSubMeshes(),
                loader.getNormalsPerSubMeshes(), loader.getUVsPerSubMeshes());
            createBuffer(mDeviceResource->getDevice(),
                &mIndexBuffer[BottomLevelASType::Floor].mResource, indices.data(),
                indices.size() * sizeof(indices[0]), L"IndexBuffer");
            createBuffer(mDeviceResource->getDevice(),
                &mVertexBuffer[BottomLevelASType::Floor].mResource, vertices.data(),
                vertices.size() * sizeof(vertices[0]), L"VertexBuffer");

            resourceIndices.insert(resourceIndices.end(), indices.begin(), indices.end());
            resourceVertices.insert(resourceVertices.end(), vertices.begin(), vertices.end());
            mIndexOffsets[LocalRootSignature::HitGroupIndex::Floor] = (UINT)indices.size();
            mVertexOffsets[LocalRootSignature::HitGroupIndex::Floor] = (UINT)vertices.size();

            auto materialList = loader.getMaterialDatas();
            GlbMaterial material = {};
            if (!materialList.empty()) material = loader.getMaterialDatas()[0];
            auto descs = loader.getImageDatas();
            loadTextures(material, descs, ModelTextureType::Plane_AlbedoTexture);
        }
        {
            Framework::Utility::GLBLoader loader(
                modelPath / MODEL_NAMES.at(BottomLevelASType::Sphere));
            auto indices = toLinearList(loader.getIndicesPerSubMeshes());
            auto vertices = toLinearVertices(loader.getPositionsPerSubMeshes(),
                loader.getNormalsPerSubMeshes(), loader.getUVsPerSubMeshes(),
                loader.getTangentsPerSubMeshes());

            createBuffer(mDeviceResource->getDevice(),
                &mIndexBuffer[BottomLevelASType::Sphere].mResource, indices.data(),
                indices.size() * sizeof(indices[0]), L"IndexBuffer");
            createBuffer(mDeviceResource->getDevice(),
                &mVertexBuffer[BottomLevelASType::Sphere].mResource, vertices.data(),
                vertices.size() * sizeof(vertices[0]), L"VertexBuffer");

            resourceIndices.insert(resourceIndices.end(), indices.begin(), indices.end());
            resourceVertices.insert(resourceVertices.end(), vertices.begin(), vertices.end());
            mIndexOffsets[LocalRootSignature::HitGroupIndex::Sphere] = (UINT)indices.size();
            mVertexOffsets[LocalRootSignature::HitGroupIndex::Sphere] = (UINT)vertices.size();

            auto materialList = loader.getMaterialDatas();
            GlbMaterial material = {};
            if (!materialList.empty()) material = loader.getMaterialDatas()[0];
            auto descs = loader.getImageDatas();

            loadTextures(material, descs, ModelTextureType::Sphere_AlbedoTexture);
        }
    }
    {
        ID3D12Device* device = mDeviceResource->getDevice();
        ID3D12GraphicsCommandList* commandList = mDeviceResource->getCommandList();
        ID3D12CommandAllocator* allocator = mDeviceResource->getCommandAllocator();
        ID3D12Device5* dxrDevice = mDXRDevice.getDXRDevice();
        ID3D12GraphicsCommandList5* dxrCommandList = mDXRDevice.getDXRCommandList();

        commandList->Reset(allocator, nullptr);

        for (int i = 0; i < BottomLevelASType::Count; i++) {
            D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
            geometryDesc.Type
                = D3D12_RAYTRACING_GEOMETRY_TYPE::D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geometryDesc.Triangles.IndexBuffer = mIndexBuffer[i].mResource->GetGPUVirtualAddress();
            geometryDesc.Triangles.IndexCount
                = static_cast<UINT>(mIndexBuffer[i].mResource->GetDesc().Width) / sizeof(Index);
            geometryDesc.Triangles.IndexFormat = DXGI_FORMAT::DXGI_FORMAT_R16_UINT;
            geometryDesc.Triangles.Transform3x4 = 0;
            geometryDesc.Triangles.VertexBuffer.StartAddress
                = mVertexBuffer[i].mResource->GetGPUVirtualAddress();
            geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Framework::DX::Vertex);
            geometryDesc.Triangles.VertexCount
                = static_cast<UINT>(mVertexBuffer[i].mResource->GetDesc().Width)
                / sizeof(Framework::DX::Vertex);
            geometryDesc.Triangles.VertexFormat = DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT;
            geometryDesc.Flags
                = D3D12_RAYTRACING_GEOMETRY_FLAGS::D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags
                = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS::
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC
            bottomLevelBuildDesc = {};
            auto& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
            bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT::D3D12_ELEMENTS_LAYOUT_ARRAY;
            bottomLevelInputs.Flags = buildFlags;
            bottomLevelInputs.NumDescs = 1;
            bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE::
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            bottomLevelInputs.pGeometryDescs = &geometryDesc;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO
            bottomLevelPreInfo = {};
            dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(
                &bottomLevelInputs, &bottomLevelPreInfo);
            MY_THROW_IF_FALSE(bottomLevelPreInfo.ResultDataMaxSizeInBytes > 0);

            allocateUAVBuffer(device, bottomLevelPreInfo.ScratchDataSizeInBytes,
                &mBLASBuffers[i].scratch,
                D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

            D3D12_RESOURCE_STATES initResourceState
                = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            allocateUAVBuffer(device, bottomLevelPreInfo.ResultDataMaxSizeInBytes,
                &mBLASBuffers[i].buffer, initResourceState, L"BottomLevelAS");

            bottomLevelBuildDesc.ScratchAccelerationStructureData
                = mBLASBuffers[i].scratch->GetGPUVirtualAddress();
            bottomLevelBuildDesc.DestAccelerationStructureData
                = mBLASBuffers[i].buffer->GetGPUVirtualAddress();

            dxrCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
            commandList->ResourceBarrier(
                1, &CD3DX12_RESOURCE_BARRIER::UAV(mBLASBuffers[i].buffer.Get()));
        }
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags
            = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS::
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

        auto& topLevelInputs = topLevelBuildDesc.Inputs;
        topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT::D3D12_ELEMENTS_LAYOUT_ARRAY;
        topLevelInputs.Flags = buildFlags;
        topLevelInputs.NumDescs = TLAS_NUM;
        topLevelInputs.pGeometryDescs = nullptr;
        topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE::
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO
        topLevelPreInfo = {};
        dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(
            &topLevelInputs, &topLevelPreInfo);
        MY_THROW_IF_FALSE(topLevelPreInfo.ResultDataMaxSizeInBytes > 0);

        allocateUAVBuffer(device, topLevelPreInfo.ScratchDataSizeInBytes, &mTLASBuffer.scratch,
            D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

        D3D12_RESOURCE_STATES initResourceState
            = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        allocateUAVBuffer(device, topLevelPreInfo.ResultDataMaxSizeInBytes, &mTLASBuffer.buffer,
            initResourceState, L"TopLevelAS");

        topLevelBuildDesc.DestAccelerationStructureData
            = mTLASBuffer.buffer->GetGPUVirtualAddress();
        topLevelBuildDesc.ScratchAccelerationStructureData
            = mTLASBuffer.scratch->GetGPUVirtualAddress();

        mDeviceResource->executeCommandList();
        mDeviceResource->waitForGPU();

        createBuffer(device, &mResourcesIndexBuffer.mResource, resourceIndices.data(),
            resourceIndices.size() * sizeof(resourceIndices[0]), L"ResourceIndex");
        createBuffer(device, &mResourcesVertexBuffer.mResource, resourceVertices.data(),
            resourceVertices.size() * sizeof(resourceVertices[0]), L"ResourceVertex");

        {
            UINT indexBufferSize
                = (UINT)resourceIndices.size() * sizeof(resourceIndices[0]) / sizeof(float);
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.NumElements = indexBufferSize;
            srvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R32_TYPELESS;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAGS::D3D12_BUFFER_SRV_FLAG_RAW;
            srvDesc.Buffer.StructureByteStride = 0;
            mDescriptorTable->allocate(&mResourcesIndexBuffer);
            device->CreateShaderResourceView(
                mResourcesIndexBuffer.mResource.Get(), &srvDesc, mResourcesIndexBuffer.mCPUHandle);
        }
        {
            UINT vertexBufferSize = (UINT)resourceVertices.size();
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.NumElements = vertexBufferSize;
            srvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAGS::D3D12_BUFFER_SRV_FLAG_NONE;
            srvDesc.Buffer.StructureByteStride = sizeof(resourceVertices[0]);
            mDescriptorTable->allocate(&mResourcesVertexBuffer);
            device->CreateShaderResourceView(mResourcesVertexBuffer.mResource.Get(), &srvDesc,
                mResourcesVertexBuffer.mCPUHandle);
        }
    }
}
{
    ID3D12Device* device = mDeviceResource->getDevice();
    ComPtr<ID3D12StateObjectProperties> stateObjectProp;
    MY_THROW_IF_FAILED(mDXRStateObject->mPipelineStateObject.As(&stateObjectProp));
    void* rayGenShaderID = stateObjectProp->GetShaderIdentifier(RAY_GEN_NAME.c_str());
    void* missShaderID = stateObjectProp->GetShaderIdentifier(MISS_SHADER_NAME.c_str());
    void* missShadowShaderID
        = stateObjectProp->GetShaderIdentifier(MISS_SHADOW_SHADER_NAME.c_str());
    void* hitGroup_SphereShaderID
        = stateObjectProp->GetShaderIdentifier(HIT_GROUP_UFO_NAME.c_str());
    void* hitGroup_QuadShaderID = stateObjectProp->GetShaderIdentifier(HIT_GROUP_QUAD_NAME.c_str());
    void* hitGroup_FloorShaderID
        = stateObjectProp->GetShaderIdentifier(HIT_GROUP_FLOOR_NAME.c_str());
    UINT shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    {
        UINT num = 1;
        UINT recordSize = shaderIDSize;
        ShaderTable table(device, num, recordSize, L"RayGenShaderTable");
        table.push_back(ShaderRecord(rayGenShaderID, shaderIDSize));
        mRayGenTable = table.getResource();
    }
    {
        struct RootArgument {
            MissConstant cb;
        } rootArgument;
        rootArgument.cb.back = Color(0.1f, 0.2f, 0.3f, 1);
        UINT num = 2;
        UINT recordSize = shaderIDSize + sizeof(RootArgument);
        ShaderTable table(device, num, recordSize, L"MissShaderTable");
        table.push_back(
            ShaderRecord(missShaderID, shaderIDSize, &rootArgument, sizeof(RootArgument)));
        table.push_back(ShaderRecord(missShadowShaderID, shaderIDSize));
        mMissTable = table.getResource();
        mMissStride = table.getShaderRecordSize();
    }
    {
        struct RootArgument {
            D3D12_GPU_DESCRIPTOR_HANDLE albedo;
            D3D12_GPU_DESCRIPTOR_HANDLE normal;
            D3D12_GPU_DESCRIPTOR_HANDLE metallicRoughness;
            D3D12_GPU_DESCRIPTOR_HANDLE emissive;
            D3D12_GPU_DESCRIPTOR_HANDLE occlusion;
            HitGroupConstant cb;
        } rootArguments;

        auto setRootArgumentTexture = [&](RootArgument& root, ModelTextureType offset) {
            root.albedo = mTextures[mTextureIDs[offset]]->getGPUHandle();
            root.normal = mTextures[mTextureIDs[ModelTextureType(offset + 1)]]->getGPUHandle();
            root.metallicRoughness
                = mTextures[mTextureIDs[ModelTextureType(offset + 2)]]->getGPUHandle();
            root.emissive = mTextures[mTextureIDs[ModelTextureType(offset + 3)]]->getGPUHandle();
            root.occlusion = mTextures[mTextureIDs[ModelTextureType(offset + 4)]]->getGPUHandle();
        };
        UINT num = 4;
        UINT recordSize = shaderIDSize + sizeof(RootArgument);
        ShaderTable table(device, num, recordSize, L"HitGroupShaderTable");
        {
            rootArguments.cb.indexOffset
                = std::get<0>(getOffset(LocalRootSignature::HitGroupIndex::UFO));
            rootArguments.cb.vertexOffset
                = std::get<1>(getOffset(LocalRootSignature::HitGroupIndex::UFO));
            setRootArgumentTexture(rootArguments, ModelTextureType::UFO_AlbedoTexture);
            table.push_back(ShaderRecord(
                hitGroup_SphereShaderID, shaderIDSize, &rootArguments, sizeof(RootArgument)));
        }
        {
            rootArguments.cb.indexOffset
                = std::get<0>(getOffset(LocalRootSignature::HitGroupIndex::Quad));
            rootArguments.cb.vertexOffset
                = std::get<1>(getOffset(LocalRootSignature::HitGroupIndex::Quad));
            setRootArgumentTexture(rootArguments, ModelTextureType::Quad_AlbedoTexture);
            table.push_back(ShaderRecord(
                hitGroup_QuadShaderID, shaderIDSize, &rootArguments, sizeof(RootArgument)));
        }
        {
            rootArguments.cb.indexOffset
                = std::get<0>(getOffset(LocalRootSignature::HitGroupIndex::Floor));
            rootArguments.cb.vertexOffset
                = std::get<1>(getOffset(LocalRootSignature::HitGroupIndex::Floor));
            setRootArgumentTexture(rootArguments, ModelTextureType::Plane_AlbedoTexture);
            table.push_back(ShaderRecord(
                hitGroup_FloorShaderID, shaderIDSize, &rootArguments, sizeof(RootArgument)));
        }
        {
            rootArguments.cb.indexOffset
                = std::get<0>(getOffset(LocalRootSignature::HitGroupIndex::Sphere));
            rootArguments.cb.vertexOffset
                = std::get<1>(getOffset(LocalRootSignature::HitGroupIndex::Sphere));
            setRootArgumentTexture(rootArguments, ModelTextureType::Sphere_AlbedoTexture);
            table.push_back(ShaderRecord(
                hitGroup_FloorShaderID, shaderIDSize, &rootArguments, sizeof(RootArgument)));
        }
        mHitGroupStride = table.getShaderRecordSize();
        mHitGroupTable = table.getResource();
    }
}
}

void Scene::releaseDeviceDependentResources() {}

void Scene::createWindowDependentResources() {
    ID3D12Device* device = mDeviceResource->getDevice();
    DXGI_FORMAT backBufferFormat = mDeviceResource->getBackBufferFormat();

    auto uavResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat, mWidth, mHeight, 1, 1, 1,
        0, D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT);
    MY_THROW_IF_FAILED(
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE,
            &uavResourceDesc, D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
            IID_PPV_ARGS(&mRaytracingOutput.mResource)));
    mRaytracingOutput.mResource->SetName(L"RaytracingOutput");

    mDescriptorTable->allocate(&mRaytracingOutput);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION::D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(
        mRaytracingOutput.mResource.Get(), nullptr, &uavDesc, mRaytracingOutput.mCPUHandle);
}

void Scene::releaseWindowDependentResources() {
    mRaytracingOutput.mResource.Reset();
}
