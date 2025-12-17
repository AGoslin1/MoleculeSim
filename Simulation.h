#pragma once
#include <string>
#include <vector>
#include <sstream>

struct SimAtom {
    std::wstring symbol;
    double x;
    double y;
    double z;
    double radius;
    std::wstring colorHex;
};

class SimulationModel {
public:
    SimulationModel();
    void InitializeWater();
    void LoadSmiles(const std::wstring& smiles);
    void LoadSmiles3D(const std::wstring& smiles); // uses OpenBabel via RDKitBridge
    void LoadPubChem(const std::vector<int>& atomicNumbers,
        const std::vector<double>& xs,
        const std::vector<double>& ys,
        const std::vector<double>& zs);
    void Advance(int steps = 1);
    std::wstring BuildJsonFrame();

private:
    int m_step;
    bool m_resetPending;
    std::vector<SimAtom> m_atoms;

    void Clear();
    void AddAtom(const std::wstring& sym, double x, double y, double z);
    void ApplyDemoMotion();
    std::wstring ElementColor(const std::wstring& sym) const;
    double ElementRadius(const std::wstring& sym) const;

    void PromoteFlatTo3D(std::vector<int>& atomicNumbers,
        std::vector<double>& xs,
        std::vector<double>& ys,
        std::vector<double>& zs);

    // (Unused)
    void BuildLinearAlkane(const std::wstring& smiles,
        std::vector<int>& atomicNumbers,
        std::vector<int>& bA1,
        std::vector<int>& bA2,
        std::vector<int>& bOrder);
    void Generate3DFromGraph(const std::vector<int>& atomicNumbers,
        const std::vector<int>& bA1,
        const std::vector<int>& bA2,
        const std::vector<int>& bOrder,
        std::vector<double>& xs,
        std::vector<double>& ys,
        std::vector<double>& zs);
};
