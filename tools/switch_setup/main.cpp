// Separate Switch setup. Layout, Barlow face and drawn room follow main.c first_run_wizard.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <propkey.h>
#include <wininet.h>
#include <bcrypt.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "../../src/app/launcher_fonts_data.h"
#include "disc_identity.h"

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
static HWND window, browse_button, usb_button, net_button, install_button, reuse_button, ip_edit,
    port_edit, next_button, back_button, exit_button;
static HFONT body_font, heading_font, title_font, action_font;
static HBRUSH ground_brush, panel_brush;
static std::wstring iso, status = L"Choose your disc to begin.", detail;
static bool network_mode, reuse_iso = true, busy;
static int percent = -1, pulse, page;
static HWND hover_control;
static float hover[110]{};
static std::atomic<bool> cancelled;
static thread_local fs::path diagnostic_log;
static thread_local std::wstring diagnostic_stage;
static const COLORREF ground = RGB(254, 254, 253), panel = RGB(242, 242, 245),
                      text = RGB(38, 40, 45), dim = RGB(110, 113, 120), accent = RGB(90, 94, 102),
                      rule = RGB(200, 202, 208);
enum { BROWSE = 100, USB, NETWORK, INSTALL, REUSE, IP, PORT, NEXT, BACK, EXIT };
static constexpr UINT STATUS = WM_APP + 1, PROGRESS = WM_APP + 2, FINISHED = WM_APP + 3;

