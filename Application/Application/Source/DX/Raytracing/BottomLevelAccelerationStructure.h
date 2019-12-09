/**
 * @file BottomLevelAccelerationStructure.h
 * @brief �{�g�����x����AS
 * @details �W�I���g���̌`���`
 */

#pragma once
#include "DX/Raytracing/DXRDevice.h"
#include "DX/Resource/IndexBuffer.h"
#include "DX/Resource/VertexBuffer.h"

namespace Framework::DX {
    /**
     * @class BottomLevelAccelerationStructure
     * @brief �W�I���g���̌`����`����AS
     */
    class BottomLevelAccelerationStructure {
    public:
        /**
         * @brief �R���X�g���N�^
         */
        BottomLevelAccelerationStructure() {}
        /**
         * @brief �f�X�g���N�^
         */
        ~BottomLevelAccelerationStructure() {}
        /**
         * @brief ������
         * @param device DXR�p�f�o�C�X
         * @param vertexBuffer ���_�o�b�t�@
         * @param vertexSize ���_�̃o�C�g�T�C�Y
         * @param indexBuffer �C���f�b�N�X�o�b�t�@
         * @param indexSize �C���f�b�N�X�̃o�C�g�T�C�Y
         */
        void init(const DXRDevice& device, const VertexBuffer& vertexBuffer, UINT vertexSize,
            const IndexBuffer& indexBuffer, UINT indexSize);
        /**
         * @brief �o�b�t�@�̎擾
         */
        ID3D12Resource* getBuffer() const {
            return mBuffer.Get();
        }

    private:
        Comptr<ID3D12Resource> mScratch;
        Comptr<ID3D12Resource> mBuffer;
    };
} // namespace Framework::DX