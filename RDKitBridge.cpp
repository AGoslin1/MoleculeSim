#include "RDKitBridge.h"
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <direct.h>

//fallback
static const wchar_t* DEFAULT_OBABEL_EXE =
L"C:\\Program Files\\OpenBabel-3.1.1\\obabel.exe"; 

//check file exists
static bool FileExists(const std::wstring& p)
{
    DWORD attr = GetFileAttributesW(p.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
}

//find obabel
static std::wstring GetOpenBabelExe()
{
    static std::wstring cached;
    if (!cached.empty()) return cached;

    wchar_t envBuf[MAX_PATH] = {};
    DWORD envLen = GetEnvironmentVariableW(L"OBABEL_EXE", envBuf, MAX_PATH);
    if (envLen > 0 && envLen < MAX_PATH) {
        std::wstring envPath(envBuf);
        if (FileExists(envPath)) {
            cached = envPath;
            return cached;
        }
    }

    //executable
    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
        std::wstring exeDir(exePath);
        size_t pos = exeDir.find_last_of(L"\\/");
        if (pos != std::wstring::npos) exeDir = exeDir.substr(0, pos);
        else exeDir = L".";

        std::vector<std::wstring> candidates;
        candidates.push_back(exeDir + L"\\OpenBabel-3.1.1\\obabel.exe");

        for (const auto& c : candidates) {
            if (FileExists(c)) {
                cached = c;
                return cached;
            }
        }
    }

    wchar_t cwdBuf[MAX_PATH] = {};
    if (GetCurrentDirectoryW(MAX_PATH, cwdBuf) && cwdBuf[0] != L'\0') {
        std::wstring cwd(cwdBuf);
        std::vector<std::wstring> candidates = {
            cwd + L"\\OpenBabel-3.1.1\\obabel.exe",
            cwd + L"\\obabel.exe"
        };
        for (const auto& c : candidates) {
            if (FileExists(c)) {
                cached = c;
                return cached;
            }
        }
    }

    //search PATH
    wchar_t found[MAX_PATH] = {};
    DWORD foundLen = SearchPathW(nullptr, L"obabel.exe", nullptr, MAX_PATH, found, nullptr);
    if (foundLen > 0 && foundLen < MAX_PATH) {
        std::wstring sp(found);
        if (FileExists(sp)) {
            cached = sp;
            return cached;
        }
    }

    //Not found
    OutputDebugStringW(L"[OpenBabel] obabel.exe not found by GetOpenBabelExe\n");
    return cached;
}

static std::wstring EnsureTempFolder()
{
    static std::wstring folder;
    if (!folder.empty()) return folder;


    wchar_t tempPathBuf[MAX_PATH] = {};
    DWORD tpLen = GetTempPathW(MAX_PATH, tempPathBuf);
    if (tpLen == 0 || tpLen > MAX_PATH) {

        wchar_t exePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
            std::wstring base(exePath);
            size_t pos = base.find_last_of(L"\\/");
            folder = (pos == std::wstring::npos) ? L"." : base.substr(0, pos);
            folder += L"\\MoleculeSimTemp";

            CreateDirectoryW(folder.c_str(), nullptr);
            return folder;
        }

        folder = L".\\MoleculeSimTemp";
        _wmkdir(folder.c_str());
        return folder;
    }

    std::wstring tempPath(tempPathBuf);

    wchar_t uniqueName[MAX_PATH] = {};
    if (GetTempFileNameW(tempPath.c_str(), L"MSM", 0, uniqueName) != 0) {
        DeleteFileW(uniqueName);
        if (CreateDirectoryW(uniqueName, nullptr)) {
            folder = uniqueName;
            return folder;
        }
    }

    folder = tempPath;
    if (folder.back() != L'\\' && folder.back() != L'/') folder += L'\\';
    folder += L"MoleculeSim";
    CreateDirectoryW(folder.c_str(), nullptr);
    return folder;
}

static int AtomicNumberFromSymbol(const char* s)
{
    static const char* symbols[] = {
        "", "H","He","Li","Be","B","C","N","O","F","Ne",
        "Na","Mg","Al","Si","P","S","Cl","Ar",
        "K","Ca","Sc","Ti","V","Cr","Mn","Fe","Co","Ni",
        "Cu","Zn","Ga","Ge","As","Se","Br","Kr",
        "Rb","Sr","Y","Zr","Nb","Mo","Tc","Ru","Rh","Pd",
        "Ag","Cd","In","Sn","Sb","Te","I","Xe",
        "Cs","Ba","La","Ce","Pr","Nd","Pm","Sm","Eu","Gd",
        "Tb","Dy","Ho","Er","Tm","Yb","Lu",
        "Hf","Ta","W","Re","Os","Ir","Pt","Au","Hg",
        "Tl","Pb","Bi","Po","At","Rn",
        "Fr","Ra","Ac","Th","Pa","U","Np","Pu","Am","Cm",
        "Bk","Cf","Es","Fm","Md","No","Lr",
        "Rf","Db","Sg","Bh","Hs","Mt","Ds","Rg","Cn","Nh",
        "Fl","Mc","Lv","Ts","Og"
    };

    for (int i = 1; i <= 118; i++)
        if (strcmp(s, symbols[i]) == 0)
            return i;

    return 6;
}

