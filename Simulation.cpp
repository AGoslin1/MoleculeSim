#define NOMINMAX
#include "Windows.h"
#include "Simulation.h"
#include "RDKitBridge.h"
#include <cmath>
#include <unordered_map>
#include <numeric>
#include <algorithm>

//Element colours
static const std::unordered_map<std::wstring, std::wstring> kElementColors = {
    {L"H",L"#FFFFFF"},{L"He",L"#D9FFFF"},{L"Li",L"#CC80FF"},{L"Be",L"#C2FF00"},{L"B",L"#FFB5B5"},
    {L"C",L"#909090"},{L"N",L"#3050F8"},{L"O",L"#FF0D0D"},{L"F",L"#90E050"},{L"Ne",L"#B3E3F5"},
    {L"Na",L"#AB5CF2"},{L"Mg",L"#8AFF00"},{L"Al",L"#BFA6A6"},{L"Si",L"#F0C8A0"},{L"P",L"#FF8000"},
    {L"S",L"#FFFF30"},{L"Cl",L"#1FF01F"},{L"Ar",L"#80D1E3"},{L"K",L"#8F40D4"},{L"Ca",L"#3DFF00"},
    {L"Sc",L"#E6E6E6"},{L"Ti",L"#BFC2C7"},{L"V",L"#A6A6AB"},{L"Cr",L"#8A99C7"},{L"Mn",L"#9C7AC7"},
    {L"Fe",L"#E06633"},{L"Co",L"#F090A0"},{L"Ni",L"#50D050"},{L"Cu",L"#C88033"},{L"Zn",L"#7D80B0"},
    {L"Ga",L"#C28F8F"},{L"Ge",L"#668F8F"},{L"As",L"#BD80E3"},{L"Se",L"#FFA100"},{L"Br",L"#A62929"},
    {L"Kr",L"#5CB8D1"},{L"Rb",L"#702EB0"},{L"Sr",L"#00FF00"},{L"Y",L"#94FFFF"},{L"Zr",L"#94E0E0"},
    {L"Nb",L"#73C2C9"},{L"Mo",L"#54B5B5"},{L"Tc",L"#3B9E9E"},{L"Ru",L"#248F8F"},{L"Rh",L"#0A7D8C"},
    {L"Pd",L"#006985"},{L"Ag",L"#C0C0C0"},{L"Cd",L"#FFD98F"},{L"In",L"#A67573"},{L"Sn",L"#668080"},
    {L"Sb",L"#9E63B5"},{L"Te",L"#D47A00"},{L"I",L"#FFAFDC"},{L"Xe",L"#429EB0"},{L"Cs",L"#57178F"},
    {L"Ba",L"#00C900"},{L"La",L"#70D4FF"},{L"Ce",L"#FFFFC7"},{L"Pr",L"#D9FFC7"},{L"Nd",L"#C7FFC7"},
    {L"Pm",L"#A3FFC7"},{L"Sm",L"#8FFFC7"},{L"Eu",L"#61FFC7"},{L"Gd",L"#45FFC7"},{L"Tb",L"#30FFC7"},
    {L"Dy",L"#1FFFC7"},{L"Ho",L"#00FF9C"},{L"Er",L"#00E675"},{L"Tm",L"#00D452"},{L"Yb",L"#00BF38"},
    {L"Lu",L"#00AB24"},{L"Hf",L"#4DC2FF"},{L"Ta",L"#4DA6FF"},{L"W",L"#2194D6"},{L"Re",L"#267DAB"},
    {L"Os",L"#266696"},{L"Ir",L"#175487"},{L"Pt",L"#D0D0E0"},{L"Au",L"#FFD123"},{L"Hg",L"#B8B8D0"},
    {L"Tl",L"#A6544D"},{L"Pb",L"#575961"},{L"Bi",L"#9E4FB5"},{L"Po",L"#AB5C00"},{L"At",L"#754F45"},
    {L"Rn",L"#428296"},{L"Fr",L"#420066"},{L"Ra",L"#007D00"},{L"Ac",L"#70ABFA"},{L"Th",L"#00BAFF"},
    {L"Pa",L"#00A1FF"},{L"U",L"#008FFF"},{L"Np",L"#0080FF"},{L"Pu",L"#006BFF"},{L"Am",L"#545CF2"},
    {L"Cm",L"#785CE3"},{L"Bk",L"#8A4FE3"},{L"Cf",L"#A136D4"},{L"Es",L"#BF2E94"},{L"Fm",L"#FF8000"},
    {L"Md",L"#FF00FF"},{L"No",L"#FF1493"},{L"Lr",L"#FF4500"},{L"Rf",L"#A0A0A0"},{L"Db",L"#A0A0A0"},
    {L"Sg",L"#A0A0A0"},{L"Bh",L"#A0A0A0"},{L"Hs",L"#A0A0A0"},{L"Mt",L"#A0A0A0"},{L"Ds",L"#A0A0A0"},
    {L"Rg",L"#A0A0A0"},{L"Cn",L"#A0A0A0"},{L"Nh",L"#A0A0A0"},{L"Fl",L"#A0A0A0"},{L"Mc",L"#A0A0A0"},
    {L"Lv",L"#A0A0A0"},{L"Ts",L"#A0A0A0"},{L"Og",L"#A0A0A0"}
};

