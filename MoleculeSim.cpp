// moleculesim.cpp - Molecule viewer using WebView2 + PubChem + OpenBabel 3D

#define UNICODE
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <string>
#include <sstream>
#include <unordered_map>

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

// NOTE: Timer no longer used
// static const UINT_PTR TIMER_ID = 1;
// static const UINT TIMER_INTERVAL_MS = 50;

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
        case L'\\': o << L"\\\\";
            break;
        case L'\n': o << L"\\n"; break;
        case L'\r': o << L"\\r"; break;
        case L'\t': o << L"\\t"; break;
        default:    o << c;     break;
        }
    }
    return o.str();
}

// Post info about a PubChemCompound to the WebView (shown in right-hand Information panel)
static void PostInfoToWebView(const PubChemCompound& c) {
    if (!g_webview) return;

    // Count bond orders (1/2/3) and total bonds
    int bondCount = static_cast<int>(c.bonds.size());
    int bondOrdersCount[4] = { 0, 0, 0, 0 }; // index by order (1..3)
    for (const auto& b : c.bonds) {
        int ord = b.order;
        if (ord < 1) ord = 1;
        if (ord > 3) ord = 3;
        bondOrdersCount[ord]++;
    }

    std::wstringstream js;
    js << L"{\"info\":{";
    js << L"\"cid\":" << c.cid;
    js << L",\"name\":\"" << JsonEscape(c.name) << L"\"";
    js << L",\"smiles\":\"" << JsonEscape(c.smiles) << L"\"";
    js << L",\"formula\":\"" << JsonEscape(c.formula) << L"\"";
    js << L",\"molecularWeight\":" << c.molecularWeight;
    js << L",\"heavyAtomCount\":" << c.heavyAtomCount;
    js << L",\"rotatableBondCount\":" << c.rotatableBondCount;
    js << L",\"bondCount\":" << bondCount;
    js << L",\"bondOrderCounts\":[" << bondOrdersCount[1] << L"," << bondOrdersCount[2] << L"," << bondOrdersCount[3] << L"]";

    // Include bonds array (a1,a2 are 0-based indices)
    js << L",\"bonds\":[";
    for (size_t i = 0; i < c.bonds.size(); ++i) {
        const auto& b = c.bonds[i];
        js << L"{\"a1\":" << b.a1 << L",\"a2\":" << b.a2 << L",\"order\":" << b.order << L"}";
        if (i + 1 < c.bonds.size()) js << L",";
    }
    js << L"]";

    js << L"}}";
    g_webview->PostWebMessageAsJson(js.str().c_str());
}

