#include "FMODDynLoad.h"

#include <sstream>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
#if defined(_WIN32)
using LibraryHandle = HMODULE;
inline LibraryHandle OpenLibrary(const std::filesystem::path& path) {
    return ::LoadLibraryW(path.wstring().c_str());
}
inline void CloseLibrary(LibraryHandle handle) {
    if (handle) {
        ::FreeLibrary(handle);
    }
}
inline void* LoadSymbol(LibraryHandle handle, const char* name) {
    return handle ? reinterpret_cast<void*>(::GetProcAddress(handle, name)) : nullptr;
}
#else
using LibraryHandle = void*;
inline LibraryHandle OpenLibrary(const std::filesystem::path& path) {
    return ::dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
}
inline void CloseLibrary(LibraryHandle handle) {
    if (handle) {
        ::dlclose(handle);
    }
}
inline void* LoadSymbol(LibraryHandle handle, const char* name) {
    return handle ? ::dlsym(handle, name) : nullptr;
}
#endif

LibraryHandle g_coreLibrary = nullptr;
LibraryHandle g_studioLibrary = nullptr;
FMODDynLoad::GetResourcePathFunc g_getResourcePath = nullptr;
[[maybe_unused]] FMODDynLoad::ShowMessageBoxFunc g_showMessageBox = nullptr;
unsigned int g_requiredVersion = 0;
std::filesystem::path g_basePath;
bool g_initialized = false;
std::string g_lastError;

using FMOD_System_Create_Fn = FMOD_RESULT(F_CALL*)(FMOD_SYSTEM**);
using FMOD_System_GetVersion_Fn = FMOD_RESULT(F_CALL*)(FMOD_SYSTEM*, unsigned int*);
using FMOD_System_Release_Fn = FMOD_RESULT(F_CALL*)(FMOD_SYSTEM*);
using FMOD_Studio_System_Create_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_SYSTEM**, unsigned int);
using FMOD_Studio_System_Initialize_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_SYSTEM*, int, FMOD_STUDIO_INITFLAGS, FMOD_INITFLAGS, void*);
using FMOD_Studio_System_GetCoreSystem_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_SYSTEM*, FMOD_SYSTEM**);
using FMOD_Studio_System_LoadBankFile_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_SYSTEM*, const char*, FMOD_STUDIO_LOAD_BANK_FLAGS, FMOD_STUDIO_BANK**);
using FMOD_Studio_System_GetEvent_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_SYSTEM*, const char*, FMOD_STUDIO_EVENTDESCRIPTION**);
using FMOD_Studio_System_Update_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_SYSTEM*);
using FMOD_Studio_System_GetBus_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_SYSTEM*, const char*, FMOD_STUDIO_BUS**);
using FMOD_Studio_System_SetParameterByName_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_SYSTEM*, const char*, float, FMOD_BOOL);
using FMOD_Studio_System_Release_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_SYSTEM*);
using FMOD_Studio_Bank_LoadSampleData_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_BANK*);
using FMOD_Studio_Bank_Unload_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_BANK*);
using FMOD_Studio_Bank_GetEventCount_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_BANK*, int*);
using FMOD_Studio_Bank_GetEventList_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_BANK*, FMOD_STUDIO_EVENTDESCRIPTION**, int, int*);
using FMOD_Studio_Bank_GetStringInfo_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_BANK*, int, FMOD_GUID*, char*, int, int*);
using FMOD_Studio_EventDescription_GetPath_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_EVENTDESCRIPTION*, char*, int, int*);
using FMOD_Studio_EventDescription_CreateInstance_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_EVENTDESCRIPTION*, FMOD_STUDIO_EVENTINSTANCE**);
using FMOD_Studio_EventDescription_GetParameterDescriptionCount_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_EVENTDESCRIPTION*, int*);
using FMOD_Studio_EventDescription_GetParameterDescriptionByIndex_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_EVENTDESCRIPTION*, int, FMOD_STUDIO_PARAMETER_DESCRIPTION*);
using FMOD_Studio_EventInstance_Start_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_EVENTINSTANCE*);
using FMOD_Studio_EventInstance_Stop_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_EVENTINSTANCE*, FMOD_STUDIO_STOP_MODE);
using FMOD_Studio_EventInstance_Release_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_EVENTINSTANCE*);
using FMOD_Studio_EventInstance_SetParameterByName_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_EVENTINSTANCE*, const char*, float, FMOD_BOOL);
using FMOD_Studio_EventInstance_GetParameterByName_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_EVENTINSTANCE*, const char*, float*, float*);
using FMOD_Studio_EventInstance_GetPlaybackState_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_EVENTINSTANCE*, FMOD_STUDIO_PLAYBACK_STATE*);
using FMOD_Studio_Bus_StopAllEvents_Fn = FMOD_RESULT(F_CALL*)(FMOD_STUDIO_BUS*, FMOD_STUDIO_STOP_MODE);

