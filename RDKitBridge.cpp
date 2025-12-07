#include "RDKitBridge.h"
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <direct.h> // _wmkdir

// --------------------------------------------------------------
// CONFIGURE OPENBABEL LOCATION
// --------------------------------------------------------------
static const wchar_t* OBABEL_EXE =
L"C:\\Program Files\\OpenBabel-3.1.1\\obabel.exe";  // NO QUOTES HERE

// --------------------------------------------------------------
// Ensure MoleculeTemp exists
// --------------------------------------------------------------
static std::wstring EnsureTempFolder()
{
    std::wstring base = L"C:\\Users\\alexg\\source\\repos\\MoleculeSim\\MoleculeTemp";
    _wmkdir(base.c_str()); // safe if exists
    return base;
}

static bool FileExists(const std::wstring& p)
{
    DWORD attr = GetFileAttributesW(p.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
}

// --------------------------------------------------------------
// UNIVERSAL PERIODIC TABLE MAPPING
// --------------------------------------------------------------
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

    return 6; // fallback = Carbon
}

// --------------------------------------------------------------
// Run OpenBabel HIDDEN (no flashing console)
// --------------------------------------------------------------
static bool RunOpenBabelHidden(const std::wstring& smiles,
    const std::wstring& outFile,
    DWORD& exitCode)
{
    exitCode = (DWORD)-1;

    // Build full command line: "C:\...\obabel.exe" -:"SMILES" --gen3d -O "outFile"
    std::wstring cmdLine =
        L"\"" + std::wstring(OBABEL_EXE) + L"\" -:\"" +
        smiles + L"\" --gen3d -O \"" + outFile + L"\"";

    // CreateProcess needs a mutable buffer for the command line
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;   // make sure no window is shown

    BOOL ok = CreateProcessW(
        nullptr,            // lpApplicationName (use command line instead)
        cmdBuf.data(),      // lpCommandLine (mutable buffer)
        nullptr,            // lpProcessAttributes
        nullptr,            // lpThreadAttributes
        FALSE,              // bInheritHandles
        CREATE_NO_WINDOW,   // dwCreationFlags: NO console window
        nullptr,            // lpEnvironment
        nullptr,            // lpCurrentDirectory
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

    // Wait for obabel to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::wstringstream ss;
    ss << L"[OpenBabel] Exit code = " << exitCode << L"\n";
    OutputDebugStringW(ss.str().c_str());

    return true;
}

// --------------------------------------------------------------
// MAIN FUNCTION: Generate 3D coordinates using OpenBabel
// --------------------------------------------------------------
bool Generate3DCoordinates(
    const std::wstring& smiles,
    std::vector<int>& atomicNumbers,
    std::vector<double>& xs,
    std::vector<double>& ys,
    std::vector<double>& zs)
{
    atomicNumbers.clear();
    xs.clear();
    ys.clear();
    zs.clear();

    if (smiles.empty())
        return false;

    // 1) Build output path
    std::wstring folder = EnsureTempFolder();
    std::wstring outFile = folder + L"\\babel_out.sdf";

    // 2) Run OpenBabel (hidden)
    DWORD exitCode = 1;
    if (!RunOpenBabelHidden(smiles, outFile, exitCode))
        return false;

    if (exitCode != 0 || !FileExists(outFile))
        return false;

    // 3) Parse SDF
    std::ifstream in(outFile);
    if (!in)
        return false;

    std::string line;
    std::vector<std::string> sdf;
    while (std::getline(in, line))
        sdf.push_back(line);

    if (sdf.size() < 4)
        return false;

    int atomCount = 0;
    {
        int a, b;
        std::stringstream ss(sdf[3]);
        ss >> a >> b;
        atomCount = a;
    }

    if (atomCount <= 0)
        return false;

    atomicNumbers.reserve(atomCount);
    xs.reserve(atomCount);
    ys.reserve(atomCount);
    zs.reserve(atomCount);

    for (int i = 4; i < 4 + atomCount; i++)
    {
        if (i >= (int)sdf.size()) break;

        double x, y, z;
        char symbol[8] = {};

        sscanf_s(sdf[i].c_str(), "%lf %lf %lf %7s",
            &x, &y, &z, symbol, (unsigned)_countof(symbol));

        xs.push_back(x);
        ys.push_back(y);
        zs.push_back(z);

        atomicNumbers.push_back(AtomicNumberFromSymbol(symbol));
    }

    return true;
}