// Run OpenBabel 
static bool RunOpenBabelHidden(const std::wstring& smiles,
    const std::wstring& outFile,
    DWORD& exitCode)
{
    exitCode = (DWORD)-1;

    std::wstring obabel = GetOpenBabelExe();
    if (obabel.empty()) {
        std::wstringstream ss;
        ss << L"[OpenBabel] obabel.exe not found; aborting generation\n";
        OutputDebugStringW(ss.str().c_str());
        return false;
    }

    // Build full command line: "C:\...\obabel.exe" -:"SMILES" --gen3d -O "outFile"
    std::wstring cmdLine =
        L"\"" + obabel + L"\" -:\"" +
        smiles + L"\" --gen3d -O \"" + outFile + L"\"";

    //CreateProcess
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;   

    BOOL ok = CreateProcessW(
        nullptr,           
        cmdBuf.data(),      
        nullptr,          
        nullptr,            
        FALSE,              
        CREATE_NO_WINDOW,  
        nullptr,          
        nullptr,           
        &si,
        &pi
    );

    if (!ok)
    {
        DWORD err = GetLastError();
        std::wstringstream ss;
        ss << L"[OpenBabel] CreateProcessW failed. GLE=" << err << L"\n";
        OutputDebugStringW(ss.str().c_str());
        return false;
    }

    //Wait for obabel to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::wstringstream ss;
    ss << L"[OpenBabel] Exit code = " << exitCode << L"\n";
    OutputDebugStringW(ss.str().c_str());

    return true;
}

static bool ParseSdfAtomsAndBonds(
    const std::wstring& sdfPath,
    std::vector<int>& atomicNumbers,
    std::vector<double>& xs,
    std::vector<double>& ys,
    std::vector<double>& zs,
    std::vector<int>& bA1,
    std::vector<int>& bA2,
    std::vector<int>& bOrder)
{
    atomicNumbers.clear(); xs.clear(); ys.clear(); zs.clear();
    bA1.clear(); bA2.clear(); bOrder.clear();

    std::ifstream in(sdfPath);
    if (!in) return false;

    std::string line;
    std::vector<std::string> sdf;
    while (std::getline(in, line)) sdf.push_back(line);
    if (sdf.size() < 4) return false;


    int atomCount = 0, bondCount = 0;
    {
        int a = 0, b = 0;
        std::stringstream ss(sdf[3]);
        ss >> a >> b;
        atomCount = a; bondCount = b;
    }
    if (atomCount <= 0) return false;

    atomicNumbers.reserve(atomCount);
    xs.reserve(atomCount); ys.reserve(atomCount); zs.reserve(atomCount);

    // Atom block
    for (int i = 4; i < 4 + atomCount && i < (int)sdf.size(); ++i) {
        double x = 0, y = 0, z = 0;
        char symbol[8] = {};
        sscanf_s(sdf[i].c_str(), "%lf %lf %lf %7s", &x, &y, &z, symbol, (unsigned)_countof(symbol));
        xs.push_back(x);
        ys.push_back(y);
        zs.push_back(z);
        atomicNumbers.push_back(AtomicNumberFromSymbol(symbol));
    }

    // Bond block
    int bondStart = 4 + atomCount;
    int bondEnd = bondStart + bondCount;
    bA1.reserve(bondCount); bA2.reserve(bondCount); bOrder.reserve(bondCount);

    for (int i = bondStart; i < bondEnd && i < (int)sdf.size(); ++i) {
        int a1 = 0, a2 = 0, order = 0;
        std::stringstream ss(sdf[i]);
        ss >> a1 >> a2 >> order;
        if (a1 > 0 && a2 > 0) {
            bA1.push_back(a1 - 1);
            bA2.push_back(a2 - 1);
            bOrder.push_back(order > 0 ? order : 1);
        }
    }

    return true;
}

//generate 3D coordinates using OpenBabel

bool Generate3DCoordinates(
    const std::wstring& smiles,
    std::vector<int>& atomicNumbers,
    std::vector<double>& xs,
    std::vector<double>& ys,
    std::vector<double>& zs)
{
    atomicNumbers.clear(); xs.clear(); ys.clear(); zs.clear();
    if (smiles.empty()) return false;

    std::wstring folder = EnsureTempFolder();
    std::wstring outFile = folder + L"\\babel_out.sdf";

    DWORD exitCode = 1;
    if (!RunOpenBabelHidden(smiles, outFile, exitCode)) return false;
    if (exitCode != 0 || !FileExists(outFile)) return false;

    std::vector<int> dummyA1, dummyA2, dummyOrder;
    return ParseSdfAtomsAndBonds(outFile, atomicNumbers, xs, ys, zs, dummyA1, dummyA2, dummyOrder);
}

bool Generate3DWithBonds(
    const std::wstring& smiles,
    std::vector<int>& atomicNumbers,
    std::vector<double>& xs,
    std::vector<double>& ys,
    std::vector<double>& zs,
    std::vector<int>& bA1,
    std::vector<int>& bA2,
    std::vector<int>& bOrder)
{
    atomicNumbers.clear(); xs.clear(); ys.clear(); zs.clear();
    bA1.clear(); bA2.clear(); bOrder.clear();
    if (smiles.empty()) return false;

    std::wstring folder = EnsureTempFolder();
    std::wstring outFile = folder + L"\\babel_out.sdf";

    DWORD exitCode = 1;
    if (!RunOpenBabelHidden(smiles, outFile, exitCode)) return false;
    if (exitCode != 0 || !FileExists(outFile)) return false;

    return ParseSdfAtomsAndBonds(outFile, atomicNumbers, xs, ys, zs, bA1, bA2, bOrder);
}