//Atomic symbols for Z -> symbol
static const wchar_t* kAtomicSymbols[] = {
    L"", L"H",L"He",L"Li",L"Be",L"B",L"C",L"N",L"O",L"F",L"Ne",
    L"Na",L"Mg",L"Al",L"Si",L"P",L"S",L"Cl",L"Ar",
    L"K",L"Ca",L"Sc",L"Ti",L"V",L"Cr",L"Mn",L"Fe",L"Co",L"Ni",
    L"Cu",L"Zn",L"Ga",L"Ge",L"As",L"Se",L"Br",L"Kr",
    L"Rb",L"Sr",L"Y",L"Zr",L"Nb",L"Mo",L"Tc",L"Ru",L"Rh",L"Pd",
    L"Ag",L"Cd",L"In",L"Sn",L"Sb",L"Te",L"I",L"Xe",
    L"Cs",L"Ba",L"La",L"Ce",L"Pr",L"Nd",L"Pm",L"Sm",L"Eu",L"Gd",
    L"Tb",L"Dy",L"Ho",L"Er",L"Tm",L"Yb",L"Lu",
    L"Hf",L"Ta",L"W",L"Re",L"Os",L"Ir",L"Pt",L"Au",L"Hg",
    L"Tl",L"Pb",L"Bi",L"Po",L"At",L"Rn",
    L"Fr",L"Ra",L"Ac",L"Th",L"Pa",L"U",L"Np",L"Pu",L"Am",L"Cm",
    L"Bk",L"Cf",L"Es",L"Fm",L"Md",L"No",L"Lr",
    L"Rf",L"Db",L"Sg",L"Bh",L"Hs",L"Mt",L"Ds",L"Rg",L"Cn",L"Nh",
    L"Fl",L"Mc",L"Lv",L"Ts",L"Og"
};

static std::wstring AtomicNumberToSymbol(int n) {
    if (n > 0 && n < (int)(sizeof(kAtomicSymbols) / sizeof(kAtomicSymbols[0])))
        return kAtomicSymbols[n];
    return L"X";
}

SimulationModel::SimulationModel() : m_step(0), m_resetPending(false) {}

void SimulationModel::Clear() {
    m_atoms.clear();
    m_bonds.clear();
    m_step = 0;
}

std::wstring SimulationModel::ElementColor(const std::wstring& sym) const {
    auto it = kElementColors.find(sym);
    return it == kElementColors.end() ? L"#A0A0A0" : it->second;
}

double SimulationModel::ElementRadius(const std::wstring& sym) const {
    if (sym == L"H") return 0.25;
    if (sym == L"C") return 0.35;
    if (sym == L"N") return 0.34;
    if (sym == L"O") return 0.38;
    return 0.33;
}

void SimulationModel::AddAtom(const std::wstring& sym, double x, double y, double z) {
    m_atoms.push_back({ sym, x, y, z, ElementRadius(sym), ElementColor(sym) });
}

void SimulationModel::InitializeWater() {
    Clear();
    AddAtom(L"O", 0, 0, 0);
    AddAtom(L"H", 0.95, 0, 0);
    AddAtom(L"H", -0.30, 0.90, 0);
    m_resetPending = true;
}

void SimulationModel::LoadSmiles(const std::wstring& smiles) {
    Clear();
    if (smiles == L"O") {
        InitializeWater();
        return;
    }
    else if (smiles == L"C") {
        AddAtom(L"C", 0, 0, 0);
        double r = 1.05;
        AddAtom(L"H", r, r, r);
        AddAtom(L"H", -r, -r, r);
        AddAtom(L"H", -r, r, -r);
        AddAtom(L"H", r, -r, -r);
    }
    else if (smiles == L"O=C=O") {
        AddAtom(L"O", -1.2, 0, 0);
        AddAtom(L"C", 0, 0, 0);
        AddAtom(L"O", 1.2, 0, 0);
    }
    else if (smiles == L"[H][H]") {
        AddAtom(L"H", -0.4, 0, 0);
        AddAtom(L"H", 0.4, 0, 0);
    }
    else {
        AddAtom(L"C", 0, 0, 0); 
    }
    m_resetPending = true;
}


void SimulationModel::PromoteFlatTo3D(std::vector<int>& atomicNumbers,
    std::vector<double>& xs,
    std::vector<double>& ys,
    std::vector<double>& zs) {
    size_t n = atomicNumbers.size();
    if (!n) return;

    double maxZ = 0.0;
    for (double z : zs) maxZ = std::max(maxZ, std::fabs(z));
    if (maxZ > 1e-5) return; 

    double amp = 0.4;
    for (size_t i = 0; i < n; ++i)
        zs[i] = amp * std::sin(i * (2.0 * 3.1415926535) / n);

    double cx = 0, cy = 0, cz = 0;
    for (size_t i = 0; i < n; ++i) { cx += xs[i]; cy += ys[i]; cz += zs[i]; }
    cx /= n; cy /= n; cz /= n;
    for (size_t i = 0; i < n; ++i) { xs[i] -= cx; ys[i] -= cy; zs[i] -= cz; }
}

