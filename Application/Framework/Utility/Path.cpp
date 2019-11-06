#include "Path.h"
#include <Windows.h>

namespace {
    constexpr int MAX_SIZE = 1024;
    //���s�t�@�C���܂ł̃p�X���擾����
    inline std::wstring getExePath() {
        wchar_t path[MAX_SIZE];
        DWORD ret = GetModuleFileName(nullptr, path, sizeof(path));
        std::wstring res(path);
        return res;
    }
}

namespace Framework::Utility {
    //�R���X�g���N�^
    ExePath::ExePath() {
        mExe = getExePath();
    }
    //�f�X�g���N�^
    ExePath::~ExePath() { }
} //Framework::Utility 