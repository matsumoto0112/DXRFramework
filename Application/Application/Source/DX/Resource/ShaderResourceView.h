/**
 * @file ShaderResourceView.h
 * @brief シェーダーリソースビュークラス
 */

#pragma once
#include "DX/Descriptor/DescriptorHeapFlag.h"
#include "DX/Descriptor/DescriptorInfo.h"
#include "DX/Resource/Buffer.h"

namespace Framework::DX {
    class DeviceResource;
    /**
     * @class ShaderResourceView
     * @brief シェーダーリソースビュー
     */
    class ShaderResourceView {
    public:
        /**
         * @brief コンストラクタ
         */
        ShaderResourceView() {}
        /**
         * @brief デストラクタ
         */
        ~ShaderResourceView() {}
        void initAsTexture2D(DeviceResource* device, const Buffer& buffer, DXGI_FORMAT format,
            DescriptorHeapType heapFlag);
        void initAsBuffer(
            DeviceResource* device, const Buffer& buffer, DescriptorHeapType heapFlag);
        void initAsRawBuffer(DeviceResource* device, const Buffer& buffer, UINT numElements,
            DescriptorHeapType heapFlag);
        void initAsRaytracingAccelerationStructure(
            DeviceResource* device, const Buffer& buffer, DescriptorHeapType heapFlag);
        const DescriptorInfo& getInfo() const {
            return mInfo;
        }

    private:
        void createShaderResourceView(DeviceResource* device, ID3D12Resource* resource,
            const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, DescriptorHeapType flag);

    private:
        DescriptorInfo mInfo;
    };
} // namespace Framework::DX