// Post minimal info when only a SMILES string is loaded (no PubChem record)
static void PostInfoToWebViewForSmiles(const std::wstring& smiles) {
    if (!g_webview) return;
    std::wstringstream js;
    js << L"{\"info\":{";
    js << L"\"cid\":0";
    js << L",\"name\":\"\"";
    js << L",\"smiles\":\"" << JsonEscape(smiles) << L"\"";
    js << L",\"formula\":\"\"";
    js << L",\"molecularWeight\":0";
    js << L",\"heavyAtomCount\":0";
    js << L",\"rotatableBondCount\":0";
    js << L",\"bondCount\":0";
    js << L",\"bondOrderCounts\":[0,0,0]";
    js << L",\"bonds\":[]";
    js << L"}}";
    g_webview->PostWebMessageAsJson(js.str().c_str());
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
                                                    SendCurrentFrame(); // single frame
                                                    // update right-hand Information panel
                                                    PostInfoToWebViewForSmiles(smiles);
                                                }
                                            }
                                        }

                                        // 1b) loadCid: load PubChem compound by CID (for entries without SMILES)
                                        if (payload.find(L"\"cmd\":\"loadCid\"") != std::wstring::npos) {
                                            const std::wstring ckey = L"\"cid\":";
                                            size_t cp = payload.find(ckey);
                                            if (cp != std::wstring::npos) {
                                                cp += ckey.size();
                                                unsigned int cid = 0;
                                                while (cp < payload.size() && iswdigit(payload[cp])) {
                                                    cid = cid * 10 + (payload[cp] - L'0');
                                                    ++cp;
                                                }
                                                if (cid) {
                                                    PubChemCompound c = QueryPubChemCid(cid);
                                                    if (!c.smiles.empty()) {
                                                        g_sim.LoadSmiles3D(c.smiles);
                                                        SendCurrentFrame(); // single frame
                                                        // publish full info to UI
                                                        PostInfoToWebView(c);
                                                    }
                                                    else if (!c.atoms.empty()) {
                                                        std::vector<int> nums; std::vector<double> xs, ys, zs;
                                                        nums.reserve(c.atoms.size());
                                                        xs.reserve(c.atoms.size());
                                                        ys.reserve(c.atoms.size());
                                                        zs.reserve(c.atoms.size());
                                                        for (auto const& a : c.atoms) {
                                                            nums.push_back(a.atomicNumber);
                                                            xs.push_back(a.x);
                                                            ys.push_back(a.y);
                                                            zs.push_back(a.z);
                                                        }
                                                        g_sim.LoadPubChem(nums, xs, ys, zs);
                                                        SendCurrentFrame(); // single frame
                                                        PostInfoToWebView(c);
                                                    }
                                                }
                                            }
                                        }

                                        // 2) queryFormulaOnline: formula -> PubChem -> isomer list (do NOT auto-load)
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
                                                        // Select the 3 most popular (same order as fastformula/topCids) that are significant
                                                        std::vector<PubChemCompound> relevant;
                                                        relevant.reserve(3);

                                                        // Build CID -> compound map to preserve popularity ordering
                                                        std::unordered_map<unsigned int, size_t> indexByCid;
                                                        indexByCid.reserve(res.compounds.size());
                                                        for (size_t i = 0; i < res.compounds.size(); ++i) {
                                                            indexByCid[res.compounds[i].cid] = i;
                                                        }

                                                        for (auto cid : res.topCids) {
                                                            auto it = indexByCid.find(cid);
                                                            if (it == indexByCid.end()) continue;
                                                            const auto& c = res.compounds[it->second];
                                                            const bool significant = (!c.smiles.empty() || !c.atoms.empty());
                                                            if (significant) {
                                                                relevant.push_back(c);
                                                                if (relevant.size() == 3) break;
                                                            }
                                                        }

                                                        // Build isomer list for UI
                                                        js << L"{\"query\":\"" << JsonEscape(formula) << L"\",\"isomers\":[";
                                                        for (size_t i = 0; i < relevant.size(); ++i) {
                                                            const auto& c = relevant[i];
                                                            js << L"{\"cid\":" << c.cid
                                                                << L",\"name\":\"" << JsonEscape(c.name) << L"\""
                                                                << L",\"smiles\":\"" << JsonEscape(c.smiles) << L"\""
                                                                << L",\"atomCount\":" << c.atoms.size()
                                                                << L",\"formula\":\"" << JsonEscape(c.formula) << L"\""
                                                                << L",\"molecularWeight\":" << c.molecularWeight
                                                                << L",\"heavyAtomCount\":" << c.heavyAtomCount
                                                                << L",\"rotatableBondCount\":" << c.rotatableBondCount
                                                                << L"}";
                                                            if (i + 1 < relevant.size()) js << L",";
                                                        }
                                                        js << L"],\"debug\":{"
                                                            << L"\"statusCids\":" << res.statusCids
                                                            << L",\"statusRecord\":" << res.statusRecord
                                                            << L"}}";

                                                        // NOTE: Auto-load removed. The UI will present the isomer list and only load
                                                        // when the user clicks an isomer (which sends loadCid/loadSmiles).
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
                                        g_sim.LoadSmiles3D(L"O");
                                        SendCurrentFrame();
                                        PostInfoToWebViewForSmiles(L"O");
                                        // Stop starting any timers: no continuous frames
                                        // SetTimer(g_hWnd, TIMER_ID, TIMER_INTERVAL_MS, nullptr);
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

        // Remove continuous animation frames
        // case WM_TIMER:
        //     if (wParam == TIMER_ID) {
        //         g_sim.Advance(1);
        //         SendCurrentFrame();
        //     }
        //     return 0;

    case WM_DESTROY:
        // KillTimer(hWnd, TIMER_ID); // not started anymore
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Entry point unchanged
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