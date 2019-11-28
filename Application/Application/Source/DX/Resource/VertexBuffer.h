/**
 * @file VertexBuffer.h
 * @brief 頂点バッファ
 */

#pragma once
#include "DX/Resource/IBuffer.h"

namespace Framework::DX {
    /**
     * @class VertexBuffer
     * @brief 頂点バッファ
     */
    class VertexBuffer : public IBuffer {
    public:
        /**
         * @brief コンストラクタ
         * @tparam T 頂点構造体
         * @param device デバイス
         * @param vertices 頂点データ
         * @param name リソース名
         */
        template <class T>
        VertexBuffer(
            ID3D12Device* device, const std::vector<T>& vertices, const std::wstring& name);
        /**
         * @brief デストラクタ
         */
        ~VertexBuffer();
        /**
         * @brief シェーダーリソースビューを作成する
         * @param device デバイス
         */
        void createSRV(ID3D12Device* device);

    private:
        const UINT mVertexByteLength; //!< 頂点の大きさ
        const UINT mVertexCount; //!< 頂点数
    };
    template <class T>
    inline VertexBuffer::VertexBuffer(
        ID3D12Device* device, const std::vector<T>& vertices, const std::wstring& name)
        : mVertexByteLength(sizeof(T)), mVertexCount(static_cast<UINT>(vertices.size())) {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc
            = CD3DX12_RESOURCE_DESC::Buffer(mVertexByteLength * mVertexCount);
        MY_THROW_IF_FAILED(
            device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE,
                &bufferDesc, D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&mResource)));
        MY_THROW_IF_FAILED(mResource->SetName(name.c_str()));

        void* mapped;
        MY_THROW_IF_FAILED(mResource->Map(0, nullptr, &mapped));
        memcpy(mapped, vertices.data(), mVertexByteLength * mVertexCount);
        mResource->Unmap(0, nullptr);
    }
} // namespace Framework::DX