FMOD_System_Create_Fn pFMOD_System_Create = nullptr;
FMOD_System_GetVersion_Fn pFMOD_System_GetVersion = nullptr;
FMOD_System_Release_Fn pFMOD_System_Release = nullptr;
FMOD_Studio_System_Create_Fn pFMOD_Studio_System_Create = nullptr;
FMOD_Studio_System_Initialize_Fn pFMOD_Studio_System_Initialize = nullptr;
FMOD_Studio_System_GetCoreSystem_Fn pFMOD_Studio_System_GetCoreSystem = nullptr;
FMOD_Studio_System_LoadBankFile_Fn pFMOD_Studio_System_LoadBankFile = nullptr;
FMOD_Studio_System_GetEvent_Fn pFMOD_Studio_System_GetEvent = nullptr;
FMOD_Studio_System_Update_Fn pFMOD_Studio_System_Update = nullptr;
FMOD_Studio_System_GetBus_Fn pFMOD_Studio_System_GetBus = nullptr;
FMOD_Studio_System_SetParameterByName_Fn pFMOD_Studio_System_SetParameterByName = nullptr;
FMOD_Studio_System_Release_Fn pFMOD_Studio_System_Release = nullptr;
FMOD_Studio_Bank_LoadSampleData_Fn pFMOD_Studio_Bank_LoadSampleData = nullptr;
FMOD_Studio_Bank_Unload_Fn pFMOD_Studio_Bank_Unload = nullptr;
FMOD_Studio_Bank_GetEventCount_Fn pFMOD_Studio_Bank_GetEventCount = nullptr;
FMOD_Studio_Bank_GetEventList_Fn pFMOD_Studio_Bank_GetEventList = nullptr;
FMOD_Studio_Bank_GetStringInfo_Fn pFMOD_Studio_Bank_GetStringInfo = nullptr;
FMOD_Studio_EventDescription_GetPath_Fn pFMOD_Studio_EventDescription_GetPath = nullptr;
FMOD_Studio_EventDescription_CreateInstance_Fn pFMOD_Studio_EventDescription_CreateInstance = nullptr;
FMOD_Studio_EventDescription_GetParameterDescriptionCount_Fn pFMOD_Studio_EventDescription_GetParameterDescriptionCount = nullptr;
FMOD_Studio_EventDescription_GetParameterDescriptionByIndex_Fn pFMOD_Studio_EventDescription_GetParameterDescriptionByIndex = nullptr;
FMOD_Studio_EventInstance_Start_Fn pFMOD_Studio_EventInstance_Start = nullptr;
FMOD_Studio_EventInstance_Stop_Fn pFMOD_Studio_EventInstance_Stop = nullptr;
FMOD_Studio_EventInstance_Release_Fn pFMOD_Studio_EventInstance_Release = nullptr;
FMOD_Studio_EventInstance_SetParameterByName_Fn pFMOD_Studio_EventInstance_SetParameterByName = nullptr;
FMOD_Studio_EventInstance_GetParameterByName_Fn pFMOD_Studio_EventInstance_GetParameterByName = nullptr;
FMOD_Studio_EventInstance_GetPlaybackState_Fn pFMOD_Studio_EventInstance_GetPlaybackState = nullptr;
FMOD_Studio_Bus_StopAllEvents_Fn pFMOD_Studio_Bus_StopAllEvents = nullptr;

