#pragma once

namespace Framework::Device {
    /**
    * @class ISystemEventNotify
    * @brief �V�X�e���̃C�x���g�ʒm�C���^�[�t�F�[�X
    */
    class ISystemEventNotify {
    public:
        /**
        * @brief �f�X�g���N�^
        */
        virtual ~ISystemEventNotify() = default;
        /**
        * @brief ������
        */
        virtual void onInit() = 0;
        /**
        * @brief �X�V
        */
        virtual void onUpdate() = 0;
        /**
        * @brief �`��
        */
        virtual void onRender() = 0;
        /**
        * @brief �j��
        */
        virtual void onDestroy() = 0;
        /**
        * @brief �t���X�N���[�����[�h�̐؂�ւ�
        */
        virtual void toggleFullScreenWindow() = 0;
        virtual void updateForSizeChange(UINT clientWidth, UINT cliendHeight) = 0;
        virtual void setWindowBounds(const RECT& rect) = 0;
        virtual void onSizeChanged(UINT width, UINT height, bool minimized) = 0;
        virtual void onWindowMoved(int x, int y) = 0;
    };
} //Framework::Device