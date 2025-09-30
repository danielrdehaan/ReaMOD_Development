#pragma once

#include <filesystem>
#include <string>

namespace FMODDynLoad {
using GetResourcePathFunc = const char* (*)();
using ShowMessageBoxFunc = int (*)(const char*, const char*, int);

bool Initialize(GetResourcePathFunc getResourcePath,
                ShowMessageBoxFunc showMessageBox,
                unsigned int requiredVersion);
void Shutdown();
bool IsInitialized();
unsigned int GetRequiredVersion();
const std::string& GetLastError();
const std::filesystem::path& GetBasePath();
} // namespace FMODDynLoad

// Redirect FMOD API calls through dynamically loaded wrappers.
#ifndef FMOD_Studio_System_Create
#define FMOD_System_Create ReaMOD_FMOD_System_Create
#define FMOD_System_GetVersion ReaMOD_FMOD_System_GetVersion
#define FMOD_System_Release ReaMOD_FMOD_System_Release

#define FMOD_Studio_System_Create ReaMOD_FMOD_Studio_System_Create
#define FMOD_Studio_System_Initialize ReaMOD_FMOD_Studio_System_Initialize
#define FMOD_Studio_System_GetCoreSystem ReaMOD_FMOD_Studio_System_GetCoreSystem
#define FMOD_Studio_System_LoadBankFile ReaMOD_FMOD_Studio_System_LoadBankFile
#define FMOD_Studio_System_GetEvent ReaMOD_FMOD_Studio_System_GetEvent
#define FMOD_Studio_System_Update ReaMOD_FMOD_Studio_System_Update
#define FMOD_Studio_System_GetBus ReaMOD_FMOD_Studio_System_GetBus
#define FMOD_Studio_System_SetParameterByName ReaMOD_FMOD_Studio_System_SetParameterByName
#define FMOD_Studio_System_Release ReaMOD_FMOD_Studio_System_Release

#define FMOD_Studio_Bank_LoadSampleData ReaMOD_FMOD_Studio_Bank_LoadSampleData
#define FMOD_Studio_Bank_Unload ReaMOD_FMOD_Studio_Bank_Unload
#define FMOD_Studio_Bank_GetEventCount ReaMOD_FMOD_Studio_Bank_GetEventCount
#define FMOD_Studio_Bank_GetEventList ReaMOD_FMOD_Studio_Bank_GetEventList
#define FMOD_Studio_Bank_GetStringCount ReaMOD_FMOD_Studio_Bank_GetStringCount
#define FMOD_Studio_Bank_GetStringInfo ReaMOD_FMOD_Studio_Bank_GetStringInfo

#define FMOD_Studio_EventDescription_GetPath ReaMOD_FMOD_Studio_EventDescription_GetPath
#define FMOD_Studio_EventDescription_CreateInstance ReaMOD_FMOD_Studio_EventDescription_CreateInstance
#define FMOD_Studio_EventDescription_GetParameterDescriptionCount ReaMOD_FMOD_Studio_EventDescription_GetParameterDescriptionCount
#define FMOD_Studio_EventDescription_GetParameterDescriptionByIndex ReaMOD_FMOD_Studio_EventDescription_GetParameterDescriptionByIndex

#define FMOD_Studio_EventInstance_Start ReaMOD_FMOD_Studio_EventInstance_Start
#define FMOD_Studio_EventInstance_Stop ReaMOD_FMOD_Studio_EventInstance_Stop
#define FMOD_Studio_EventInstance_Release ReaMOD_FMOD_Studio_EventInstance_Release
#define FMOD_Studio_EventInstance_SetParameterByName ReaMOD_FMOD_Studio_EventInstance_SetParameterByName
#define FMOD_Studio_EventInstance_GetParameterByName ReaMOD_FMOD_Studio_EventInstance_GetParameterByName
#define FMOD_Studio_EventInstance_GetPlaybackState ReaMOD_FMOD_Studio_EventInstance_GetPlaybackState

#define FMOD_Studio_Bus_StopAllEvents ReaMOD_FMOD_Studio_Bus_StopAllEvents
#endif

#if __has_include("fmod_studio.hpp")
#define FMODDYNLOAD_HAS_SDK 1
#include "fmod_studio.hpp"
#include "fmod.hpp"
#include "fmod_errors.h"
#else
#include "FMODMinimal.hpp"
#endif