void ResetState() {
    if (g_coreLibrary) {
        CloseLibrary(g_coreLibrary);
        g_coreLibrary = nullptr;
    }
    if (g_studioLibrary) {
        CloseLibrary(g_studioLibrary);
        g_studioLibrary = nullptr;
    }
    g_initialized = false;
    g_basePath.clear();
    pFMOD_System_Create = nullptr;
    pFMOD_System_GetVersion = nullptr;
    pFMOD_System_Release = nullptr;
    pFMOD_Studio_System_Create = nullptr;
    pFMOD_Studio_System_Initialize = nullptr;
    pFMOD_Studio_System_GetCoreSystem = nullptr;
    pFMOD_Studio_System_LoadBankFile = nullptr;
    pFMOD_Studio_System_GetEvent = nullptr;
    pFMOD_Studio_System_Update = nullptr;
    pFMOD_Studio_System_GetBus = nullptr;
    pFMOD_Studio_System_SetParameterByName = nullptr;
    pFMOD_Studio_System_Release = nullptr;
    pFMOD_Studio_Bank_LoadSampleData = nullptr;
    pFMOD_Studio_Bank_Unload = nullptr;
    pFMOD_Studio_Bank_GetEventCount = nullptr;
    pFMOD_Studio_Bank_GetEventList = nullptr;
    pFMOD_Studio_Bank_GetStringInfo = nullptr;
    pFMOD_Studio_EventDescription_GetPath = nullptr;
    pFMOD_Studio_EventDescription_CreateInstance = nullptr;
    pFMOD_Studio_EventDescription_GetParameterDescriptionCount = nullptr;
    pFMOD_Studio_EventDescription_GetParameterDescriptionByIndex = nullptr;
    pFMOD_Studio_EventInstance_Start = nullptr;
    pFMOD_Studio_EventInstance_Stop = nullptr;
    pFMOD_Studio_EventInstance_Release = nullptr;
    pFMOD_Studio_EventInstance_SetParameterByName = nullptr;
    pFMOD_Studio_EventInstance_GetParameterByName = nullptr;
    pFMOD_Studio_EventInstance_GetPlaybackState = nullptr;
    pFMOD_Studio_Bus_StopAllEvents = nullptr;
}

template <typename Fn>
bool LoadFunction(LibraryHandle library, const char* symbolName, Fn& fn, const char* libraryLabel) {
    fn = reinterpret_cast<Fn>(LoadSymbol(library, symbolName));
    if (!fn) {
        std::ostringstream oss;
        oss << "Failed to resolve " << symbolName;
        if (libraryLabel) {
            oss << " from " << libraryLabel;
        }
        g_lastError = oss.str();
        return false;
    }
    return true;
}

} // namespace

