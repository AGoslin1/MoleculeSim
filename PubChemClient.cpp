
#include "PubChemClient.h"
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

#pragma comment(lib, "winhttp.lib")

// Utilities 
static std::wstring Utf8ToW(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

static bool HttpGet(const std::wstring& host,
    const std::wstring& path,
    std::string& body,
    unsigned long& status);

static std::vector<PubChemCompound> ParseRecord(const std::string& json);

PubChemCompound QueryPubChemCid(unsigned int cid) {
    PubChemCompound out{};
    std::wstring path = L"/rest/pug/compound/cid/" + std::to_wstring(cid) + L"/record/JSON";
    std::string recordJson;
    unsigned long status = 0;
    if (!HttpGet(L"pubchem.ncbi.nlm.nih.gov", path, recordJson, status) || status != 200)
        return out;
    auto comps = ParseRecord(recordJson);
    if (!comps.empty()) out = comps[0];
    return out;
}

static bool HttpGet(const std::wstring& host,
    const std::wstring& path,
    std::string& body,
    unsigned long& status) {
    body.clear();
    status = 0;
    HINTERNET hSession = WinHttpOpen(L"MoleculeSim/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    static const wchar_t* hdr = L"Accept-Encoding: identity\r\n";
    BOOL ok = WinHttpSendRequest(hRequest, hdr, (DWORD)wcslen(hdr),
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (!ok) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD st = 0, stSize = sizeof(st);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &st, &stSize, WINHTTP_NO_HEADER_INDEX);
    status = st;

    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail) {
        std::string buf; buf.resize(avail);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, &buf[0], avail, &read) || read == 0) break;
        buf.resize(read);
        body.append(buf);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

static std::vector<unsigned int> ParseCIDArray(const std::string& json) {
    std::vector<unsigned int> cids;
    size_t idList = json.find("\"IdentifierList\"");
    if (idList == std::string::npos) return cids;
    size_t cidTag = json.find("\"CID\"", idList);
    if (cidTag == std::string::npos) return cids;
    size_t lb = json.find('[', cidTag);
    size_t rb = json.find(']', lb == std::string::npos ? 0 : lb);
    if (lb == std::string::npos || rb == std::string::npos) return cids;
    std::string arr = json.substr(lb + 1, rb - lb - 1);
    size_t i = 0;
    while (i < arr.size()) {
        while (i < arr.size() && !isdigit((unsigned char)arr[i])) ++i;
        if (i >= arr.size()) break;
        unsigned int v = 0;
        while (i < arr.size() && isdigit((unsigned char)arr[i])) {
            v = v * 10 + (arr[i] - '0');
            ++i;
        }
        if (v) cids.push_back(v);
    }
    return cids;
}

static std::string ExtractArrayObjectBlock(const std::string& json,
    size_t arrayStartPos,
    size_t& afterArrayPos) {
    int depth = 0;
    size_t i = arrayStartPos;
    for (; i < json.size(); ++i) {
        char c = json[i];
        if (c == '[') ++depth;
        else if (c == ']') {
            --depth;
            if (depth == 0) { ++i; break; }
        }
    }
    afterArrayPos = i;
    if (depth != 0) return {};
    return json.substr(arrayStartPos, i - arrayStartPos);
}

static std::vector<std::string> SplitTopLevelObjects(const std::string& arrayBlock) {
    std::vector<std::string> objs;
    size_t pos = 0;
    while (pos < arrayBlock.size() && arrayBlock[pos] != '[') ++pos;
    if (pos >= arrayBlock.size()) return objs;
    ++pos;
    while (pos < arrayBlock.size()) {
        while (pos < arrayBlock.size() && isspace((unsigned char)arrayBlock[pos])) ++pos;
        if (pos >= arrayBlock.size() || arrayBlock[pos] == ']') break;
        if (arrayBlock[pos] != '{') {
            ++pos;
            continue;
        }
        size_t start = pos;
        int depth = 0;
        bool inString = false;
        bool escape = false;
        for (; pos < arrayBlock.size(); ++pos) {
            char c = arrayBlock[pos];
            if (inString) {
                if (escape) escape = false;
                else if (c == '\\') escape = true;
                else if (c == '"') inString = false;
                continue;
            }
            else {
                if (c == '"') { inString = true; continue; }
                if (c == '{') ++depth;
                else if (c == '}') {
                    --depth;
                    if (depth == 0) {
                        ++pos;
                        objs.push_back(arrayBlock.substr(start, pos - start));
                        break;
                    }
                }
            }
        }
    }
    return objs;
}

static std::wstring FindStringValue(const std::string& obj,
    const std::string& label,
    const std::string& name) {
    std::string patLabel = "\"label\"";
    std::string patName = "\"name\"";
    std::string patSval = "\"sval\"";

    size_t pos = obj.find(patLabel);
    while (pos != std::string::npos) {
        size_t labelPos = obj.find("\"" + label + "\"", pos);
        if (labelPos != std::string::npos) {
            size_t urnEnd = obj.find("}", pos);
            if (urnEnd == std::string::npos) urnEnd = obj.size();
            size_t namePos = obj.find("\"" + name + "\"", pos);
            if (namePos != std::string::npos && namePos < urnEnd) {
                size_t svalPos = obj.find(patSval, urnEnd);
                if (svalPos != std::string::npos) {
                    size_t colon = obj.find(':', svalPos);
                    if (colon != std::string::npos) {
                        size_t firstQuote = obj.find('"', colon);
                        if (firstQuote != std::string::npos) {
                            size_t secondQuote = obj.find('"', firstQuote + 1);
                            if (secondQuote != std::string::npos) {
                                return Utf8ToW(obj.substr(firstQuote + 1, secondQuote - firstQuote - 1));
                            }
                        }
                    }
                }
            }
        }
        pos = obj.find(patLabel, pos + patLabel.size());
    }
    return L"";
}

static std::vector<int> ParseIntArray(const std::string& obj, const char* key) {
    std::vector<int> vals;
    size_t kp = obj.find(key);
    if (kp == std::string::npos) return vals;
    size_t lb = obj.find('[', kp);
    size_t rb = obj.find(']', lb == std::string::npos ? 0 : lb);
    if (lb == std::string::npos || rb == std::string::npos) return vals;
    std::string arr = obj.substr(lb + 1, rb - lb - 1);
    size_t i = 0;
    while (i < arr.size()) {
        while (i < arr.size() && !isdigit((unsigned char)arr[i])) ++i;
        if (i >= arr.size()) break;
        int v = 0;
        while (i < arr.size() && isdigit((unsigned char)arr[i])) {
            v = v * 10 + (arr[i] - '0');
            ++i;
        }
        vals.push_back(v);
    }
    return vals;
}

static std::vector<double> ParseDoubleArray(const std::string& obj, const char* key) {
    std::vector<double> vals;
    size_t kp = obj.find(key);
    if (kp == std::string::npos) return vals;
    size_t lb = obj.find('[', kp);
    size_t rb = obj.find(']', lb == std::string::npos ? 0 : lb);
    if (lb == std::string::npos || rb == std::string::npos) return vals;
    std::string arr = obj.substr(lb + 1, rb - lb - 1);
    size_t i = 0;
    while (i < arr.size()) {
        while (i < arr.size() &&
            !isdigit((unsigned char)arr[i]) &&
            arr[i] != '-' && arr[i] != '.') ++i;
        if (i >= arr.size()) break;
        std::string num;
        while (i < arr.size() &&
            (isdigit((unsigned char)arr[i]) || arr[i] == '-' || arr[i] == '.' ||
                arr[i] == 'E' || arr[i] == 'e' || arr[i] == '+')) {
            num.push_back(arr[i]);
            ++i;
        }
        if (!num.empty()) vals.push_back(strtod(num.c_str(), nullptr));
    }
    return vals;
}

static std::wstring FindSmilesInProps(const std::string& obj) {
    size_t propsPos = obj.find("\"props\"");
    if (propsPos == std::string::npos) return L"";

    size_t lb = obj.find('[', propsPos);
    if (lb == std::string::npos) return L"";

    size_t afterProps = 0;
    std::string propsArray = ExtractArrayObjectBlock(obj, lb, afterProps);
    if (propsArray.empty()) return L"";

    auto propObjs = SplitTopLevelObjects(propsArray);
    for (auto& prop : propObjs) {
        if (prop.find("\"label\"") == std::string::npos) continue;
        if (prop.find("SMILES") == std::string::npos) continue;
        if (prop.find("\"name\"") == std::string::npos) continue;
        if (prop.find("Absolute") == std::string::npos &&
            prop.find("Connectivity") == std::string::npos)
            continue;

        size_t svalKey = prop.find("\"sval\"");
        if (svalKey == std::string::npos) continue;
        size_t colon = prop.find(':', svalKey);
        if (colon == std::string::npos) continue;
        size_t firstQuote = prop.find('"', colon);
        if (firstQuote == std::string::npos) continue;
        size_t secondQuote = prop.find('"', firstQuote + 1);
        if (secondQuote == std::string::npos) continue;

        return Utf8ToW(prop.substr(firstQuote + 1, secondQuote - firstQuote - 1));
    }
    return L"";
}

static std::string FindPropValueInProps(const std::string& obj, const std::string& label) {
    size_t propsPos = obj.find("\"props\"");
    if (propsPos == std::string::npos) return "";
    size_t lb = obj.find('[', propsPos);
    if (lb == std::string::npos) return "";
    size_t afterProps = 0;
    std::string propsArray = ExtractArrayObjectBlock(obj, lb, afterProps);
    if (propsArray.empty()) return "";
    auto propObjs = SplitTopLevelObjects(propsArray);
    for (auto& prop : propObjs) {
        if (prop.find("\"label\"") == std::string::npos) continue;
        //Loose match label appears somewhere in the label string
        if (prop.find(label) == std::string::npos) continue;

        size_t svalKey = prop.find("\"sval\"");
        if (svalKey != std::string::npos) {
            size_t colon = prop.find(':', svalKey);
            if (colon != std::string::npos) {
                size_t firstQuote = prop.find('"', colon);
                if (firstQuote != std::string::npos) {
                    size_t secondQuote = prop.find('"', firstQuote + 1);
                    if (secondQuote != std::string::npos)
                        return prop.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                }
            }
        }
        size_t fvalKey = prop.find("\"fval\"");
        if (fvalKey != std::string::npos) {
            size_t colon = prop.find(':', fvalKey);
            if (colon != std::string::npos) {
                size_t start = colon + 1;
                while (start < prop.size() && isspace((unsigned char)prop[start])) ++start;
                size_t end = start;
                while (end < prop.size() &&
                    (isdigit((unsigned char)prop[end]) || prop[end] == '.' || prop[end] == '-' ||
                        prop[end] == 'E' || prop[end] == 'e' || prop[end] == '+')) ++end;
                if (end > start) return prop.substr(start, end - start);
            }
        }
    }
    return "";
}

static std::vector<PubChemCompound> ParseRecord(const std::string& json) {
    std::vector<PubChemCompound> compounds;
    size_t pc = json.find("\"PC_Compounds\"");
    if (pc == std::string::npos) return compounds;
    size_t arrStart = json.find('[', pc);
    if (arrStart == std::string::npos) return compounds;
    size_t afterArray = 0;
    std::string arrayBlock = ExtractArrayObjectBlock(json, arrStart, afterArray);
    if (arrayBlock.empty()) return compounds;
    auto objs = SplitTopLevelObjects(arrayBlock);

    static const double atomicWeights[] = {
        0.0, 1.00794, 4.002602, 6.941, 9.012182, 10.811, 12.0107, 14.0067, 15.9994, 18.9984032, 20.1797,
        22.98976928, 24.3050, 26.9815386, 28.0855, 30.973762, 32.065, 35.453, 39.948, 39.0983, 40.078,
        44.955912, 47.867, 50.9415, 51.9961, 54.938045, 55.845, 58.933195, 58.6934, 63.546, 65.38,
        69.723, 72.64, 74.92160, 78.96, 79.904, 83.798
    };

    for (auto& obj : objs) {
        PubChemCompound c{};
        // CID
        {
            std::string cidPat = "\"cid\":";
            size_t cp = obj.find(cidPat);
            if (cp != std::string::npos) {
                cp += cidPat.size();
                while (cp < obj.size() && isspace((unsigned char)obj[cp])) ++cp;
                unsigned int cid = 0;
                while (cp < obj.size() && isdigit((unsigned char)obj[cp])) {
                    cid = cid * 10 + (obj[cp] - '0');
                    ++cp;
                }
                c.cid = cid;
            }
        }
        // Names
        c.name = FindStringValue(obj, "IUPAC Name", "Preferred");
        if (c.name.empty()) c.name = FindStringValue(obj, "IUPAC Name", "Traditional");
        if (c.name.empty()) c.name = FindStringValue(obj, "IUPAC Name", "Systematic");
        if (c.name.empty()) c.name = FindStringValue(obj, "IUPAC Name", "Allowed");

        // SMILES 
        c.smiles = FindSmilesInProps(obj);

        //Try to extract molecular formula and molecular weight from props
        {
            std::string f = FindPropValueInProps(obj, "Molecular Formula");
            if (f.empty()) f = FindPropValueInProps(obj, "MolecularFormula");
            if (!f.empty()) c.formula = Utf8ToW(f);

            std::string mw = FindPropValueInProps(obj, "Molecular Weight");
            if (mw.empty()) mw = FindPropValueInProps(obj, "MolecularWeight");
            if (mw.empty()) mw = FindPropValueInProps(obj, "Molecular Mass");
            if (!mw.empty()) c.molecularWeight = strtod(mw.c_str(), nullptr);
        }

        // Atoms coords
        auto elements = ParseIntArray(obj, "\"element\"");
        auto xs = ParseDoubleArray(obj, "\"x\"");
        auto ys = ParseDoubleArray(obj, "\"y\"");
        auto zs = ParseDoubleArray(obj, "\"z\"");
        size_t atomCount = elements.size();
        c.atoms.reserve(atomCount);
        for (size_t i = 0; i < atomCount; ++i) {
            PubChemAtom a{};
            a.atomicNumber = elements[i];
            a.x = (i < xs.size() ? xs[i] : 0.0);
            a.y = (i < ys.size() ? ys[i] : 0.0);
            a.z = (i < zs.size() ? zs[i] : 0.0);
            c.atoms.push_back(a);
        }

        //Bonds
        auto aid1 = ParseIntArray(obj, "\"aid1\"");
        auto aid2 = ParseIntArray(obj, "\"aid2\"");
        auto order = ParseIntArray(obj, "\"order\"");
        size_t nb = std::min(aid1.size(), aid2.size());
        c.bonds.reserve(nb);
        for (size_t bi = 0; bi < nb; ++bi) {
            PubChemBond b{};
            b.a1 = aid1[bi] - 1;
            b.a2 = aid2[bi] - 1;
            b.order = (bi < order.size() ? order[bi] : 1);
            if (b.a1 >= 0 && b.a2 >= 0 &&
                b.a1 < (int)c.atoms.size() && b.a2 < (int)c.atoms.size())
                c.bonds.push_back(b);
        }

        // heavy atom count
        int heavy = 0;
        for (auto& a : c.atoms) if (a.atomicNumber != 1) ++heavy;
        c.heavyAtomCount = heavy;

        if (c.molecularWeight <= 0.0) {
            double sum = 0.0;
            for (auto& a : c.atoms) {
                int z = a.atomicNumber;
                if (z > 0 && z < (int)(sizeof(atomicWeights) / sizeof(atomicWeights[0])))
                    sum += atomicWeights[z];
                else
                    sum += 12.0; 
            }
            c.molecularWeight = sum;
        }

        // Rotatable bond count
        {
            std::string rb = FindPropValueInProps(obj, "Rotatable Bond Count");
            if (rb.empty()) rb = FindPropValueInProps(obj, "Rotatable Bonds");
            if (rb.empty()) rb = FindPropValueInProps(obj, "rotatableBondCount");
            if (!rb.empty()) c.rotatableBondCount = (int)strtol(rb.c_str(), nullptr, 10);
        }

        if (c.cid != 0) compounds.push_back(c);
    }
    return compounds;
}

//Main formula query
PubChemFormulaResult QueryPubChemFormulaRecords(const std::wstring& formula, size_t topN) {
    PubChemFormulaResult res{};
    res.formula = formula;
    if (topN == 0) topN = 3;

    std::wstring path1 = L"/rest/pug/compound/fastformula/" + formula + L"/cids/JSON";
    std::string cidsJson;
    if (!HttpGet(L"pubchem.ncbi.nlm.nih.gov", path1, cidsJson, res.statusCids)) {
        res.error = L"Network failure (fastformula)";
        return res;
    }
    res.cidsHead = Utf8ToW(cidsJson.substr(0, 300));
    if (res.statusCids != 200) {
        res.error = L"CID request status " + std::to_wstring(res.statusCids);
        return res;
    }

    auto allCids = ParseCIDArray(cidsJson);
    if (allCids.empty()) {
        res.error = L"No CIDs for formula";
        return res;
    }
    if (allCids.size() > topN) allCids.resize(topN);
    res.topCids = allCids;

    std::wstringstream list;
    for (size_t i = 0; i < allCids.size(); ++i) {
        list << allCids[i];
        if (i + 1 < allCids.size()) list << L",";
    }

    std::wstring path2 = L"/rest/pug/compound/cid/" + list.str() + L"/record/JSON";
    std::string recordJson;
    if (!HttpGet(L"pubchem.ncbi.nlm.nih.gov", path2, recordJson, res.statusRecord) ||
        res.statusRecord != 200) {
        res.error = L"Record status " + std::to_wstring(res.statusRecord);
        return res;
    }

    OutputDebugStringA(recordJson.c_str());

    res.recordHead = Utf8ToW(recordJson.substr(0, 300));
    res.compounds = ParseRecord(recordJson);

    if (res.compounds.empty()) {
        res.error = L"Parsed zero compounds";
        res.ok = false;
        return res;
    }

    res.ok = true;
    return res;
}