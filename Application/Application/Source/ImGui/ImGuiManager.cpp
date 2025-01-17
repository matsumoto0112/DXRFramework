#include "ImGuiManager.h"

namespace Framework {
    //コンストラクタ
    ImGuiManager::ImGuiManager(
        HWND hWnd, ID3D12Device* device, DXGI_FORMAT format, UINT frameCount, bool enableImGui)
        : mEnableImGui(enableImGui), mHeap(nullptr) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(hWnd);

        //ImGui用のヒープ確保
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap));

        ImGui_ImplDX12_Init(device, frameCount, format, mHeap->GetCPUDescriptorHandleForHeapStart(),
            mHeap->GetGPUDescriptorHandleForHeapStart());
    }

    //デストラクタ
    ImGuiManager::~ImGuiManager() {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiManager::beginFrame() {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiManager::endFrame(ID3D12GraphicsCommandList* commandList) {
        if (mEnableImGui) {
            commandList->SetDescriptorHeaps(1, mHeap.GetAddressOf());
            ImGui::Render();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
        }
        ImGui::EndFrame();
    }

} // namespace Framework