namespace FMODDynLoad {

bool Initialize(GetResourcePathFunc getResourcePath,
                ShowMessageBoxFunc showMessageBox,
                unsigned int requiredVersion) {
    g_lastError.clear();
    g_getResourcePath = getResourcePath;
    g_showMessageBox = showMessageBox;
    g_requiredVersion = requiredVersion;

    if (g_initialized) {
        return true;
    }

    if (!g_getResourcePath) {
        g_lastError = "GetResourcePath function pointer is null.";
        return false;
    }

    const char* resourcePathCStr = g_getResourcePath();
    if (!resourcePathCStr || std::string(resourcePathCStr).empty()) {
        g_lastError = "GetResourcePath returned an empty path.";
        return false;
    }

    std::error_code ec;
    g_basePath = std::filesystem::path(resourcePathCStr) / "ReaMOD";
    std::filesystem::create_directories(g_basePath, ec);
    if (ec) {
        g_lastError = std::string("Failed to create ReaMOD directory: ") + ec.message();
        return false;
    }

    const std::filesystem::path apiBase = g_basePath / "fmod" / "api";
#if defined(_WIN32)
    const std::filesystem::path coreLibPath = apiBase / "core" / "lib" / "x64" / "fmod.dll";
    const std::filesystem::path studioLibPath = apiBase / "studio" / "lib" / "x64" / "fmodstudio.dll";
#else
    const std::filesystem::path coreLibPath = apiBase / "core" / "lib" / "libfmod.dylib";
    const std::filesystem::path studioLibPath = apiBase / "studio" / "lib" / "libfmodstudio.dylib";
#endif

    if (!std::filesystem::exists(coreLibPath)) {
        g_lastError = std::string("Missing FMOD core library at ") + coreLibPath.string();
        return false;
    }
    if (!std::filesystem::exists(studioLibPath)) {
        g_lastError = std::string("Missing FMOD studio library at ") + studioLibPath.string();
        return false;
    }

    g_coreLibrary = OpenLibrary(coreLibPath);
    if (!g_coreLibrary) {
        g_lastError = std::string("Failed to load FMOD core library: ") + coreLibPath.string();
        return false;
    }

    g_studioLibrary = OpenLibrary(studioLibPath);
    if (!g_studioLibrary) {
        g_lastError = std::string("Failed to load FMOD studio library: ") + studioLibPath.string();
        ResetState();
        return false;
    }

    if (!LoadFunction(g_coreLibrary, "FMOD_System_Create", pFMOD_System_Create, "FMOD core")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_coreLibrary, "FMOD_System_GetVersion", pFMOD_System_GetVersion, "FMOD core")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_coreLibrary, "FMOD_System_Release", pFMOD_System_Release, "FMOD core")) {
        ResetState();
        return false;
    }

    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_System_Create", pFMOD_Studio_System_Create, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_System_Initialize", pFMOD_Studio_System_Initialize, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_System_GetCoreSystem", pFMOD_Studio_System_GetCoreSystem, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_System_LoadBankFile", pFMOD_Studio_System_LoadBankFile, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_System_GetEvent", pFMOD_Studio_System_GetEvent, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_System_Update", pFMOD_Studio_System_Update, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_System_GetBus", pFMOD_Studio_System_GetBus, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_System_SetParameterByName", pFMOD_Studio_System_SetParameterByName, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_System_Release", pFMOD_Studio_System_Release, "FMOD studio")) {
        ResetState();
        return false;
    }

    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_Bank_LoadSampleData", pFMOD_Studio_Bank_LoadSampleData, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_Bank_Unload", pFMOD_Studio_Bank_Unload, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_Bank_GetEventCount", pFMOD_Studio_Bank_GetEventCount, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_Bank_GetEventList", pFMOD_Studio_Bank_GetEventList, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_Bank_GetStringInfo", pFMOD_Studio_Bank_GetStringInfo, "FMOD studio")) {
        ResetState();
        return false;
    }

    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_EventDescription_GetPath", pFMOD_Studio_EventDescription_GetPath, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_EventDescription_CreateInstance", pFMOD_Studio_EventDescription_CreateInstance, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_EventDescription_GetParameterDescriptionCount", pFMOD_Studio_EventDescription_GetParameterDescriptionCount, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_EventDescription_GetParameterDescriptionByIndex", pFMOD_Studio_EventDescription_GetParameterDescriptionByIndex, "FMOD studio")) {
        ResetState();
        return false;
    }

    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_EventInstance_Start", pFMOD_Studio_EventInstance_Start, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_EventInstance_Stop", pFMOD_Studio_EventInstance_Stop, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_EventInstance_Release", pFMOD_Studio_EventInstance_Release, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_EventInstance_SetParameterByName", pFMOD_Studio_EventInstance_SetParameterByName, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_EventInstance_GetParameterByName", pFMOD_Studio_EventInstance_GetParameterByName, "FMOD studio")) {
        ResetState();
        return false;
    }
    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_EventInstance_GetPlaybackState", pFMOD_Studio_EventInstance_GetPlaybackState, "FMOD studio")) {
        ResetState();
        return false;
    }

    if (!LoadFunction(g_studioLibrary, "FMOD_Studio_Bus_StopAllEvents", pFMOD_Studio_Bus_StopAllEvents, "FMOD studio")) {
        ResetState();
        return false;
    }

    g_initialized = true;
    return true;
}