static std::wstring wide(const std::string &s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}
static std::string utf8(const std::wstring &s)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}
static void require(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
static void checked(HRESULT hr, const char *message)
{
    if (FAILED(hr)) {
        std::ostringstream error;
        error << message << " (HRESULT 0x" << std::hex << (uint32_t)hr << ")";
        throw std::runtime_error(error.str());
    }
}
static void check_cancel()
{
    require(!cancelled.load(), "Operation cancelled; saves preserved.");
}
static void tell(const std::wstring &message)
{
    diagnostic_stage=message;
    if(!diagnostic_log.empty()) {
        std::ofstream log(diagnostic_log,std::ios::app);
        log << GetTickCount64() << " " << utf8(message) << "\n";
    }
    if (window)
        PostMessageW(window, STATUS, 0, (LPARAM) new std::wstring(message));
}
static void progress(int value)
{
    if (window)
        PostMessageW(window, PROGRESS, (WPARAM)value, 0);
}
static std::string read_file(const fs::path &path)
{
    std::ifstream f(path, std::ios::binary);
    require((bool)f, "Could not read a package file.");
    return std::string(std::istreambuf_iterator<char>(f), {});
}
static void write_file(const fs::path &path, const std::string &data)
{
    std::ofstream f(path, std::ios::binary);
    f.write(data.data(), data.size());
    require((bool)f, "Could not write a temporary file.");
}
static std::string json_string(const std::string &data, const std::string &name)
{
    size_t p = data.find("\"" + name + "\"");
    require(p != std::string::npos, "Incomplete package manifest.");
    p = data.find(':', p);
    p = data.find('"', p);
    size_t end = data.find('"', p + 1);
    require(p != std::string::npos && end != std::string::npos, "Invalid package manifest.");
    return data.substr(p + 1, end - p - 1);
}
struct Internet {
    HINTERNET handle{};
    explicit Internet(HINTERNET h = nullptr) : handle(h)
    {
    }
    ~Internet()
    {
        if (handle)
            InternetCloseHandle(handle);
    }
    Internet(const Internet &) = delete;
};
struct Hash {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    explicit Hash(bool sha1)
    {
        checked(BCryptOpenAlgorithmProvider(
                    &algorithm, sha1 ? BCRYPT_SHA1_ALGORITHM : BCRYPT_SHA256_ALGORITHM, nullptr, 0),
                "Hash provider unavailable.");
        checked(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0),
                "Could not initialize verification.");
    }
    ~Hash()
    {
        if (hash)
            BCryptDestroyHash(hash);
        if (algorithm)
            BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    void add(unsigned char *data, DWORD size)
    {
        checked(BCryptHashData(hash, data, size, 0), "Hash verification failed.");
    }
    std::string finish(bool sha1)
    {
        std::vector<unsigned char> bytes(sha1 ? 20 : 32);
        checked(BCryptFinishHash(hash, bytes.data(), (ULONG)bytes.size(), 0),
                "Hash verification failed.");
        std::string out;
        const char *hex = "0123456789abcdef";
        for (unsigned char b : bytes) {
            out += hex[b >> 4];
            out += hex[b & 15];
        }
        return out;
    }
};
static std::string file_hash(const fs::path &path, bool sha1 = false)
{
    std::ifstream f(path, std::ios::binary);
    require((bool)f, "File not found.");
    uint64_t size = fs::file_size(path), done = 0;
    std::vector<unsigned char> data(1024 * 1024);
    Hash hash(sha1);
    int last = -1;
    while (f) {
        check_cancel();
        f.read((char *)data.data(), data.size());
        DWORD n = (DWORD)f.gcount();
        hash.add(data.data(), n);
        done += n;
        int value = (int)(done * 100 / std::max<uint64_t>(1, size));
        if (value != last) {
            progress(value);
            last = value;
        }
    }
    require(f.eof(), "Could not read the file during verification.");
    return hash.finish(sha1);
}
static std::string disc_config(std::string data, const std::string &directory)
{
    std::istringstream lines(data);
    std::string out, line;
    bool disc = false, saves = false, show = false;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.rfind("disc=", 0) == 0) {
            line = "disc=sdmc:/switch/" + directory + "/gt4.iso";
            disc = true;
        }
        saves |= line.rfind("saves=", 0) == 0;
        show |= line.rfind("show=", 0) == 0;
        out += line + "\n";
    }
    if (!disc)
        out += "disc=sdmc:/switch/" + directory + "/gt4.iso\n";
    if (!saves)
        out += "saves=sdmc:/switch/" + directory + "/\n";
    if (!show)
        out += "show=0\n";
    return out;
}
static ComPtr<IShellItem> shell_item(const fs::path &path)
{
    ComPtr<IShellItem> item;
    fs::path native_path=path;native_path.make_preferred();
    checked(SHCreateItemFromParsingName(native_path.c_str(), nullptr, IID_PPV_ARGS(&item)),
            "Windows could not find the file or folder.");
    return item;
}
static std::wstring name_of(IShellItem *item)
{
    PWSTR name = nullptr;
    checked(item->GetDisplayName(SIGDN_NORMALDISPLAY, &name), "Could not identify the USB item.");
    std::wstring result = name;
    CoTaskMemFree(name);
    return result;
}
static std::vector<ComPtr<IShellItem>> children(IShellItem *parent)
{
    ComPtr<IEnumShellItems> items;
    if (FAILED(parent->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&items))))
        return {};
    std::vector<ComPtr<IShellItem>> out;
    ComPtr<IShellItem> item;
    while (items->Next(1, &item, nullptr) == S_OK) {
        out.push_back(item);
        item.Reset();
    }
    return out;
}
static ComPtr<IShellItem> child(IShellItem *parent, const std::wstring &name)
{
    for (auto &item : children(parent))
        if (name_of(item.Get()) == name)
            return item;
    return {};
}
static ComPtr<IFileOperation> operation(bool quiet = false)
{
    ComPtr<IFileOperation> op;
    checked(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&op)),
            "Windows file transfer is unavailable.");
    checked(op->SetOperationFlags(quiet ? 0x614 : FOF_NOCONFIRMMKDIR),
            "Could not prepare the file transfer.");
    if (window)
        op->SetOwnerWindow(window);
    return op;
}
static void perform(IFileOperation *op)
{
    check_cancel();
    checked(op->PerformOperations(), "The USB transfer did not complete.");
    BOOL aborted = FALSE;
    checked(op->GetAnyOperationsAborted(&aborted), "Could not verify the transfer.");
    require(!aborted, "Transfer cancelled in Windows.");
    check_cancel();
}
static ComPtr<IShellItem> wait_child(IShellItem *parent,const std::wstring &name)
{
    for(int attempt=0;attempt<100;attempt++) {
        check_cancel();
        ComPtr<IShellItem2> refresh;
        if(SUCCEEDED(parent->QueryInterface(IID_PPV_ARGS(&refresh)))) refresh->Update(nullptr);
        auto item=child(parent,name);
        if(item) return item;
        Sleep(100);
    }
    throw std::runtime_error("DBI did not publish the copied item: " + utf8(name));
}
static ComPtr<IShellItem> directory(IShellItem *parent, const std::wstring &name)
{
    auto item = child(parent, name);
    if (item)
        return item;
    auto op = operation();
    checked(op->NewItem(parent, FILE_ATTRIBUTE_DIRECTORY, name.c_str(), nullptr, nullptr),
            "Could not create the SD card folder.");
    perform(op.Get());
    item = wait_child(parent, name);
    return item;
}
static void copy(IShellItem *source, IShellItem *target, const std::wstring &name,
                 bool quiet = false)
{
    auto op = operation(quiet);
    checked(op->CopyItem(source, target, name.c_str(), nullptr),
            "Could not prepare the USB transfer.");
    perform(op.Get());
}
static ComPtr<IShellItem> sd_card()
{
    ComPtr<IShellItem> computer;
    checked(SHGetKnownFolderItem(FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, nullptr,
                                 IID_PPV_ARGS(&computer)),
            "Could not list connected devices.");
    ComPtr<IShellItem> found;
    for (auto &device : children(computer.Get())) {
        SFGAOF attributes = SFGAO_FILESYSTEM;
        device->GetAttributes(SFGAO_FILESYSTEM, &attributes);
        if (attributes & SFGAO_FILESYSTEM)
            continue;
        for (auto &item : children(device.Get()))
            if (name_of(item.Get()) == L"1: SD Card") {
                require(!found, "Connect only one Switch at a time.");
                found = item;
            }
    }
    require(found != nullptr,
            "DBI not found. Open Start MTP Connection and connect the USB cable.");
    return found;
}
static uint64_t shell_size(IShellItem *item)
{
    ComPtr<IShellItem2> value;
    checked(item->QueryInterface(IID_PPV_ARGS(&value)), "USB file size is unavailable.");
    value->Update(nullptr);
    ULONGLONG size = 0;
    checked(value->GetUInt64(PKEY_Size, &size), "DBI did not report the file size.");
    return size;
}
static ComPtr<IShellItem> wait_file(IShellItem *parent,const std::wstring &name,uint64_t expected,int attempts=100)
{
    std::string last="DBI did not publish " + utf8(name);
    for(int attempt=0;attempt<attempts;attempt++) {
        check_cancel();
        ComPtr<IShellItem2> refresh;
        if(SUCCEEDED(parent->QueryInterface(IID_PPV_ARGS(&refresh)))) refresh->Update(nullptr);
        auto item=child(parent,name);
        if(item) try {
            uint64_t actual=shell_size(item.Get());
            if(actual==expected) return item;
            last="DBI reports " + utf8(name) + " size " + std::to_string(actual) +
                 "; expected " + std::to_string(expected) + ". Check free space/exFAT.";
        } catch(const std::exception &e) { last=e.what(); }
        Sleep(100);
    }
    throw std::runtime_error(last);
}
struct Job {
    fs::path root, iso, temp;
    std::wstring ip;
    INTERNET_PORT port;
    bool network, reuse;
};
static void install_usb_to(const Job &job, const fs::path &nro, const std::string &hash,
                           const std::wstring &stamp, IShellItem *sd)
{
    auto parent = directory(sd, L"switch");
    std::wstring folder = L"moonengine";
    auto app = child(parent.Get(), folder);
    if (!app) {
        app = child(parent.Get(), L"opengt4");
        if (app)
            folder = L"opengt4";
    }
    if (!app)
        app = directory(parent.Get(), folder);
    auto backups = directory(sd, L"moonengine-backups"), local = shell_item(job.temp);
    auto existing = child(app.Get(), L"gt4.iso");
    if (!existing || !job.reuse) {
        tell(L"Copying the ISO over USB. Windows will display transfer progress.");
        copy(shell_item(job.iso).Get(), app.Get(), L"gt4.iso");
    }
    tell(L"Waiting for DBI to confirm the ISO size (the existing copy is kept)...");
    existing = wait_file(app.Get(), L"gt4.iso", fs::file_size(job.iso));
    std::wstring filename = folder + L".nro";
    existing = child(app.Get(), filename);
    if (existing)
        copy(existing.Get(), backups.Get(), L"before-" + stamp + L".nro.disabled");
    tell(L"Installing the app and verifying the transfer...");
    copy(shell_item(nro).Get(), app.Get(), filename, true);
    existing = wait_file(app.Get(), filename, fs::file_size(nro));
    copy(existing.Get(), local.Get(), L"verified.nro", true);
    require(file_hash(job.temp / L"verified.nro") == hash,
            "The installed NRO failed verification.");
    tell(L"Preserving settings and writing the disc path...");
    std::string config;
    existing = child(app.Get(), L"config.ini");
    if (existing) {
        copy(existing.Get(), local.Get(), L"old.ini", true);
        config = read_file(job.temp / L"old.ini");
        copy(existing.Get(), backups.Get(), L"config-" + stamp + L".ini.bak");
    }
    write_file(job.temp / L"config.ini", disc_config(config, utf8(folder)));
    copy(shell_item(job.temp / L"config.ini").Get(), app.Get(), L"config.ini", true);
}