// Wrapper function declarations exposed by the dynamic loader.
extern "C" {
FMOD_RESULT F_CALL ReaMOD_FMOD_System_Create(FMOD_SYSTEM** system);
FMOD_RESULT F_CALL ReaMOD_FMOD_System_GetVersion(FMOD_SYSTEM* system, unsigned int* version);
FMOD_RESULT F_CALL ReaMOD_FMOD_System_Release(FMOD_SYSTEM* system);

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_Create(FMOD_STUDIO_SYSTEM** system, unsigned int headerVersion);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_Initialize(FMOD_STUDIO_SYSTEM* system, int maxchannels,
                                                        FMOD_STUDIO_INITFLAGS studioflags,
                                                        FMOD_INITFLAGS flags, void* extradriverdata);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_GetCoreSystem(FMOD_STUDIO_SYSTEM* system, FMOD_SYSTEM** coresystem);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_LoadBankFile(FMOD_STUDIO_SYSTEM* system, const char* filename,
                                                          FMOD_STUDIO_LOAD_BANK_FLAGS flags,
                                                          FMOD_STUDIO_BANK** bank);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_GetEvent(FMOD_STUDIO_SYSTEM* system, const char* pathOrID,
                                                      FMOD_STUDIO_EVENTDESCRIPTION** event);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_Update(FMOD_STUDIO_SYSTEM* system);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_GetBus(FMOD_STUDIO_SYSTEM* system, const char* pathOrID,
                                                    FMOD_STUDIO_BUS** bus);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_SetParameterByName(FMOD_STUDIO_SYSTEM* system, const char* name,
                                                                float value, FMOD_BOOL ignoreseekspeed);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_System_Release(FMOD_STUDIO_SYSTEM* system);

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_LoadSampleData(FMOD_STUDIO_BANK* bank);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_Unload(FMOD_STUDIO_BANK* bank);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_GetEventCount(FMOD_STUDIO_BANK* bank, int* count);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_GetEventList(FMOD_STUDIO_BANK* bank,
                                                        FMOD_STUDIO_EVENTDESCRIPTION** array,
                                                        int capacity, int* count);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_GetStringCount(FMOD_STUDIO_BANK* bank, int* count);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bank_GetStringInfo(FMOD_STUDIO_BANK* bank, int index,
                                                         FMOD_GUID* id, char* path, int size, int* retrieved);

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventDescription_GetPath(FMOD_STUDIO_EVENTDESCRIPTION* eventdescription,
                                                               char* path, int size, int* retrieved);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventDescription_CreateInstance(FMOD_STUDIO_EVENTDESCRIPTION* eventdescription,
                                                                      FMOD_STUDIO_EVENTINSTANCE** instance);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventDescription_GetParameterDescriptionCount(
    FMOD_STUDIO_EVENTDESCRIPTION* eventdescription, int* count);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventDescription_GetParameterDescriptionByIndex(
    FMOD_STUDIO_EVENTDESCRIPTION* eventdescription, int index,
    FMOD_STUDIO_PARAMETER_DESCRIPTION* description);

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_Start(FMOD_STUDIO_EVENTINSTANCE* eventinstance);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_Stop(FMOD_STUDIO_EVENTINSTANCE* eventinstance,
                                                         FMOD_STUDIO_STOP_MODE mode);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_Release(FMOD_STUDIO_EVENTINSTANCE* eventinstance);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_SetParameterByName(FMOD_STUDIO_EVENTINSTANCE* eventinstance,
                                                                       const char* name, float value,
                                                                       FMOD_BOOL ignoreseekspeed);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_GetParameterByName(FMOD_STUDIO_EVENTINSTANCE* eventinstance,
                                                                       const char* name, float* value,
                                                                       float* finalvalue);
FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_EventInstance_GetPlaybackState(FMOD_STUDIO_EVENTINSTANCE* eventinstance,
                                                                     FMOD_STUDIO_PLAYBACK_STATE* state);

FMOD_RESULT F_CALL ReaMOD_FMOD_Studio_Bus_StopAllEvents(FMOD_STUDIO_BUS* bus, FMOD_STUDIO_STOP_MODE mode);
}