void Shutdown() {
    ResetState();
    g_lastError.clear();
}

bool IsInitialized() {
    return g_initialized;
}

unsigned int GetRequiredVersion() {
    return g_requiredVersion;
}

const std::string& GetLastError() {
    return g_lastError;
}

const std::filesystem::path& GetBasePath() {
    return g_basePath;
}

} // namespace FMODDynLoad

extern "C" {

FMOD_RESULT F_CALL ReaMOD_FMOD_System_Create(FMOD_SYSTEM** system) {
    if (!pFMOD_System_Create) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_System_Create(system);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_System_GetVersion(FMOD_SYSTEM* system, unsigned int* version) {
    if (!pFMOD_System_GetVersion) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_System_GetVersion(system, version);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_System_Release(FMOD_SYSTEM* system) {
    if (!pFMOD_System_Release) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_System_Release(system);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_Create(FMOD_STUDIO_SYSTEM** system, unsigned int headerVersion) {
    if (!pFMOD_Studio_System_Create) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_System_Create(system, headerVersion);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_Initialize(FMOD_STUDIO_SYSTEM* system, int maxchannels,
                                                        FMOD_STUDIO_INITFLAGS studioflags,
                                                        FMOD_INITFLAGS flags, void* extradriverdata) {
    if (!pFMOD_Studio_System_Initialize) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_System_Initialize(system, maxchannels, studioflags, flags, extradriverdata);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_GetCoreSystem(FMOD_STUDIO_SYSTEM* system, FMOD_SYSTEM** coresystem) {
    if (!pFMOD_Studio_System_GetCoreSystem) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_System_GetCoreSystem(system, coresystem);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_LoadBankFile(FMOD_STUDIO_SYSTEM* system, const char* filename,
                                                          FMOD_STUDIO_LOAD_BANK_FLAGS flags,
                                                          FMOD_STUDIO_BANK** bank) {
    if (!pFMOD_Studio_System_LoadBankFile) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_System_LoadBankFile(system, filename, flags, bank);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_GetEvent(FMOD_STUDIO_SYSTEM* system, const char* pathOrID,
                                                      FMOD_STUDIO_EVENTDESCRIPTION** event) {
    if (!pFMOD_Studio_System_GetEvent) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_System_GetEvent(system, pathOrID, event);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_Update(FMOD_STUDIO_SYSTEM* system) {
    if (!pFMOD_Studio_System_Update) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_System_Update(system);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_GetBus(FMOD_STUDIO_SYSTEM* system, const char* pathOrID,
                                                    FMOD_STUDIO_BUS** bus) {
    if (!pFMOD_Studio_System_GetBus) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_System_GetBus(system, pathOrID, bus);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_SetParameterByName(FMOD_STUDIO_SYSTEM* system, const char* name,
                                                                float value, FMOD_BOOL ignoreseekspeed) {
    if (!pFMOD_Studio_System_SetParameterByName) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_System_SetParameterByName(system, name, value, ignoreseekspeed);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_Release(FMOD_STUDIO_SYSTEM* system) {
    if (!pFMOD_Studio_System_Release) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_System_Release(system);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_LoadSampleData(FMOD_STUDIO_BANK* bank) {
    if (!pFMOD_Studio_Bank_LoadSampleData) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_Bank_LoadSampleData(bank);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_Unload(FMOD_STUDIO_BANK* bank) {
    if (!pFMOD_Studio_Bank_Unload) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_Bank_Unload(bank);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_GetEventCount(FMOD_STUDIO_BANK* bank, int* count) {
    if (!pFMOD_Studio_Bank_GetEventCount) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_Bank_GetEventCount(bank, count);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_GetEventList(FMOD_STUDIO_BANK* bank,
                                                        FMOD_STUDIO_EVENTDESCRIPTION** array,
                                                        int capacity, int* count) {
    if (!pFMOD_Studio_Bank_GetEventList) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_Bank_GetEventList(bank, array, capacity, count);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_GetStringInfo(FMOD_STUDIO_BANK* bank, int index,
                                                         FMOD_GUID* id, char* path, int size, int* retrieved) {
    if (!pFMOD_Studio_Bank_GetStringInfo) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_Bank_GetStringInfo(bank, index, id, path, size, retrieved);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventDescription_GetPath(FMOD_STUDIO_EVENTDESCRIPTION* eventdescription,
                                                               char* path, int size, int* retrieved) {
    if (!pFMOD_Studio_EventDescription_GetPath) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_EventDescription_GetPath(eventdescription, path, size, retrieved);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventDescription_CreateInstance(FMOD_STUDIO_EVENTDESCRIPTION* eventdescription,
                                                                      FMOD_STUDIO_EVENTINSTANCE** instance) {
    if (!pFMOD_Studio_EventDescription_CreateInstance) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_EventDescription_CreateInstance(eventdescription, instance);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventDescription_GetParameterDescriptionCount(
    FMOD_STUDIO_EVENTDESCRIPTION* eventdescription, int* count) {
    if (!pFMOD_Studio_EventDescription_GetParameterDescriptionCount) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_EventDescription_GetParameterDescriptionCount(eventdescription, count);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventDescription_GetParameterDescriptionByIndex(
    FMOD_STUDIO_EVENTDESCRIPTION* eventdescription, int index,
    FMOD_STUDIO_PARAMETER_DESCRIPTION* description) {
    if (!pFMOD_Studio_EventDescription_GetParameterDescriptionByIndex) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_EventDescription_GetParameterDescriptionByIndex(eventdescription, index, description);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_Start(FMOD_STUDIO_EVENTINSTANCE* eventinstance) {
    if (!pFMOD_Studio_EventInstance_Start) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_EventInstance_Start(eventinstance);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_Stop(FMOD_STUDIO_EVENTINSTANCE* eventinstance,
                                                         FMOD_STUDIO_STOP_MODE mode) {
    if (!pFMOD_Studio_EventInstance_Stop) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_EventInstance_Stop(eventinstance, mode);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_Release(FMOD_STUDIO_EVENTINSTANCE* eventinstance) {
    if (!pFMOD_Studio_EventInstance_Release) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_EventInstance_Release(eventinstance);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_SetParameterByName(FMOD_STUDIO_EVENTINSTANCE* eventinstance,
                                                                       const char* name, float value,
                                                                       FMOD_BOOL ignoreseekspeed) {
    if (!pFMOD_Studio_EventInstance_SetParameterByName) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_EventInstance_SetParameterByName(eventinstance, name, value, ignoreseekspeed);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_GetParameterByName(FMOD_STUDIO_EVENTINSTANCE* eventinstance,
                                                                       const char* name, float* value,
                                                                       float* finalvalue) {
    if (!pFMOD_Studio_EventInstance_GetParameterByName) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_EventInstance_GetParameterByName(eventinstance, name, value, finalvalue);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_GetPlaybackState(FMOD_STUDIO_EVENTINSTANCE* eventinstance,
                                                                     FMOD_STUDIO_PLAYBACK_STATE* state) {
    if (!pFMOD_Studio_EventInstance_GetPlaybackState) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_EventInstance_GetPlaybackState(eventinstance, state);
}

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bus_StopAllEvents(FMOD_STUDIO_BUS* bus, FMOD_STUDIO_STOP_MODE mode) {
    if (!pFMOD_Studio_Bus_StopAllEvents) {
        return FMOD_ERR_UNINITIALIZED;
    }
    return pFMOD_Studio_Bus_StopAllEvents(bus, mode);
}

} // extern "C"