void SimulationModel::LoadPubChem(const std::vector<int>& atomicNumbers,
    const std::vector<double>& xs,
    const std::vector<double>& ys,
    const std::vector<double>& zs,
    const std::vector<int>* bA1,
    const std::vector<int>* bA2,
    const std::vector<int>* bOrder) {
    Clear();
    size_t n = atomicNumbers.size();
    if (!n) { m_resetPending = true; return; }

    std::vector<int> nums = atomicNumbers;
    std::vector<double> xbuf = xs, ybuf = ys, zbuf = zs;


    PromoteFlatTo3D(nums, xbuf, ybuf, zbuf);

    for (size_t i = 0; i < n; ++i)
        AddAtom(AtomicNumberToSymbol(nums[i]), xbuf[i], ybuf[i], zbuf[i]);


    if (bA1 && bA2 && bOrder) {
        size_t nb = std::min(bA1->size(), bA2->size());
        m_bonds.reserve(nb);
        for (size_t i = 0; i < nb; ++i) {
            SimBond b{ (*bA1)[i], (*bA2)[i], (i < bOrder->size() ? (*bOrder)[i] : 1) };
            if (b.a1 >= 0 && b.a2 >= 0 && b.a1 < (int)m_atoms.size() && b.a2 < (int)m_atoms.size())
                m_bonds.push_back(b);
        }
    }

    m_resetPending = true;
}


void SimulationModel::LoadSmiles3D(const std::wstring& smiles) {
    std::vector<int> nums;
    std::vector<double> xs, ys, zs;
    std::vector<int> bA1, bA2, bOrder;

    if (Generate3DWithBonds(smiles, nums, xs, ys, zs, bA1, bA2, bOrder) ||
        Generate3DCoordinates(smiles, nums, xs, ys, zs)) {
        if (!bA1.empty()) {
            LoadPubChem(nums, xs, ys, zs, &bA1, &bA2, &bOrder);
        }
        else {
            LoadPubChem(nums, xs, ys, zs);
        }
        return;
    }
    LoadSmiles(smiles);
}

void SimulationModel::ApplyDemoMotion() {
    for (size_t i = 1; i < m_atoms.size(); ++i) {
        auto& a = m_atoms[i];
        double t = m_step * 0.04 + i;
        a.x += 0.02 * std::sin(t);
        a.y += 0.02 * std::cos(t * 0.9);
        a.z += 0.01 * std::sin(t * 0.7);
    }
}

std::wstring SimulationModel::BuildJsonFrame() {
    std::wstringstream ss;
    ss << L"{\"step\":" << m_step
        << L",\"reset\":" << (m_resetPending ? L"true" : L"false")
        << L",\"atoms\":[";

    for (size_t i = 0; i < m_atoms.size(); ++i) {
        const auto& a = m_atoms[i];
        ss << L"{\"element\":\"" << a.symbol
            << L"\",\"x\":" << a.x
            << L",\"y\":" << a.y
            << L",\"z\":" << a.z
            << L",\"color\":\"" << a.colorHex
            << L"\",\"radius\":" << a.radius
            << L"}";
        if (i + 1 < m_atoms.size()) ss << L",";
    }
    ss << L"]";

    if (!m_bonds.empty()) {
        ss << L",\"bonds\":[";
        for (size_t i = 0; i < m_bonds.size(); ++i) {
            const auto& b = m_bonds[i];
            ss << L"{\"a1\":" << b.a1 << L",\"a2\":" << b.a2 << L",\"order\":" << b.order << L"}";
            if (i + 1 < m_bonds.size()) ss << L",";
        }
        ss << L"]";
    }

    {
        int heavy = 0;
        for (const auto& a : m_atoms) {
            if (a.symbol != L"H") ++heavy;
        }

        int bondCount = static_cast<int>(m_bonds.size());
        int bondOrders[4] = { 0, 0, 0, 0 };
        for (const auto& b : m_bonds) {
            int ord = b.order;
            if (ord < 1) ord = 1;
            if (ord > 3) ord = 3;
            ++bondOrders[ord];
        }

        ss << L",\"info\":{";
        ss << L"\"heavyAtomCount\":" << heavy;
        ss << L",\"bondCount\":" << bondCount;
        ss << L",\"bondOrderCounts\":[" << bondOrders[1] << L"," << bondOrders[2] << L"," << bondOrders[3] << L"]";

        ss << L",\"bonds\":[";
        for (size_t i = 0; i < m_bonds.size(); ++i) {
            const auto& b = m_bonds[i];
            ss << L"{\"a1\":" << b.a1 << L",\"a2\":" << b.a2 << L",\"order\":" << b.order << L"}";
            if (i + 1 < m_bonds.size()) ss << L",";
        }
        ss << L"]";

        ss << L"}";
    }

    ss << L"}";
    m_resetPending = false;
    return ss.str();
}