#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <cstdio>

// 计算器状态
struct CalculatorState {
    char display[32] = "0";
    float current = 0;
    float operand = 0;
    char operation = 0;
    bool reset_display = false;
};

// DirectX 11全局变量
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// DirectX 11辅助函数
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 计算器逻辑函数
void UpdateCalculator(CalculatorState& state, const char* btn);

int main(int, char**)
{
    // 创建窗口
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, _T("iOS Calculator"), nullptr };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("iOS Style Calculator"), WS_OVERLAPPEDWINDOW, 100, 100, 320, 520, nullptr, nullptr, wc.hInstance, nullptr);

    // 初始化Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // 显示窗口
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // 初始化ImGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // 设置iOS风格
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 12.0f;
    style.GrabRounding = 12.0f;
    style.WindowRounding = 12.0f;
    style.Colors[ImGuiCol_Button] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

    // 初始化平台/Renderer后端
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // 计算器状态
    CalculatorState state;

    // 主循环
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // 开始ImGUI帧
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 创建计算器窗口
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(320, 520));
            ImGui::Begin("Calculator", nullptr, 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove);

            // 显示区域
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
            ImGui::BeginChild("Display", ImVec2(0, 100), true);
            ImGui::SetWindowFontScale(2.0f);
            ImGui::Text("%s", state.display);
            ImGui::EndChild();
            ImGui::PopStyleColor();

            // 按钮布局
            const char* buttons[5][4] = {
                {"C", "+/-", "%", "/"},
                {"7", "8", "9", "x"},
                {"4", "5", "6", "-"},
                {"1", "2", "3", "+"},
                {"0", ".", "=", "="}
            };

            // 绘制按钮
            for (int row = 0; row < 5; row++) {
                ImGui::Columns(4, nullptr, false);
                for (int col = 0; col < 4; col++) {
                    if (buttons[row][col][0] == '=' && col == 3) {
                        ImGui::NextColumn();
                        ImGui::NextColumn();
                        ImGui::NextColumn();
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.0f, 1.0f));
                    }
                    if (ImGui::Button(buttons[row][col], ImVec2(70, 70))) {
                        UpdateCalculator(state, buttons[row][col]);
                    }
                    if (buttons[row][col][0] == '=' && col == 3) {
                        ImGui::PopStyleColor(3);
                    }
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }

            ImGui::End();
        }

        // 渲染
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.9f, 0.9f, 0.9f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // 清理
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// 计算器逻辑实现
void UpdateCalculator(CalculatorState& state, const char* btn)
{
    if (strcmp(btn, "C") == 0) {
        strcpy(state.display, "0");
        state.current = 0;
        state.operand = 0;
        state.operation = 0;
        return;
    }

    if (strcmp(btn, "+/-") == 0) {
        state.current = -atof(state.display);
        sprintf(state.display, "%.6g", state.current);
        return;
    }

    if (strcmp(btn, "%") == 0) {
        state.current = atof(state.display) / 100.0f;
        sprintf(state.display, "%.6g", state.current);
        return;
    }

    if (btn[0] >= '0' && btn[0] <= '9') {
        if (strcmp(state.display, "0") == 0 || state.reset_display) {
            strcpy(state.display, btn);
            state.reset_display = false;
        } else {
            strcat(state.display, btn);
        }
        state.current = atof(state.display);
        return;
    }

    if (strcmp(btn, ".") == 0) {
        if (strchr(state.display, '.') == nullptr) {
            strcat(state.display, ".");
        }
        return;
    }

    if (btn[0] == '+' || btn[0] == '-' || btn[0] == 'x' || btn[0] == '/') {
        state.operand = atof(state.display);
        state.operation = btn[0];
        state.reset_display = true;
        return;
    }

    if (strcmp(btn, "=") == 0) {
        float operand2 = atof(state.display);
        switch (state.operation) {
            case '+': state.current = state.operand + operand2; break;
            case '-': state.current = state.operand - operand2; break;
            case 'x': state.current = state.operand * operand2; break;
            case '/': state.current = state.operand / operand2; break;
            default: return;
        }
        sprintf(state.display, "%.6g", state.current);
        state.reset_display = true;
        state.operation = 0;
    }
}

// DirectX辅助函数
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
