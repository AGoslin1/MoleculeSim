#pragma once
#include <string>
#include <vector>

bool Generate3DCoordinates(
    const std::wstring& smiles,
    std::vector<int>& atomicNumbers,
    std::vector<double>& xs,
    std::vector<double>& ys,
    std::vector<double>& zs);


bool Generate3DWithBonds(
    const std::wstring& smiles,
    std::vector<int>& atomicNumbers,
    std::vector<double>& xs,
    std::vector<double>& ys,
    std::vector<double>& zs,
    std::vector<int>& bA1,
    std::vector<int>& bA2,
    std::vector<int>& bOrder);