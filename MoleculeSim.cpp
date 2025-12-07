// moleculesim.cpp - Molecule viewer using WebView2 + PubChem + OpenBabel 3D

#define UNICODE
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <string>
#include <sstream>

#include "Simulation.h"
#include "PubChemClient.h"
#include "RDKitBridge.h"

using Microsoft::WRL::ComPtr;

// Globals
static HWND g_hWnd = nullptr;
static ComPtr<ICoreWebView2Controller> g_controller;
static ComPtr<ICoreWebView2> g_webview;
static bool g_webReady = false;
static SimulationModel g_sim;

static const UINT_PTR TIMER_ID = 1;
static const UINT TIMER_INTERVAL_MS = 50;

// Helpers
static std::wstring GetExeDir() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring base(exePath);
    size_t pos = base.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : base.substr(0, pos);
}

static void SendCurrentFrame() {
    if (!g_webview || !g_webReady) return;
    std::wstring json = g_sim.BuildJsonFrame();
    g_webview->PostWebMessageAsJson(json.c_str());
}

static std::wstring JsonEscape(const std::wstring& w) {
    std::wstringstream o;
    for (wchar_t c : w) {
        switch (c) {
        case L'\"': o << L"\\\""; break;
        case L'\\': o << L"\\\\"; break;
        case L'\n': o << L"\\n"; break;
        case L'\r': o << L"\\r"; break;
        case L'\t': o << L"\\t"; break;
        default:    o << c;     break;
        }
    }
    return o.str();
}

