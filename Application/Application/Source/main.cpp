#include "Framework/Game.h"
#include "Utility/Debug.h"
#include "Framework/ImGui/ImGuiManager.h"

/**
* @class MainApp
* @brief メインアプリケーション
*/
class MainApp : public Framework::Game {
public:
    /**
    * @brief コンストラクタ
    */
    MainApp(UINT width, UINT height, const std::wstring& title)
        :Game(width, height, title) { }
    /**
    * @brief デストラクタ
    */
    ~MainApp() { }
    void onInit() override {
        Game::onInit();
        mDebugWindow = std::make_unique<Framework::ImGUI::Window>("Debug");
        mFPSText = std::make_shared<Framework::ImGUI::Text>("FPS:");
        mDebugWindow->addItem(mFPSText);

    }
    void onUpdate() override {
        Game::onUpdate();
    }
    void onRender() override {
        Game::onRender();

        mDeviceResource->prepare();
        Framework::ImGuiManager::getInstance()->beginFrame();
        ID3D12GraphicsCommandList* list = mDeviceResource->getCommandList();
        D3D12_CPU_DESCRIPTOR_HANDLE rtv[] = { mDeviceResource->getRenderTargetView() };
        list->OMSetRenderTargets(1, rtv, FALSE, nullptr);
        static float color[4] = { 0.0f,0.0f,0.0f,0.0f };
        list->ClearRenderTargetView(mDeviceResource->getRenderTargetView(), color, 0, nullptr);

        mDebugWindow->draw();

        Framework::ImGuiManager::getInstance()->endFrame(mDeviceResource->getCommandList());

        mDeviceResource->present();
    }
    void onDestroy()override {
        Game::onDestroy();
    }
private:
    std::unique_ptr<Framework::ImGUI::Window> mDebugWindow;
    std::shared_ptr<Framework::ImGUI::Text> mFPSText;
};

//メイン関数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    MainApp app(1280, 720, L"Game");
    return app.run(hInstance, nCmdShow);
}
