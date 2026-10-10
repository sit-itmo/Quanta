/* SPDX-License-Identifier: MIT */
#include "os.h"
#include "windows_internal.h"

static int registry_text(const wchar_t *value, char *out, size_t capacity)
{
    wchar_t text[QUANTA_TEXT_CAP];
    DWORD bytes = sizeof(text);
    LSTATUS status =
        RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", value,
                     RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, NULL, text, &bytes);
    return status == ERROR_SUCCESS && quanta_win_utf8(out, capacity, text);
}

quanta_status quanta_hal_os_refresh(quanta_os_snapshot *out, quanta_hal_context context)
{
    const quanta_hal_context *hal = &context;
    wchar_t hostname[256];
    DWORD length = QUANTA_ARRAY_SIZE(hostname);
    SYSTEM_INFO info;
    OSVERSIONINFOW version;
    typedef LONG(WINAPI * get_version_fn)(OSVERSIONINFOW *);
    get_version_fn get_version = NULL;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    FARPROC address = ntdll ? GetProcAddress(ntdll, "RtlGetVersion") : NULL;
    memset(out, 0, sizeof(*out));
    memset(&version, 0, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);
    if (sizeof(get_version) == sizeof(address))
    {
        memcpy(&get_version, &address, sizeof(get_version));
    }
    if (!GetComputerNameExW(ComputerNameDnsHostname, hostname, &length))
    {
        DWORD error = GetLastError();
        quanta_win_error(hal->error, hal->error_capacity, error, "GetComputerNameExW");
        out->hostname_status = quanta_win_status(error);
    }
    else if (!quanta_win_utf8(out->hostname, sizeof(out->hostname), hostname))
    {
        out->hostname_status = QUANTA_LIMIT;
    }
    quanta_copy(out->name, sizeof(out->name), "Microsoft Windows");
    (void)registry_text(L"ProductName", out->name, sizeof(out->name));
    if (get_version && get_version(&version) == 0)
    {
        snprintf(out->kernel, sizeof(out->kernel), "%lu.%lu.%lu",
                 (unsigned long)version.dwMajorVersion, (unsigned long)version.dwMinorVersion,
                 (unsigned long)version.dwBuildNumber);
        snprintf(out->version, sizeof(out->version), "NT build %lu",
                 (unsigned long)version.dwBuildNumber);
        (void)registry_text(L"BuildLabEx", out->version, sizeof(out->version));
    }
    else
    {
        out->kernel_status = QUANTA_UNSUPPORTED;
    }
    GetNativeSystemInfo(&info);
    switch (info.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64:
            quanta_copy(out->arch, sizeof(out->arch), "x86_64");
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            quanta_copy(out->arch, sizeof(out->arch), "x86");
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            quanta_copy(out->arch, sizeof(out->arch), "arm");
            break;
#ifdef PROCESSOR_ARCHITECTURE_ARM64
        case PROCESSOR_ARCHITECTURE_ARM64:
            quanta_copy(out->arch, sizeof(out->arch), "arm64");
            break;
#endif
        default:
            quanta_copy(out->arch, sizeof(out->arch), "unknown");
            break;
    }
    out->online_cpus = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    out->online_status = out->online_cpus ? QUANTA_OK : quanta_win_status(GetLastError());
    if (!out->online_cpus && out->online_status == QUANTA_OK)
    {
        out->online_status = QUANTA_NO_DATA;
    }
    out->uptime = (double)GetTickCount64() / 1000.0;
    out->load_status = QUANTA_UNSUPPORTED;
    return QUANTA_OK;
}

void quanta_hal_os_cleanup(quanta_os_snapshot *data)
{
    (void)data;
}