// WebView init + message handling
static void InitWebView(HWND hWnd) {
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hWnd](HRESULT result, ICoreWebView2Environment* env)->HRESULT {
                if (result != S_OK || !env) return result;

                env->CreateCoreWebView2Controller(
                    hWnd,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hWnd](HRESULT r, ICoreWebView2Controller* controller)->HRESULT {
                            if (r != S_OK || !controller) return r;
                            g_controller = controller;
                            g_controller->get_CoreWebView2(&g_webview);

                            if (g_webview) {
                                ComPtr<ICoreWebView2Settings> settings;
                                if (SUCCEEDED(g_webview->get_Settings(&settings)) && settings) {
                                    settings->put_IsWebMessageEnabled(TRUE);
                                    settings->put_AreDevToolsEnabled(TRUE);
                                }
                            }

                            RECT rc;
                            GetClientRect(hWnd, &rc);
                            g_controller->put_Bounds(rc);

                            EventRegistrationToken msgToken{};
                            g_webview->add_WebMessageReceived(
                                Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args)->HRESULT {
                                        LPWSTR raw = nullptr;
                                        if (FAILED(args->get_WebMessageAsJson(&raw)) || !raw)
                                            return S_OK;
                                        std::wstring payload(raw);
                                        CoTaskMemFree(raw);

                                        // 1) loadSmiles: presets + isomer buttons
                                        if (payload.find(L"\"cmd\":\"loadSmiles\"") != std::wstring::npos) {

                                            const std::wstring skey = L"\"smiles\":\"";
                                            size_t sp = payload.find(skey);
                                            if (sp != std::wstring::npos) {
                                                sp += skey.size();
                                                size_t se = payload.find(L"\"", sp);
                                                if (se != std::wstring::npos) {
                                                    std::wstring smiles = payload.substr(sp, se - sp);
                                                    g_sim.LoadSmiles3D(smiles);
                                                    SendCurrentFrame();
                                                }
                                            }
                                        }

                                        // 2) queryFormulaOnline: formula -> PubChem -> isomer list + auto-load
                                        if (payload.find(L"\"cmd\":\"queryFormulaOnline\"") != std::wstring::npos) {
                                            const std::wstring fkey = L"\"formula\":\"";
                                            size_t fp = payload.find(fkey);
                                            if (fp != std::wstring::npos) {
                                                fp += fkey.size();
                                                size_t fe = payload.find(L"\"", fp);
                                                if (fe != std::wstring::npos) {
                                                    std::wstring formula = payload.substr(fp, fe - fp);

                                                    auto res = QueryPubChemFormulaRecords(formula, 3);

                                                    // Build JSON back to JS for isomer panel
                                                    std::wstringstream js;
                                                    if (!res.ok) {
                                                        js << L"{\"query\":\"" << JsonEscape(formula)
                                                            << L"\",\"error\":\"" << JsonEscape(res.error)
                                                            << L"\",\"debug\":{"
                                                            << L"\"statusCids\":" << res.statusCids
                                                            << L",\"statusRecord\":" << res.statusRecord
                                                            << L"}}";
                                                    }
                                                    else {
                                                        js << L"{\"query\":\"" << JsonEscape(formula) << L"\",\"isomers\":[";
                                                        for (size_t i = 0; i < res.compounds.size(); ++i) {
                                                            const auto& c = res.compounds[i];
                                                            js << L"{\"cid\":" << c.cid
                                                                << L",\"name\":\"" << JsonEscape(c.name)
                                                                << L"\",\"smiles\":\"" << JsonEscape(c.smiles)
                                                                << L",\"atomCount\":" << c.atoms.size()
                                                                << L"}";
                                                            if (i + 1 < res.compounds.size())
                                                                js << L",";
                                                        }
                                                        js << L"],\"debug\":{"
                                                            << L"\"statusCids\":" << res.statusCids
                                                            << L",\"statusRecord\":" << res.statusRecord
                                                            << L"}}";

                                                        // Auto-load first compound (prefer SMILES + OpenBabel)
                                                        if (!res.compounds.empty()) {
                                                            const auto& first = res.compounds.front();
                                                            if (!first.smiles.empty()) {
                                                                g_sim.LoadSmiles3D(first.smiles);
                                                                SendCurrentFrame();
                                                            }
                                                            else {
                                                                // Fallback: PubChem atoms (maybe 2D)
                                                                std::vector<int> nums;
                                                                std::vector<double> xs, ys, zs;
                                                                nums.reserve(first.atoms.size());
                                                                xs.reserve(first.atoms.size());
                                                                ys.reserve(first.atoms.size());
                                                                zs.reserve(first.atoms.size());
                                                                for (auto const& a : first.atoms) {
                                                                    nums.push_back(a.atomicNumber);
                                                                    xs.push_back(a.x);
                                                                    ys.push_back(a.y);
                                                                    zs.push_back(a.z);
                                                                }
                                                                g_sim.LoadPubChem(nums, xs, ys, zs);
                                                                SendCurrentFrame();
                                                            }
                                                        }
                                                    }

                                                    g_webview->PostWebMessageAsJson(js.str().c_str());
                                                }
                                            }
                                        }

                                        return S_OK;
                                    }).Get(),
                                        &msgToken);

                            EventRegistrationToken navToken{};
                            g_webview->add_NavigationCompleted(
                                Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*)->HRESULT {
                                        g_webReady = true;
                                        g_sim.InitializeWater();
                                        SendCurrentFrame();
                                        SetTimer(g_hWnd, TIMER_ID, TIMER_INTERVAL_MS, nullptr);
                                        return S_OK;
                                    }).Get(),
                                        &navToken);

                            std::wstring html = L"file:///" + GetExeDir() + L"/viewer.html";
                            for (auto& c : html) if (c == L'\\') c = L'/';
                            g_webview->Navigate(html.c_str());

                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}

// Window procedure
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (g_controller) {
            RECT rc;
            GetClientRect(hWnd, &rc);
            g_controller->put_Bounds(rc);
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID) {
            g_sim.Advance(1);
            SendCurrentFrame();
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Entry point
int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    const wchar_t* clsName = L"MoleculeSimWnd";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = clsName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    g_hWnd = CreateWindowExW(
        0, clsName,
        L"Molecule Simulation Viewer (OpenBabel 3D)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1000, 700,
        nullptr, nullptr,
        hInst, nullptr);

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    InitWebView(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
