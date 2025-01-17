#pragma once
#include "Assets/Shader/Raytracing/Util/GlobalCompat.h"
#include "DX/DescriptorTable.h"
#include "DX/DeviceResource.h"
#include "DX/Raytracing/BottomLevelAccelerationStructure.h"
#include "DX/Raytracing/DXRDevice.h"
#include "DX/Raytracing/DXRPipelineStateObject.h"
#include "DX/Raytracing/TopLevelAccelerationStructure.h"
#include "DX/Resource/ConstantBuffer.h"
#include "DX/Resource/IndexBuffer.h"
#include "DX/Resource/ShaderResourceView.h"
#include "DX/Resource/Texture2D.h"
#include "DX/Resource/UnorderedAccessView.h"
#include "DX/Resource/VertexBuffer.h"
#include "DX/Shader/RootSignature.h"
#include "Define.h"
#include "Device/ISystemEventNotify.h"
#include "Input/InputManager.h"
#include "Utility/GPUTimer.h"
#include "Utility/Time.h"

namespace DescriptorIndex {
    enum MyEnum {
        RaytracingOutput,
        IndexBuffer,
        VertexBuffer,
    };
}

namespace ModelType {
    enum Enum {
        UFO,
        Floor,
        Sphere,
        House,
        Tree,
        Crate,

        Count,
    };
}

class Scene {
public:
    Scene(Framework::DX::DeviceResource* device, Framework::Input::InputManager* inputManager,
        UINT width, UINT height);
    ~Scene();
    void create();
    void reset();
    void update();
    void render();
    void onWindowSizeChanged(UINT width, UINT height);

private:
    void createDeviceDependentResources();
    void releaseDeviceDependentResources();
    void createWindowDependentResources();
    void releaseWindowDependentResources();

private:
    Framework::DX::DeviceResource* mDeviceResource;
    Framework::Input::InputManager* mInputManager;
    Framework::DX::DXRDevice mDXRDevice;
    std::unordered_map<ModelType::Enum,
        std::unique_ptr<Framework::DX::BottomLevelAccelerationStructure>>
        mBLASBuffers;
    std::unique_ptr<Framework::DX::TopLevelAccelerationStructure> mTLASBuffer;
    Framework::DX::ConstantBuffer<SceneConstantBuffer> mSceneCB;

private:
    std::unique_ptr<Framework::DX::RootSignature> mGlobalRootSignature;
    std::unique_ptr<Framework::DX::RootSignature> mMissLocalRootSignature;
    std::unique_ptr<Framework::DX::RootSignature> mHitGroupLocalRootSignature;
    Framework::DX::IndexBuffer mResourcesIndexBuffer;
    Framework::DX::ShaderResourceView mResourceIndexBufferSRV;
    Framework::DX::VertexBuffer mResourcesVertexBuffer;
    Framework::DX::ShaderResourceView mResourceVertexBufferSRV;
    Framework::DX::Buffer mRaytracingOutput;
    Framework::DX::UnorderedAccessView mRaytracingOutputUAV;

    std::unique_ptr<Framework::DX::DXRPipelineStateObject> mDXRStateObject;

private:
    UINT mWidth;
    UINT mHeight;
    Framework::Utility::Time mTime;
    Framework::Utility::GPUTimer mGpuTimer;
    Vec3 mCameraRotation;
    Color mLightAmbient;
};
