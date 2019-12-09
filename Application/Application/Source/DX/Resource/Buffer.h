/**
 * @file Buffer.h
 * @brief バッファクラス
 */

#pragma once
#include "DX/DeviceResource.h"

namespace Framework::DX {
    /**
     * @class Buffer
     * @brief バッファクラス
     */
    class Buffer {
    public:
        /**
         * @enum Usage
         * @brief バッファの用途
         */
        enum class Usage {
            ConstantBuffer,
            VertexBuffer,
            IndexBuffer,
            ShaderResource,
        };

    public:
        /**
         * @brief コンストラクタ
         */
        Buffer() {}
        /**
         * @brief デストラクタ
         */
        ~Buffer();
        /**
         * @brief 通常のバッファとして初期化
         */
        void init(
            ID3D12Device* device, Usage usage, UINT size, UINT stride, const std::wstring& name);
        /**
         * @brief テクスチャ2Dとして初期化
         */
        void init(ID3D12Device* device, Usage usage, DXGI_FORMAT format, UINT width, UINT height,
            const std::wstring& name);
        /**
         * @brief メモリのマップ処理
         * @details メモリの書き込み領域の先頭を返す
         */
        void* map();
        /**
         * @brief メモリのアンマップ
         */
        void unmap();
        /**
         * @brief リソースの書き込み
         */
        void writeResource(const void* data, UINT size);
        ID3D12Resource* getResource() const {
            return mResource.Get();
        }
        Usage getResourceType() const {
            return mResourceType;
        }
        UINT getSize() const {
            return mSize;
        }
        UINT getStride() const {
            return mStride;
        }

    private:
        Comptr<ID3D12Resource> mResource{ nullptr };
        Usage mResourceType = Usage::ConstantBuffer;
        D3D12_RESOURCE_STATES mCurrentState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
        UINT mSize = 0;
        UINT mStride = 0;
        void* mMapped = nullptr;
    };
} // namespace Framework::DX
