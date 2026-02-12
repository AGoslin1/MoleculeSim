#pragma once
#include <string>
#include <vector>

bool Generate3DCoordinates(
    const std::wstring& smiles,
    std::vector<int>& atomicNumbers,
    std::vector<double>& xs,
    std::vector<double>& ys,
    std::vector<double>& zs);


// New: also return bonds (indices are 0-based; order is 1/2/3)
bool Generate3DWithBonds(
    const std::wstring& smiles,
    std::vector<int>& atomicNumbers,
    std::vector<double>& xs,
    std::vector<double>& ys,
    std::vector<double>& zs,
    std::vector<int>& bA1,
    std::vector<int>& bA2,
    std::vector<int>& bOrder);