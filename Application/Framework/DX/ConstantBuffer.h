#pragma once
#include "DX/Util/GPUUploadBuffer.h"
#include "DX/Util/DXHelper.h"
#include "Utility/Debug.h"

namespace Framework::DX {
    /**
    * @class ConstantBuffer
    * @tparam T 使用する構造体
    * @brief コンスタントバッファ
    */
    template <class T>
    class ConstantBuffer : public GPUUploadBuffer {
    public:
        /**
        * @brief コンストラクタ
        */
        ConstantBuffer();
        /**
        * @brief デストラクタ
        */
        ~ConstantBuffer();
        /**
        * @brief バッファを作成する
        * @param device デバイス
        * @param numInstances 総インスタンス数
        * @param resourceName リソース名
        */
        void create(ID3D12Device* device, UINT numInstances = 1, LPCWSTR resourceName = nullptr);
        /**
        * @brief ステージングの内容をGPUにコピーする
        * @param instanceIndex コピーするインデックス
        */
        void copyStatingToGPU(UINT instanceIndex = 0);
        /**
        * @brief インスタンス数を取得する
        */
        UINT numInstances()const { return mNumInstances; }
        /**
        * @brief GPUアドレスを取得する
        * @param instanceIndex 取得するインデックス
        */
        D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress(UINT instanceIndex = 0);
        /**
        * @brief アロー演算子
        * @details ステージングの内容にアクセスしやすくするため
        */
        T* operator->() { return &mStaging; }
    private:
        uint8_t* mMappedConstantData; //!< メモリの書き込み範囲
        UINT mAlignedInstanceSize; //!< アラインメントされたバッファサイズ
        UINT mNumInstances; //!< インスタンス数
        T mStaging; //!< コンスタントバッファの値
    };

    //コンストラクタ
    template<class T>
    inline ConstantBuffer<T>::ConstantBuffer()
        :mMappedConstantData(nullptr), mAlignedInstanceSize(0), mNumInstances(0) { }
    //デストラクタ
    template<class T>
    inline ConstantBuffer<T>::~ConstantBuffer() { }
    //バッファの作成
    template<class T>
    inline void ConstantBuffer<T>::create(ID3D12Device* device, UINT numInstances, LPCWSTR resourceName) {
        mNumInstances = numInstances;
        mAlignedInstanceSize = calcConstantBufferByteSize(sizeof(T));
        UINT bufferSize = mNumInstances * mAlignedInstanceSize;
        allocate(device, bufferSize, resourceName);
        mMappedConstantData = getMapCPUWriteOnly();
    }
    //GPUにデータをコピーする
    template<class T>
    inline void ConstantBuffer<T>::copyStatingToGPU(UINT instanceIndex) {
        MY_ASSERTION(instanceIndex < mNumInstances, "インデックスが範囲外です");
        memcpy(mMappedConstantData + instanceIndex * mAlignedInstanceSize, &mStaging, sizeof(T));
    }
    //GPUアドレスの取得
    template<class T>
    inline D3D12_GPU_VIRTUAL_ADDRESS ConstantBuffer<T>::gpuVirtualAddress(UINT instanceIndex) {
        MY_ASSERTION(instanceIndex < mNumInstances, "インデックスが範囲外です");
        return mResource->GetGPUVirtualAddress() + instanceIndex * mAlignedInstanceSize;
    }


} //Framework::DX
