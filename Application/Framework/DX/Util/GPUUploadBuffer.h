#pragma once
#include <d3d12.h>
#include "Libs/d3dx12.h"
#include "Utility/Typedef.h"

namespace Framework::DX {
    /**
    * @class GPUUploadBuffer
    * @brief GPUにアップロードするバッファ
    */
    class GPUUploadBuffer {
    public:
        /**
        * @brief コンストラクタ
        */
        GPUUploadBuffer();
        /**
        * @brief デストラクタ
        */
        virtual ~GPUUploadBuffer();
        /**
        * @brief メモリのアロケート処理
        */
        void allocate(ID3D12Device* device, UINT bufferSize, LPCWSTR resourceName = nullptr);
        /**
        * @brief CPU書き込み専用のメモリを返す
        */
        uint8_t* getMapCPUWriteOnly();
    protected:
        ComPtr<ID3D12Resource> mResource;
    };
} //Framework::DX