static void install_usb(const Job &job,const fs::path &nro,const std::string &hash,const std::wstring &stamp)
{
    tell(L"Looking for the SD card in DBI...");
    auto sd=sd_card();install_usb_to(job,nro,hash,stamp,sd.Get());
}

class Ftp {
    Internet internet, connection;

  public:
    explicit Ftp(const Job &job)
        : internet(
              InternetOpenW(L"MoonEngine-Switch", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0))
    {
        require(internet.handle != nullptr, "Network unavailable.");
        DWORD timeout = 15000;
        for (DWORD option : {INTERNET_OPTION_CONNECT_TIMEOUT, INTERNET_OPTION_RECEIVE_TIMEOUT,
                             INTERNET_OPTION_SEND_TIMEOUT})
            InternetSetOptionW(internet.handle, option, &timeout, sizeof timeout);
        connection.handle =
            InternetConnectW(internet.handle, job.ip.c_str(), job.port, L"anonymous",
                             L"moonengine@", INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0);
        if(!connection.handle) checked(HRESULT_FROM_WIN32(GetLastError()),
                "FTP did not respond. Check the IP/port and enable FTP in Sphaira.");
    }
    bool exists_dir(const std::wstring &path)
    {
        bool ok = FtpSetCurrentDirectoryW(connection.handle, path.c_str()) != FALSE;
        FtpSetCurrentDirectoryW(connection.handle, L"/");
        return ok;
    }
    void mkdir(const std::wstring &path)
    {
        if (!exists_dir(path))
            require(FtpCreateDirectoryW(connection.handle, path.c_str()) != FALSE,
                    "Could not create the folder over FTP.");
    }
    int64_t size(const std::wstring &path)
    {
        WIN32_FIND_DATAW data{};
        Internet item(
            FtpFindFirstFileW(connection.handle, path.c_str(), &data, INTERNET_FLAG_RELOAD, 0));
        if (!item.handle)
            return -1;
        require(!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY),
                "The destination is a folder; this setup requires a single ISO file on exFAT.");
        return (int64_t)(((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow);
    }
    void rename(const std::wstring &from, const std::wstring &to)
    {
        require(FtpRenameFileW(connection.handle, from.c_str(), to.c_str()) != FALSE,
                "Could not rename the FTP file. The backup was preserved.");
    }
    std::string hash(const std::wstring &path)
    {
        Internet file(FtpOpenFileW(
            connection.handle, path.c_str(), GENERIC_READ,
            FTP_TRANSFER_TYPE_BINARY | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0));
        require(file.handle != nullptr, "Could not read the NRO over FTP.");
        Hash digest(false);
        std::vector<unsigned char> data(1024 * 1024);
        DWORD n;
        do {
            check_cancel();
            require(InternetReadFile(file.handle, data.data(), (DWORD)data.size(), &n) != FALSE,
                    "FTP read failed.");
            digest.add(data.data(), n);
        } while (n);
        return digest.finish(false);
    }
    std::string text_file(const std::wstring &path)
    {
        if (size(path) < 0)
            return {};
        Internet file(FtpOpenFileW(
            connection.handle, path.c_str(), GENERIC_READ,
            FTP_TRANSFER_TYPE_BINARY | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0));
        require(file.handle != nullptr, "Could not read the configuration over FTP.");
        std::string out;
        char data[4096];
        DWORD n;
        do {
            require(InternetReadFile(file.handle, data, sizeof data, &n) != FALSE,
                    "Could not read the configuration.");
            out.append(data, n);
            require(out.size() < 1024 * 1024, "The configuration is larger than expected.");
        } while (n);
        return out;
    }
    void upload(const fs::path &source, const std::wstring &target)
    {
        std::ifstream input(source, std::ios::binary);
        require((bool)input, "Local file not found.");
        uint64_t total = fs::file_size(source), done = 0;
        std::vector<char> data(1024 * 1024);
        int last = -1;
        {
            Internet file(FtpOpenFileW(connection.handle, target.c_str(), GENERIC_WRITE,
                                       FTP_TRANSFER_TYPE_BINARY, 0));
            require(file.handle != nullptr, "Could not start the FTP upload.");
            while (input) {
                check_cancel();
                input.read(data.data(), data.size());
                DWORD n = (DWORD)input.gcount(), written = 0;
                if (n)
                    require(InternetWriteFile(file.handle, data.data(), n, &written) != FALSE &&
                                written == n,
                            "Incomplete FTP upload.");
                done += n;
                int value = (int)(done * 100 / std::max<uint64_t>(total, 1));
                if (value != last) {
                    progress(value);
                    last = value;
                }
            }
            require(input.eof(), "Could not read the local file.");
        }
        require(size(target) == (int64_t)total,
                "The remote file size does not match the uploaded file.");
    }
    void replace(const fs::path &source, const std::wstring &target, const std::wstring &backup,
                 const std::wstring &stamp, const std::string &expected = {})
    {
        std::wstring staged = target + L".upload-" + stamp;
        upload(source, staged);
        if (!expected.empty())
            require(hash(staged) == expected, "Upload verification failed.");
        check_cancel();
        bool old = size(target) >= 0;
        if (old)
            rename(target, backup);
        try {
            rename(staged, target);
        } catch (...) {
            if (old)
                rename(backup, target);
            throw;
        }
    }
};
static std::wstring quote(const std::wstring &value)
{
    std::wstring out = L"\"";
    int slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') {
            slashes++;
            continue;
        }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        out += c;
        slashes = 0;
    }
    out.append(slashes * 2, L'\\');
    return out + L'"';
}
static void nxlink(const Job &job, const fs::path &nro, const std::wstring &folder)
{
    std::wstring command = quote((job.root / L"installer/nxlink.exe").wstring()) + L" -a " +
                           job.ip + L" -r 5 -p " + folder + L"/" + folder + L".nro " +
                           quote(nro.wstring());
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE output=CreateFileW((job.temp/L"nxlink.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,
                              &security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(output!=INVALID_HANDLE_VALUE,"Could not create Nxlink diagnostic log.");
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                            &security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    STARTUPINFOW start{};start.cb=sizeof start;
    start.dwFlags=STARTF_USESTDHANDLES;start.hStdOutput=start.hStdError=output;start.hStdInput=input;
    PROCESS_INFORMATION process{};
    BOOL started=CreateProcessW(nullptr,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,
                                nullptr,job.root.c_str(),&start,&process);
    DWORD launch_error=GetLastError();CloseHandle(output);
    if(input!=INVALID_HANDLE_VALUE) CloseHandle(input);
    if(!started) checked(HRESULT_FROM_WIN32(launch_error),"Could not start Nxlink.");
    DWORD result = WaitForSingleObject(process.hProcess, 120000), code = 1;
    if (result == WAIT_TIMEOUT)
        TerminateProcess(process.hProcess, 1);
    else
        GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if(result==WAIT_TIMEOUT || code!=0)
        throw std::runtime_error("Files installed on SD; Nxlink launch failed (exit " +
            std::to_string(code) + "). Enable the Nxlink receiver in Sphaira. Details: " +
            utf8((job.temp/L"nxlink.log").wstring()));
}
static void install_network(const Job &job, const fs::path &nro, const std::string &expected,
                            const std::wstring &stamp, const std::string &manifest)
{
    require(file_hash(job.root / L"installer/nxlink.exe") == json_string(manifest, "nxlink_sha256"),
            "Nxlink does not match the package manifest.");
    std::wstring directory;
    {
        tell(L"Connecting to Sphaira FTP...");
        Ftp ftp(job);
        ftp.mkdir(L"/switch");
        ftp.mkdir(L"/moonengine-backups");
        directory = ftp.exists_dir(L"/switch/moonengine") ? L"moonengine"
                    : ftp.exists_dir(L"/switch/opengt4")  ? L"opengt4"
                                                          : L"moonengine";
        std::wstring folder = L"/switch/" + directory;
        ftp.mkdir(folder);
        int64_t existing = ftp.size(folder + L"/gt4.iso");
        if (existing < 0 || !job.reuse) {
            tell(L"Copying the ISO over FTP. Keep Sphaira open...");
            ftp.replace(job.iso, folder + L"/gt4.iso",
                        L"/moonengine-backups/gt4-" + stamp + L".iso.disabled", stamp);
        } else
            require(existing == (int64_t)fs::file_size(job.iso),
                    "The existing ISO has a different size. Turn off ISO reuse to replace it.");
        tell(L"Installing and verifying the app over the network...");
        ftp.replace(nro, folder + L"/" + directory + L".nro",
                    L"/moonengine-backups/before-" + stamp + L".nro.disabled", stamp, expected);
        fs::path config = job.temp / L"config.ini";
        write_file(config, disc_config(ftp.text_file(folder + L"/config.ini"), utf8(directory)));
        ftp.replace(config, folder + L"/config.ini",
                    L"/moonengine-backups/config-" + stamp + L".ini.bak", stamp, file_hash(config));
    }
    tell(L"Installed on the SD card. Sending and launching with Nxlink...");
    nxlink(job, nro, directory);
}

static void execute(Job job)
{
    std::wstring error;
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        fs::create_directories(job.temp);
        diagnostic_log=job.temp/L"setup.log";
        checked(com, "Could not initialize setup.");
        tell(job.network ? L"Starting network installation (FTP + Nxlink)." : L"Starting USB/DBI installation.");
        std::string manifest = read_file(job.root / L"manifest.json");
        fs::path nro = job.root / L"switch/moonengine/moonengine.nro";
        tell(L"Verifying the packaged app...");
        std::string expected = json_string(manifest, "nro_sha256");
        require(file_hash(nro) == expected, "The NRO does not match the package manifest.");
        tell(L"Checking disc serial SCUS-97328...");
        require(disc_is_scus97328(job.iso),
                "Unrecognized disc serial. Choose GT4 USA (SCUS-97328).");
        fs::create_directories(job.temp);
        std::wstring stamp = job.temp.filename().wstring();
        if (job.network)
            install_network(job, nro, expected, stamp, manifest);
        else
            install_usb(job, nro, expected, stamp);
        tell(L"Installation completed.");
    } catch (const std::exception &e) {
        error=diagnostic_stage + L"\n" + wide(e.what());
        tell(L"FAILED: " + error);
    }
    std::error_code log_error;
    fs::path report=job.root/L"MoonEngine-Switch-Setup.log";
    if(!diagnostic_log.empty()) {
        fs::copy_file(diagnostic_log,report,fs::copy_options::overwrite_existing,log_error);
        if(log_error) report=diagnostic_log;
        if(!error.empty()) error+=L"\nLog: " + report.wstring();
    }
    diagnostic_log.clear();diagnostic_stage.clear();
    if (SUCCEEDED(com))
        CoUninitialize();
    if(window) PostMessageW(window, FINISHED, 0, (LPARAM) new std::wstring(error));
}
static std::wstring control_text(HWND control)
{
    int n = GetWindowTextLengthW(control);
    std::wstring out(n + 1, 0);
    GetWindowTextW(control, out.data(), n + 1);
    out.resize(n);
    return out;
}
static fs::path program_root()
{
    wchar_t path[32768];
    GetModuleFileNameW(nullptr, path, 32768);
    return fs::path(path).parent_path();
}
static void start_install()
{
    if (busy) {
        cancelled = true;
        status = L"Cancelling safely...";
        InvalidateRect(window, nullptr, FALSE);
        return;
    }
    if (iso.empty()) {
        status = L"Choose your ISO first.";
        InvalidateRect(window, nullptr, FALSE);
        return;
    }
    Job job;
    job.root = program_root();
    job.iso = iso;
    job.ip = control_text(ip_edit);
    job.network = network_mode;
    job.reuse = reuse_iso;
    int port = _wtoi(control_text(port_edit).c_str());
    IN_ADDR address{};
    if (network_mode &&
        (InetPtonW(AF_INET, job.ip.c_str(), &address) != 1 || port < 1 || port > 65535)) {
        status = L"Enter your Switch IPv4 address and a valid FTP port.";
        InvalidateRect(window, nullptr, FALSE);
        return;
    }
    job.port = (INTERNET_PORT)port;
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    job.temp = fs::path(temp) / (L"moon-switch-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                 std::to_wstring(GetTickCount64()));
    cancelled = false;
    busy = true;
    percent = -1;
    status = L"Preparing installation...";
    detail.clear();
    for (HWND control :
         {browse_button, usb_button, net_button, reuse_button, back_button, ip_edit, port_edit})
        EnableWindow(control, FALSE);
    SetWindowTextW(install_button, L"CANCEL");
    InvalidateRect(window, nullptr, FALSE);
    std::thread(execute, job).detach();
}
static void draw_text(HDC dc, const std::wstring &value, RECT bounds, HFONT font, COLORREF color,
                      UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE)
{
    SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, value.c_str(), (int)value.size(), &bounds, flags);
}
static void fill(HDC dc, RECT rect, COLORREF color)
{
    HBRUSH b = CreateSolidBrush(color);
    FillRect(dc, &rect, b);
    DeleteObject(b);
}
// Same 640x448 layout scaled by 1.5 as the launcher's first-run screen.
static void gradient(HDC dc, int top, int bottom, COLORREF a, COLORREF b)
{
    for (int y = top; y < bottom; y++) {
        int t = y - top, n = bottom - top;
        fill(dc, {0, y, 960, y + 1},
             RGB((GetRValue(a) * (n - t) + GetRValue(b) * t) / n,
                 (GetGValue(a) * (n - t) + GetGValue(b) * t) / n,
                 (GetBValue(a) * (n - t) + GetBValue(b) * t) / n));
    }
}
static void paint(HDC dc)
{
    fill(dc, {0, 0, 960, 672}, ground);
    gradient(dc, 375, 600, ground, panel);
    draw_text(dc, L"MOON ENGINE", {24, 16, 455, 62}, title_font, text);
    draw_text(dc, L"Nintendo Switch  /  Private setup", {25, 65, 455, 87}, body_font, dim);
    draw_text(dc, iso.empty() ? L"No disc chosen" : L"Disc selected", {620, 22, 935, 46}, body_font,
              text, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SIZE label;
    const wchar_t *disc_state = iso.empty() ? L"No disc chosen" : L"Disc selected";
    GetTextExtentPoint32W(dc, disc_state, (int)wcslen(disc_state), &label);
    fill(dc, {935 - label.cx - 18, 30, 935 - label.cx - 10, 38},
         iso.empty() ? RGB(205, 50, 45) : RGB(40, 170, 80));
    draw_text(dc, page ? L"Setup  2 of 2  -  Connection" : L"Setup  1 of 2  -  Disc",
              {580, 50, 935, 76}, body_font, dim, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    const UINT center = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
    draw_text(dc, page ? L"Connect your Nintendo Switch." : L"Find your Gran Turismo 4 disc.",
              {70, 150, 890, 194}, heading_font, text, center);
    if (!page) {
        draw_text(dc,
                  iso.empty() ? L"Choose your own .iso to prepare Moon Engine for the Switch."
                              : fs::path(iso).filename().wstring(),
                  {70, 204, 890, 240}, body_font, dim, center);
        if (!iso.empty()) {
            draw_text(dc, L"Gran Turismo 4, NTSC-U  /  SCUS-97328", {80, 270, 880, 302},
                      body_font, text, center);
            draw_text(dc, L"Your disc will be verified before any installation begins.",
                      {80, 308, 880, 340}, body_font, dim, center);
        }
        draw_text(dc, L"No game data is included. Existing saves and settings are preserved.",
                  {70, 388, 890, 424}, body_font, dim, center);
    } else {
        draw_text(dc, network_mode ? L"Selected:  Network / Nxlink" : L"Selected:  USB / DBI",
                  {80, 200, 880, 232}, body_font, text, center);
        draw_text(dc,
                  network_mode
                      ? L"Enable FTP and Nxlink in Sphaira, then leave its home screen open."
                      : L"Open DBI > Start MTP Connection and connect the USB cable to this PC.",
                  {70, 242, 890, 274}, body_font, dim, center);
        if (network_mode) {
            draw_text(dc, L"SWITCH IP ADDRESS", {210, 290, 505, 316}, body_font, dim);
            draw_text(dc, L"FTP PORT", {540, 290, 750, 316}, body_font, dim);
            draw_text(
                dc,
                L"FTP copies the ISO and app. Nxlink then launches it. Same local network; anonymous FTP.",
                {55, 366, 905, 395}, body_font, dim, center);
        } else {
            draw_text(dc, L"Only the SD card is used. Keep DBI open until the transfer finishes.",
                      {70, 302, 890, 338}, body_font, dim, center);
        }
    }
    if (busy || !detail.empty()) {
        draw_text(dc, status, {60, 450, 900, 480}, body_font, text, center);
        draw_text(dc, detail == status ? L"" : detail, {70, 484, 890, 532}, body_font, dim,
                  DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
        if (busy) {
            fill(dc, {210, 438, 750, 440}, rule);
            int start = percent < 0 ? pulse % 460 : 0;
            int length = percent < 0 ? 80 : 540 * std::clamp(percent, 0, 100) / 100;
            fill(dc, {210 + start, 438, 210 + start + length, 440}, accent);
        }
    } else if (page) {
        draw_text(dc, status, {70, 466, 890, 510}, body_font, dim, center);
    }
    gradient(dc, 544, 627, RGB(62, 64, 70), RGB(32, 34, 38));
    fill(dc, {0, 544, 960, 545}, RGB(170, 171, 175));
    fill(dc, {0, 627, 960, 672}, RGB(0, 0, 0));
    fill(dc, {0, 627, 960, 628}, RGB(70, 72, 76));
    std::wstring caption = page
                               ? L"Install: Verify your disc, copy the app and prepare the SD card."
                               : L"Choose disc: Find your Gran Turismo 4 disc image (.iso).";
    int selected = hover_control ? GetDlgCtrlID(hover_control) : 0;
    if (selected == USB)
        caption = L"USB / DBI: Install over a USB cable using Start MTP Connection.";
    if (selected == NETWORK)
        caption = L"Network / Nxlink: Install over FTP and launch the app over your local network.";
    if (selected == NEXT)
        caption = L"Continue: Choose how to connect your Nintendo Switch.";
    if (selected == BACK)
        caption = L"Back: Choose another disc.";
    if (selected == EXIT)
        caption = L"Exit: Close Switch setup.";
    if (busy)
        caption = L"Keep the Switch connected. Saves and settings are preserved.";
    draw_text(dc, caption, {24, 635, 936, 666}, body_font, RGB(214, 216, 220), center);
}
static LRESULT CALLBACK button_events(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR)
{
    if (msg == WM_MOUSEMOVE) {
        hover_control = hwnd;
        TRACKMOUSEEVENT track{sizeof track, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&track);
    } else if (msg == WM_MOUSELEAVE && hover_control == hwnd)
        hover_control = nullptr;
    return DefSubclassProc(hwnd, msg, w, l);
}
static HWND button(HWND parent, const wchar_t *title, int id, int x, int y, int width, int height)
{
    HWND c = CreateWindowW(L"BUTTON", title, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, x,
                           y, width, height, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SetWindowSubclass(c, button_events, 1, 0);
    SendMessageW(c, WM_SETFONT, (WPARAM)action_font, TRUE);
    return c;
}
static void update_page()
{
    ShowWindow(browse_button, page ? SW_HIDE : SW_SHOW);
    ShowWindow(next_button, page ? SW_HIDE : SW_SHOW);
    ShowWindow(exit_button, page ? SW_HIDE : SW_SHOW);
    for (HWND c : {usb_button, net_button, back_button, install_button, reuse_button})
        ShowWindow(c, page ? SW_SHOW : SW_HIDE);
    for (HWND c : {ip_edit, port_edit})
        ShowWindow(c, page && network_mode ? SW_SHOW : SW_HIDE);
    EnableWindow(next_button, !iso.empty());
    EnableWindow(install_button, !iso.empty());
    InvalidateRect(window, nullptr, FALSE);
}
static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    switch (message) {
    case WM_CREATE:
        window = hwnd;
        browse_button = button(hwnd, L"CHOOSE DISC", BROWSE, 115, 560, 250, 58);
        next_button = button(hwnd, L"CONTINUE", NEXT, 355, 560, 250, 58);
        exit_button = button(hwnd, L"EXIT", EXIT, 620, 560, 200, 58);
        usb_button = button(hwnd, L"USB / DBI", USB, 88, 560, 208, 58);
        net_button = button(hwnd, L"NETWORK / NXLINK", NETWORK, 278, 560, 212, 58);
        back_button = button(hwnd, L"BACK", BACK, 508, 560, 136, 58);
        install_button = button(hwnd, L"INSTALL", INSTALL, 658, 560, 220, 58);
        reuse_button = button(hwnd, L"Reuse the ISO on the SD card if it already exists", REUSE,
                              160, 397, 640, 34);
        ip_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 210, 322,
                                  295, 30, hwnd, (HMENU)IP, nullptr, nullptr);
        port_edit = CreateWindowExW(0, L"EDIT", L"5000", WS_CHILD | WS_TABSTOP | ES_NUMBER, 540,
                                    322, 210, 30, hwnd, (HMENU)PORT, nullptr, nullptr);
        SendMessageW(ip_edit, EM_SETCUEBANNER, TRUE, (LPARAM)L"192.168.0.x");
        for (HWND c : {ip_edit, port_edit})
            SendMessageW(c, WM_SETFONT, (WPARAM)body_font, TRUE);
        update_page();
        SetTimer(hwnd, 1, 50, nullptr);
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == BROWSE) {
            wchar_t path[32768]{};
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof ofn;
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Disc image (*.iso)\0*.iso\0";
            ofn.lpstrFile = path;
            ofn.nMaxFile = 32768;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            ofn.lpstrTitle = L"Your Gran Turismo 4 disc";
            if (GetOpenFileNameW(&ofn)) {
                iso = path;
                status = L"Disc selected. Integrity will be verified before installation.";
                update_page();
            }
        } else if (LOWORD(w) == USB || LOWORD(w) == NETWORK) {
            network_mode = LOWORD(w) == NETWORK;
            page = 1;
            update_page();
        } else if (LOWORD(w) == NEXT || LOWORD(w) == BACK) {
            page = LOWORD(w) == NEXT ? 1 : 0;
            detail.clear();
            status = L"The SD card must support files larger than 4 GB (exFAT).";
            update_page();
            SetFocus(page ? usb_button : browse_button);
        } else if (LOWORD(w) == EXIT) {
            SendMessageW(hwnd, WM_CLOSE, 0, 0);
        } else if (LOWORD(w) == REUSE)
            reuse_iso = !reuse_iso;
        else if (LOWORD(w) == INSTALL)
            start_install();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_DRAWITEM: {
        auto *draw = (DRAWITEMSTRUCT *)l;
        bool reuse = draw->CtlID == REUSE;
        RECT r = draw->rcItem;
        if (reuse)
            fill(draw->hDC, r, RGB(252, 252, 253));
        else
            gradient(draw->hDC, r.top, r.bottom, RGB(55, 57, 63), RGB(35, 37, 42));
        bool selected =
            (draw->CtlID == USB && !network_mode) || (draw->CtlID == NETWORK && network_mode);
        float h = hover[draw->CtlID - BROWSE];
        int shade = IsWindowEnabled(draw->hwndItem) ? (int)(175 + 80 * h) : 105;
        if (selected)
            shade = 255;
        std::wstring label = control_text(draw->hwndItem);
        if (reuse)
            label = (reuse_iso ? L"[x]  " : L"[ ]  ") + label;
        OffsetRect(&r, 0, -(int)(4 * h));
        draw_text(draw->hDC, label, r, reuse ? body_font : action_font,
                  reuse ? text : RGB(shade, shade, shade), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if ((selected || h > .05f || (draw->itemState & ODS_FOCUS)) && !reuse) {
            SIZE size;
            GetTextExtentPoint32W(draw->hDC, label.c_str(), (int)label.size(), &size);
            int x = (r.left + r.right - size.cx) / 2;
            fill(draw->hDC, {x, r.bottom - 6, x + size.cx, r.bottom - 5}, RGB(shade, shade, shade));
        }
        return TRUE;
    }
    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)w, text);
        SetBkColor((HDC)w, panel);
        return (LRESULT)panel_brush;
    case WM_TIMER:
        for (HWND c : {browse_button, next_button, exit_button, usb_button, net_button, back_button,
                       install_button, reuse_button}) {
            int i = GetDlgCtrlID(c) - BROWSE;
            float target = c == hover_control || c == GetFocus() ? 1.f : 0.f;
            if (abs(hover[i] - target) > .01f) {
                hover[i] += (target - hover[i]) * .25f;
                InvalidateRect(c, nullptr, FALSE);
            }
        }
        if (busy && percent < 0)
            pulse += 16;
        {
            RECT content{0, 438, 960, 544}, caption{0, 628, 960, 672};
            InvalidateRect(hwnd, &content, FALSE);
            InvalidateRect(hwnd, &caption, FALSE);
        }
        return 0;
    case STATUS: {
        auto *value = (std::wstring *)l;
        status = *value;
        detail = *value;
        delete value;
        percent = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case PROGRESS:
        percent = (int)w;
        {
            RECT r{210, 438, 750, 440};
            InvalidateRect(hwnd, &r, FALSE);
        }
        return 0;
    case FINISHED: {
        auto *error = (std::wstring *)l;
        busy = false;
        percent = error->empty() ? 100 : 0;
        status = error->empty()
                     ? (network_mode ? L"Done. Files installed and app sent through Nxlink."
                                     : L"Done. Close MTP and open Moon Engine with full RAM.")
                     : L"Installation did not complete. See the details below.";
        detail = error->empty()
                     ? L"For full RAM: hold R while opening a game to enter Homebrew Menu/Sphaira."
                     : *error;
        delete error;
        for (HWND c : {browse_button, usb_button, net_button, reuse_button, back_button,
                       install_button, ip_edit, port_edit})
            EnableWindow(c, TRUE);
        SetWindowTextW(install_button, L"INSTALL");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_CLOSE:
        if (busy) {
            cancelled = true;
            status = L"Cancelling. Please wait for the transfer to stop.";
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_PRINTCLIENT:
        paint((HDC)w);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        HDC buffer = CreateCompatibleDC(dc);
        HBITMAP image = CreateCompatibleBitmap(dc, 960, 672),
                old = (HBITMAP)SelectObject(buffer, image);
        paint(buffer);
        BitBlt(dc, 0, 0, 960, 672, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, old);
        DeleteObject(image);
        DeleteDC(buffer);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}
static int self_test()
{
    require(quote(L"a b\\") == L"\"a b\\\\\"", "Argument quoting failed.");
    std::string config = disc_config("disc=old\nsaves=sdmc:/custom/\nvsync=1\n", "opengt4");
    require(config.find("disc=sdmc:/switch/opengt4/gt4.iso") != std::string::npos &&
                config.find("saves=sdmc:/custom/") != std::string::npos,
            "Config preservation failed.");
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    fs::path root = fs::path(temp) / (L"moon-native-test-" + std::to_wstring(GetTickCount64()));
    fs::create_directories(root / L"destination");
    write_file(root / L"source.bin", "Moon Engine Shell fixture");
    auto from = shell_item(root / L"source.bin"), to = shell_item(root / L"destination");
    copy(from.Get(), to.Get(), L"renamed.bin", true);
    require(file_hash(root / L"source.bin") == file_hash(root / L"destination/renamed.bin"),
            "Shell copy hash failed.");
    require(child(to.Get(), L"renamed.bin") != nullptr, "Shell child lookup failed.");
    // Exercise the complete DBI Shell transaction on disposable filesystem folders.
    fs::create_directories(root/L"sd/switch/moonengine");
    fs::create_directories(root/L"work");
    write_file(root/L"selected.iso","COPY");
    write_file(root/L"sd/switch/moonengine/gt4.iso","KEEP");
    fs::create_directories(root/L"switch/moonengine");
    write_file(root/L"switch/moonengine/moonengine.nro","replacement native app");
    write_file(root/L"sd/switch/moonengine/moonengine.nro","old native app");
    write_file(root/L"sd/switch/moonengine/config.ini","disc=old\nsaves=sdmc:/custom/\nvsync=1\n");
    Job job{};job.iso=root/L"selected.iso";job.temp=root/L"work";job.root=root;job.reuse=true;
    auto sd=shell_item(root/L"sd");
    install_usb_to(job,root/L"switch/moonengine/moonengine.nro",file_hash(root/L"switch/moonengine/moonengine.nro"),L"self-test",sd.Get());
    require(read_file(root/L"sd/switch/moonengine/gt4.iso")=="KEEP","ISO reuse was not preserved.");
    require(read_file(root/L"sd/moonengine-backups/before-self-test.nro.disabled")=="old native app","Old NRO backup missing.");
    require(file_hash(root/L"sd/switch/moonengine/moonengine.nro")==file_hash(root/L"switch/moonengine/moonengine.nro"),"Installed NRO mismatch.");
    std::string settings=read_file(root/L"sd/switch/moonengine/config.ini");
    require(settings.find("saves=sdmc:/custom/")!=std::string::npos &&
            settings.find("disc=sdmc:/switch/moonengine/gt4.iso")!=std::string::npos,"Installed config mismatch.");
    auto app=shell_item(root/L"sd/switch/moonengine");
    std::thread publish([&] { Sleep(200);write_file(root/L"sd/switch/moonengine/delayed.bin","ready"); });
    try { wait_file(app.Get(),L"delayed.bin",5); } catch(...) { publish.join();throw; }
    publish.join();
    bool wrong_size=false;
    try { wait_file(app.Get(),L"delayed.bin",99,3); } catch(const std::exception &) { wrong_size=true; }
    require(wrong_size && read_file(root/L"sd/switch/moonengine/delayed.bin")=="ready","Wrong-size guard modified the file.");
    // Missing package must leave an actionable report, not require remembering a dialog.
    job.temp=root/L"diagnostic";
    execute(job);
    require(read_file(root/L"MoonEngine-Switch-Setup.log").find("FAILED:")!=std::string::npos,"Diagnostic report missing.");
    return 0;
}
static void snapshot(const fs::path &path)
{
    HDC dc = GetDC(window), memory = CreateCompatibleDC(dc);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 960;
    info.bmiHeader.biHeight = 672;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *pixels;
    HBITMAP image = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0),
            old = (HBITMAP)SelectObject(memory, image);
    paint(memory);
    EnumChildWindows(
        window,
        [](HWND child, LPARAM value) -> BOOL {
            if (!(GetWindowLongW(child, GWL_STYLE) & WS_VISIBLE))
                return TRUE;
            HDC dc = (HDC)value;
            RECT bounds;
            GetWindowRect(child, &bounds);
            MapWindowPoints(nullptr, window, (POINT *)&bounds, 2);
            int saved = SaveDC(dc);
            IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
            SetViewportOrgEx(dc, bounds.left, bounds.top, nullptr);
            SendMessageW(child, WM_PRINT, (WPARAM)dc, PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
            RestoreDC(dc, saved);
            return TRUE;
        },
        (LPARAM)memory);
    BITMAPFILEHEADER file{};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof file + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + 960 * 672 * 4;
    std::ofstream output(path, std::ios::binary);
    output.write((char *)&file, sizeof file);
    output.write((char *)&info.bmiHeader, sizeof(BITMAPINFOHEADER));
    output.write((char *)pixels, 960 * 672 * 4);
    SelectObject(memory, old);
    DeleteObject(image);
    DeleteDC(memory);
    ReleaseDC(window, dc);
}
static int ftp_self_test(const std::wstring &host, int port, const fs::path &report)
{
    try {
        Job job;
        job.ip = host;
        job.port = (INTERNET_PORT)port;
        job.temp = report.parent_path();
        std::wstring stamp = std::to_wstring(GetTickCount64()),
                     folder = L"/moonengine-backups/setup-test-" + stamp;
        Ftp ftp(job);
        ftp.mkdir(L"/moonengine-backups");
        ftp.mkdir(folder);
        fs::path source = report.parent_path() / L"transfer-fixture.bin";
        write_file(source, "Moon Engine native FTP fixture");
        std::string first = file_hash(source);
        ftp.replace(source, folder + L"/file.bin", folder + L"/old.bin", stamp, first);
        require(ftp.hash(folder + L"/file.bin") == first, "Initial FTP hash failed.");
        write_file(source, "Moon Engine replacement fixture");
        std::string second = file_hash(source);
        ftp.replace(source, folder + L"/file.bin", folder + L"/old.bin", stamp + L"-2", second);
        require(ftp.hash(folder + L"/file.bin") == second &&
                    ftp.hash(folder + L"/old.bin") == first,
                "FTP replacement/backup failed.");
        bool rejected = false;
        try {
            ftp.replace(source, folder + L"/file.bin", folder + L"/should-not-exist.bin",
                        stamp + L"-negative", "incorrect hash");
        } catch (...) {
            rejected = true;
        }
        require(rejected && ftp.hash(folder + L"/file.bin") == second,
                "Rejected FTP hash changed the installed file.");
        write_file(
            report,
            "PASS: FTP create/upload/rename/read-back/hash, replacement backup, wrong-hash negative preserves original.\n");
        return 0;
    } catch (const std::exception &error) {
        write_file(report, std::string("FAIL: ") + error.what());
        return 1;
    }
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    SetProcessDPIAware();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return 1;
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if(argc==3 && std::wstring(argv[1])==L"--check-disc") {
        int code=disc_is_scus97328(argv[2]) ? 0 : 1;
        LocalFree(argv);CoUninitialize();return code;
    }
    if (argc > 1 && std::wstring(argv[1]) == L"--self-test") {
        int code = 0;
        try {
            code = self_test();
        } catch (const std::exception &e) {
            write_file(program_root()/L"self-test-error.log",e.what());
            code = 1;
        }
        LocalFree(argv);
        CoUninitialize();
        return code;
    }
    if (argc == 5 && std::wstring(argv[1]) == L"--ftp-self-test") {
        int code = ftp_self_test(argv[2], _wtoi(argv[3]), argv[4]);
        LocalFree(argv);
        CoUninitialize();
        return code;
    }
    DWORD fonts;
    AddFontMemResourceEx((void *)launcher_ttf_BARLOW_REGULAR, sizeof launcher_ttf_BARLOW_REGULAR,
                         nullptr, &fonts);
    body_font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH, L"Barlow");
    heading_font = CreateFontW(-28, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH, L"Barlow");
    title_font = CreateFontW(-40, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Barlow");
    action_font = CreateFontW(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Barlow");
    ground_brush = CreateSolidBrush(ground);
    panel_brush = CreateSolidBrush(panel);
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = procedure;
    wc.lpszClassName = L"MoonSwitchSetup";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = ground_brush;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);
    RECT bounds{0, 0, 960, 672};
    AdjustWindowRect(&bounds, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    window =
        CreateWindowW(wc.lpszClassName, L"Moon Engine | Switch",
                      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
                      CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
                      bounds.bottom - bounds.top, nullptr, nullptr, instance, nullptr);
    if (argc == 3 &&
        (std::wstring(argv[1]) == L"--preview" || std::wstring(argv[1]) == L"--preview-network")) {
        if (std::wstring(argv[1]) == L"--preview-network")
            SendMessageW(window, WM_COMMAND, NETWORK, 0);
        ShowWindow(window, SW_SHOWNOACTIVATE);
        UpdateWindow(window);
        snapshot(argv[2]);
        DestroyWindow(window);
        LocalFree(argv);
        CoUninitialize();
        return 0;
    }
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    LocalFree(argv);
    CoUninitialize();
    return 0;
}
