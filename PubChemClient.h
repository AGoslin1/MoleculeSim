#pragma once
#include <string>
#include <vector>

struct PubChemAtom {
    int atomicNumber; // e.g. 6 for C, 1 for H
    double x;
    double y;
    double z;
};

struct PubChemBond {
    int a1; // 0-based atom index
    int a2;
    int order;
};

struct PubChemCompound {
    unsigned int cid;
    std::wstring name;   // IUPAC Preferred (fallback any IUPAC)
    std::wstring smiles; // Absolute/Connectivity (fallback empty)
    std::vector<PubChemAtom> atoms;
    std::vector<PubChemBond> bonds;
};

struct PubChemFormulaResult {
    bool ok = false;
    std::wstring error;
    std::wstring formula;
    std::vector<unsigned int> topCids;
    std::vector<PubChemCompound> compounds;
    // Debug
    unsigned long statusCids = 0;
    unsigned long statusRecord = 0;
    std::wstring cidsHead;
    std::wstring recordHead;
};

PubChemFormulaResult QueryPubChemFormulaRecords(const std::wstring& formula,
    size_t topN = 3);
