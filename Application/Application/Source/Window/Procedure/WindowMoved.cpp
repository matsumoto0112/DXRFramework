#include "WindowMoved.h"
#include "Device/ISystemEventNotify.h"

namespace Framework::Window {

    LRESULT WindowMoved::wndProc(
        HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, bool* isReturn) {
        //WM_MOVEのみを処理する
        if (msg != WM_MOVE) return 0L;
        Device::ISystemEventNotify* notify
            = reinterpret_cast<Device::ISystemEventNotify*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        if (!notify) return 0L;
        *isReturn = true;

        RECT windowRect = {};
        GetWindowRect(hWnd, &windowRect);
        notify->setWindowBounds(windowRect);

        //新しい座標を求める
        int xPos = (int)(short)LOWORD(lParam);
        int yPos = (int)(short)HIWORD(lParam);
        notify->onWindowMoved(xPos, yPos);

        return 0L;
    }
} // namespace Framework::Window
