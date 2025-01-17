#include "DestroyProc.h"

namespace Framework::Window {
    //プロシージャ
    LRESULT DestroyProc::wndProc(
        HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, bool* isReturn) {
        if (msg != WM_DESTROY) return 0L;
        *isReturn = true;
        //終了指令を発行する
        PostQuitMessage(0);
        return 0L;
    }
} // namespace Framework::Window
