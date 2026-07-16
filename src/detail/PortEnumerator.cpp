#include "PortEnumerator.hpp"

#include <algorithm>
#include <string>
#include <cctype>
#include <cstdio>
#include <cstring>

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
#  include <dirent.h>
#  include <cstdio>
#  include <fstream>
#  include <cstdlib>
#  define CPS_ENUM_POSIX 1
#elif defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <setupapi.h>
#  include <initguid.h>
#  include <devpkey.h>
#  include <vector>
#  include <string>
#  pragma comment(lib, "setupapi.lib")
#  define CPS_ENUM_WIN 1
#endif

namespace cps {
namespace detail {

#if defined(CPS_ENUM_POSIX)

static bool readSysFile(const std::string& path, std::string& out) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) return false;
    std::getline(f, out);
    return true;
}

static void findAttrUpwards(std::string dir, const std::string& attr, std::string& out) {
    for (int i = 0; i < 10 && !dir.empty(); ++i) {
        std::string p = dir + "/" + attr;
        if (readSysFile(p, out) && !out.empty()) return;
        std::size_t pos = dir.find_last_of('/');
        if (pos == std::string::npos) break;
        dir.resize(pos);
    }
    out.clear();
}

static int hexToInt(const std::string& s) {
    if (s.empty()) return 0;
    try { return static_cast<int>(std::stoul(s, nullptr, 16)); }
    catch (...) { return 0; }
}

static bool looksSerial(const std::string& name) {
    static const char* prefixes[] = {
        "ttyUSB", "ttyACM", "ttyS", "ttyAMA", "ttyXRUSB", "ttyMAX", "ttyHS",
        "ttyMSL", "ttyO", "ttyAT", "rfcomm", "ttyGS", "ttyMI", "ttySPI"
    };
    for (auto p : prefixes)
        if (name.compare(0, std::strlen(p), p) == 0) return true;
    return false;
}

std::vector<RawPortInfo> enumeratePorts() {
    std::vector<RawPortInfo> out;
    DIR* dir = ::opendir("/sys/class/tty");
    if (!dir) return out;

    struct dirent* ent;
    while ((ent = ::readdir(dir)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        if (!looksSerial(name)) continue;

        std::string devLink = "/sys/class/tty/" + name + "/device";
        char* rp = ::realpath(devLink.c_str(), nullptr);
        if (!rp) continue;          // not a real hardware device
        std::string deviceDir = rp;
        std::free(rp);

        RawPortInfo info;
        info.portName = std::string("/dev/") + name;
        info.systemLocation = info.portName;

        std::string vid, pid, manuf, product, serial, iface;
        findAttrUpwards(deviceDir, "idVendor", vid);
        findAttrUpwards(deviceDir, "idProduct", pid);
        findAttrUpwards(deviceDir, "manufacturer", manuf);
        findAttrUpwards(deviceDir, "product", product);
        findAttrUpwards(deviceDir, "serial", serial);
        findAttrUpwards(deviceDir, "interface", iface);

        info.description   = product.empty() ? iface : product;
        info.manufacturer  = manuf;
        info.serialNumber  = serial;
        if (!vid.empty() && !pid.empty()) {
            info.vendorId  = static_cast<std::uint16_t>(hexToInt(vid));
            info.productId = static_cast<std::uint16_t>(hexToInt(pid));
            info.hasUsbIds = (info.vendorId != 0 && info.productId != 0);
        }
        out.push_back(std::move(info));
    }
    ::closedir(dir);

    std::sort(out.begin(), out.end(), [](const RawPortInfo& a, const RawPortInfo& b) {
        return a.portName < b.portName;
    });
    return out;
}

#elif defined(CPS_ENUM_WIN)

#ifndef GUID_DEVINTERFACE_COMPORT
DEFINE_GUID(GUID_DEVINTERFACE_COMPORT, 0x86E0D1E0L, 0x8089, 0x11D0, 0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73);
#endif

static std::string winNarrow(const wchar_t* w) {
    if (!w || !*w) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return std::string();
    std::string s(static_cast<std::size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], len, nullptr, nullptr);
    return s;
}

// "USB Serial Port (COM3)" -> "COM3" ; returns empty when no "COM<digit>" token.
static std::string extractComName(const wchar_t* w) {
    if (!w) return std::string();
    for (std::size_t i = 0; w[i]; ++i) {
        if (w[i] == L'C' && w[i + 1] == L'O' && w[i + 2] == L'M' &&
            w[i + 3] >= L'0' && w[i + 3] <= L'9') {
            std::size_t e = i + 3;
            while (w[e] >= L'0' && w[e] <= L'9') ++e;
            std::wstring token(w + i, w + e);
            return winNarrow(token.c_str());
        }
    }
    return std::string();
}

static bool readRegStr(HDEVINFO hdev, PSP_DEVINFO_DATA did, DWORD prop,
                       wchar_t* buf, DWORD bufCch) {
    buf[0] = L'\0';
    return SetupDiGetDeviceRegistryPropertyW(hdev, did, prop, nullptr,
                                             reinterpret_cast<PBYTE>(buf),
                                             bufCch * sizeof(wchar_t), nullptr) != 0;
}

static std::uint16_t extractHexId(const std::string& upper, const std::string& key) {
    auto p = upper.find(key);
    if (p == std::string::npos) return 0;
    unsigned v = 0;
    std::sscanf(upper.c_str() + p + key.size(), "%4x", &v);
    return static_cast<std::uint16_t>(v);
}

std::vector<RawPortInfo> enumeratePorts() {
    std::vector<RawPortInfo> out;
    HDEVINFO hdev = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdev == INVALID_HANDLE_VALUE) return out;

    SP_DEVINFO_DATA devInfo{};
    devInfo.cbSize = sizeof(devInfo);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hdev, i, &devInfo); ++i) {
        wchar_t friendly[512] = {0};
        wchar_t devDesc[512]  = {0};
        wchar_t mfg[512]      = {0};
        wchar_t hwid[1024]    = {0};

        readRegStr(hdev, &devInfo, SPDRP_FRIENDLYNAME, friendly, 512);
        readRegStr(hdev, &devInfo, SPDRP_DEVICEDESC,  devDesc, 512);

        std::string comName = extractComName(friendly[0] ? friendly : devDesc);
        if (comName.empty()) continue;  // device exposes no openable COMx name

        RawPortInfo info;
        info.portName       = comName;
        info.systemLocation = std::string("\\\\.\\") + comName;
        info.description    = winNarrow(devDesc[0] ? devDesc : friendly);

        if (readRegStr(hdev, &devInfo, SPDRP_MFG, mfg, 512))
            info.manufacturer = winNarrow(mfg);

        if (readRegStr(hdev, &devInfo, SPDRP_HARDWAREID, hwid, 1024)) {
            std::string s = winNarrow(hwid);
            std::string upper; upper.reserve(s.size());
            for (char c : s) upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            info.vendorId  = extractHexId(upper, "VID_");
            info.productId = extractHexId(upper, "PID_");
            info.hasUsbIds = (info.vendorId != 0 && info.productId != 0);
        }

        out.push_back(std::move(info));
    }
    SetupDiDestroyDeviceInfoList(hdev);
    return out;
}

#else
#  warning "No native serial enumeration for this platform; returning empty list."
std::vector<RawPortInfo> enumeratePorts() { return {}; }
#endif

} // namespace detail
} // namespace cps
