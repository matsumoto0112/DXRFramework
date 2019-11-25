#include "Keyboard.h"
#include "KeyCode.h"

namespace Framework::Input {
    //�R���X�g���N�^
    Keyboard::Keyboard(HWND hWnd)
        :mCurrentKeys(), mPrevKeys() { }
    //�f�X�g���N�^
    Keyboard::~Keyboard() { }
    //�X�V
    void Keyboard::update() {
        //�O�t���[���̃L�[�̏����R�s�[����
        std::copy(mCurrentKeys.begin(), mCurrentKeys.end(), mPrevKeys.begin());
        //���݂̃L�[�̉�����Ԃ��擾����
        GetKeyboardState(mCurrentKeys.data());
    }
    //�L�[�̉������̎擾
    bool Keyboard::getKey(KeyCode key) const {
        return checkKeyDown(mCurrentKeys, key);
    }
    //�L�[�̉������u�Ԃ��ǂ������擾
    bool Keyboard::getKeyDown(KeyCode key) const {
        bool prev = checkKeyDown(mPrevKeys, key);
        bool cur = checkKeyDown(mCurrentKeys, key);
        return !prev && cur;
    }
    //�L�[�̗������u�Ԃ��ǂ������擾
    bool Keyboard::getKeyUp(KeyCode key) const {
        bool prev = checkKeyDown(mPrevKeys, key);
        bool cur = checkKeyDown(mCurrentKeys, key);
        return prev && !cur;
    }
    //�L�[��������Ă��邩�ǂ�������
    bool Keyboard::checkKeyDown(const KeyInfo& keys, KeyCode key) const {
        return (keys[key] & 0x80) != 0;
    }

} //Framework::Input