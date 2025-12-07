#pragma once
#include <string>
#include <vector>

bool Generate3DCoordinates(
    const std::wstring& smiles,
    std::vector<int>& atomicNumbers,
    std::vector<double>& xs,
    std::vector<double>& ys,
    std::vector<double>& zs);
