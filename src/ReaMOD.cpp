#include <cstdarg>
#include <string>
#include <memory>
#include <cstring>
#include <vector>
#include <exception>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <map>        // Use for storing hierarchical paths
#include <set>        // Use for sorted folder paths
#include <filesystem> // C++17 file system operations
#include <functional>
#include <chrono>
#include <thread>
#include <fstream> // Include for file I/O operations
#include <ctime>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif
#include "fmod_studio.hpp"
#include "fmod.hpp"
#include "fmod_errors.h"
#include "reaper_plugin.h"
#include "tinyfiledialogs.h"


#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"

#define REAIMGUIAPI_IMPLEMENT
#include "reaper_imgui_functions.h"

#define FILE_PATH_BUFFER_SIZE 1024

#define DEBUG true

namespace fs = std::filesystem;  // Alias for easier use of filesystem operations

// Declare the global variable to store the custom action ID
static int actionIdOpenCloseReaMODWindow = 0;
// static int actionIdAddMarkerWithSelectedEvent = 0;
static int actionIdAddItemWithSelectedEventAtEditCursor = 0;
static int actionIdAddItemWithSelectedEventAtEditCursorMatchLength = 0;
static int actionIdAddItemWithSelectedEventWithinTimeSelection = 0;
static int actionIDUpdateNumFramesForItemInsertionFromCurrentTimeSelection = 0;
static int actionIDStopAndReleaseAllFmodEventInstances = 0;
static int actionIDInsertParamUpdateItemForSelectedMediaItem = 0;
static int actionIDInsertParamAutomationItemsForSelectedMediaItem = 0;
static int actionIDInsertPositionInterpolationItemsForSelectedMediaItem = 0;
static int actionIDInsertParamEnvelopesForSelectedEventOnSelectedItem = 0;
// static int actionIDPostFmodTracksListToConsole = 0;
static int actionIDSearchForFmodEvent = 0;
static int actionIDTriggerSelectedEvent = 0;
static int actionIDToggleDebugOnOff = 0;

bool debugMessages = false;
bool showFullBankDirectoryPaths = false;


// ImGui context
ImGui_Context* reaMOD_Main_ImGui_Context = nullptr;
ImGui_Context* reaMOD_EventSearch_ImGui_Context = nullptr;
ImGui_Font* reaMODRegularFont = nullptr;
ImGui_Font* reaMODMediumFont = nullptr;
ImGui_Font* reaMODBoldFont = nullptr;
char selected_file_path[FILE_PATH_BUFFER_SIZE] = "";  // Full path of selected .fspro file
char selected_file_name[FILE_PATH_BUFFER_SIZE] = "No project selected.";  // Initial text in the input box
std::string currentReaMODFileName = " ";
std::string currentDisplayedFileName = " ";
std::string fmodProjectDirectory = "";
bool reaModWindowOpen = true;
bool reaModWindowPreviouslyOpen = false;
bool searchFmodEventWindowOpen = false;


std::vector<std::string> masterStringEvents;  // Store event paths from Master.strings.bank
std::vector<std::string> bank_files; // Store the list of found .bank files and their toggle states
std::vector<bool> bank_load_states;
std::vector<std::string> customBankDirectories; // User-defined directories to search for bank files
std::unordered_map<std::string, FMOD::Studio::Bank*> loaded_banks;  // Map of loaded banks
std::unordered_map<std::string, std::vector<std::string>> bank_events;  // Map of events in each bank
// std::unordered_map<int, bool> triggeredMarkers;  // Stores whether a marker has already triggered
std::unordered_map<MediaItem*, bool> triggeredItems; // Global variable to store whether an item has already triggered
std::unordered_map<std::string, bool> buttonStates; // Global state map to store the color toggle state for each button

std::string selectedFMODEvent = "";  // Global or static variable to store the selected event

// Global variables to track playback
double previousPlayPosition = 0.0;
double lastCallTime = 0.0;
int previousPlayState = 0;

// Look-ahead time for event triggering
int lookAheadTimeMs = 60;  // Default look-ahead time set to 0 milliseconds

// Task management
std::unordered_map<int, std::function<void()>> taskMap;
int nextTaskId = 0;
int guiTaskId = -1;
int searchEventGuiTaskId = -1;
int playbackTaskId = -1;
int itemSelectionTaskId = -1;


// Global variable to store the last triggered FMOD event path
std::string lastTriggeredFMODEvent;

std::string lastSaveTimestamp; // Global variable to store the last modified timestamp
std::string formattedLastSaveTimestamp; // Holds the formatted "Last Save" text

int numFramesForItem = 10; // Default number of frames for the inserted item
bool moveCursorAfterInsert = true; // Default to true, meaning the cursor moves forward by default
bool updateItemInsertionLength = true; 
bool syncSelectedEventWithItemSelection = true;
MediaItem* lastSelectedItem = nullptr;

// FMOD system pointers
FMOD::Studio::System* fmod_system = nullptr;

void RefreshBankFiles();
void SynchronizeLoadedBanks();

// Define the ParameterInfo struct
struct ParameterInfo {
    std::string name;
    FMOD_STUDIO_PARAMETER_ID id;
    float minValue;
    float maxValue;
    float defaultValue;
    float currentValue;
};

struct GlobalParameter {
    std::string name;
    float currentValue;
    float minValue;
    float maxValue;
};

// Declare a global vector to store parameters of the selected event
std::vector<ParameterInfo> selectedEventParameters;
std::vector<GlobalParameter> globalParameters;
// Map to store cached parameters for each event
std::unordered_map<std::string, std::vector<ParameterInfo>> eventParameterCache;


// Define the EventInstanceData struct
struct EventInstanceData {
    FMOD::Studio::EventInstance* instance;
    double startPosition;
    double endPosition;
};

std::unordered_map<std::string, EventInstanceData> activeEventInstances;

// Declare the function pointer for BR_GetMediaItemGUID
void (*BR_GetMediaItemGUID)(MediaItem* item, char* guidStringOut, int guidStringOut_sz) = nullptr;


void LoadReaperAPIFunctions(reaper_plugin_info_t* rec) {
    if (rec && rec->GetFunc) {
        GetUserFileNameForRead = (bool (*)(char*, const char*, const char*))rec->GetFunc("GetUserFileNameForRead");
        plugin_getapi = reinterpret_cast<decltype(plugin_getapi)>(rec->GetFunc("plugin_getapi"));
        plugin_register = reinterpret_cast<decltype(plugin_register)>(rec->GetFunc("plugin_register"));
        ShowMessageBox = reinterpret_cast<decltype(ShowMessageBox)>(rec->GetFunc("ShowMessageBox"));
        ShowConsoleMsg = reinterpret_cast<decltype(ShowConsoleMsg)>(rec->GetFunc("ShowConsoleMsg"));
        GetPlayState = reinterpret_cast<decltype(GetPlayState)>(rec->GetFunc("GetPlayState"));
        GetPlayPosition = reinterpret_cast<decltype(GetPlayPosition)>(rec->GetFunc("GetPlayPosition"));
        EnumProjectMarkers = reinterpret_cast<decltype(EnumProjectMarkers)>(rec->GetFunc("EnumProjectMarkers"));
        CountProjectMarkers = reinterpret_cast<decltype(CountProjectMarkers)>(rec->GetFunc("CountProjectMarkers"));
        AddProjectMarker2 = reinterpret_cast<decltype(AddProjectMarker2)>(rec->GetFunc("AddProjectMarker2"));
        GetCursorPosition = reinterpret_cast<decltype(GetCursorPosition)>(rec->GetFunc("GetCursorPosition"));
        CountTracks = reinterpret_cast<decltype(CountTracks)>(rec->GetFunc("CountTracks"));
        GetTrack = reinterpret_cast<decltype(GetTrack)>(rec->GetFunc("GetTrack"));
        GetSetMediaTrackInfo = reinterpret_cast<decltype(GetSetMediaTrackInfo)>(rec->GetFunc("GetSetMediaTrackInfo"));
        CountTrackMediaItems = reinterpret_cast<decltype(CountTrackMediaItems)>(rec->GetFunc("CountTrackMediaItems"));
        GetTrackMediaItem = reinterpret_cast<decltype(GetTrackMediaItem)>(rec->GetFunc("GetTrackMediaItem"));
        GetActiveTake = reinterpret_cast<decltype(GetActiveTake)>(rec->GetFunc("GetActiveTake"));
        GetSetMediaItemTakeInfo = reinterpret_cast<decltype(GetSetMediaItemTakeInfo)>(rec->GetFunc("GetSetMediaItemTakeInfo"));
        GetSetMediaItemInfo = reinterpret_cast<decltype(GetSetMediaItemInfo)>(rec->GetFunc("GetSetMediaItemInfo"));
        GetSetMediaItemInfo_String = reinterpret_cast<decltype(GetSetMediaItemInfo_String)>(rec->GetFunc("GetSetMediaItemInfo_String"));
        AddMediaItemToTrack = reinterpret_cast<decltype(AddMediaItemToTrack)>(rec->GetFunc("AddMediaItemToTrack"));
        UpdateArrange = reinterpret_cast<decltype(UpdateArrange)>(rec->GetFunc("UpdateArrange"));
        GetSetProjectGrid = reinterpret_cast<decltype(GetSetProjectGrid)>(rec->GetFunc("GetSetProjectGrid"));
        GetTempoTimeSigMarker = reinterpret_cast<decltype(GetTempoTimeSigMarker)>(rec->GetFunc("GetTempoTimeSigMarker"));
        Master_GetTempo = reinterpret_cast<decltype(Master_GetTempo)>(rec->GetFunc("Master_GetTempo"));
        SetEditCurPos = reinterpret_cast<decltype(SetEditCurPos)>(rec->GetFunc("SetEditCurPos")); // Load SetEditCurPos
        TimeMap_curFrameRate = reinterpret_cast<decltype(TimeMap_curFrameRate)>(rec->GetFunc("TimeMap_curFrameRate")); // Load TimeMap_curFrameRate
        EnumProjects = reinterpret_cast<decltype(EnumProjects)>(rec->GetFunc("EnumProjects"));
        BR_GetMediaItemGUID = reinterpret_cast<decltype(BR_GetMediaItemGUID)>(rec->GetFunc("BR_GetMediaItemGUID"));
        GetSetMediaItemTakeInfo_String = reinterpret_cast<decltype(GetSetMediaItemTakeInfo_String)>(rec->GetFunc("GetSetMediaItemTakeInfo_String"));
        AddTakeToMediaItem = reinterpret_cast<decltype(AddTakeToMediaItem)>(rec->GetFunc("AddTakeToMediaItem"));
        GetSet_LoopTimeRange = reinterpret_cast<decltype(GetSet_LoopTimeRange)>(rec->GetFunc("GetSet_LoopTimeRange")); // Load GetSet_LoopTimeRange
        GetSelectedMediaItem = reinterpret_cast<decltype(GetSelectedMediaItem)>(rec->GetFunc("GetSelectedMediaItem"));
        CountSelectedMediaItems = reinterpret_cast<decltype(CountSelectedMediaItems)>(rec->GetFunc("CountSelectedMediaItems"));
        GetUserInputs = reinterpret_cast<decltype(GetUserInputs)>(rec->GetFunc("GetUserInputs"));
        GetTrackDepth = reinterpret_cast<decltype(GetTrackDepth)>(rec->GetFunc("GetTrackDepth"));
        GetParentTrack = reinterpret_cast<decltype(GetParentTrack)>(rec->GetFunc("GetParentTrack"));
        GetMediaTrackInfo_Value = reinterpret_cast<decltype(GetMediaTrackInfo_Value)>(rec->GetFunc("GetMediaTrackInfo_Value"));
        GetResourcePath = reinterpret_cast<decltype(GetResourcePath)>(rec->GetFunc("GetResourcePath"));
        TrackFX_AddByName = reinterpret_cast<decltype(TrackFX_AddByName)>(rec->GetFunc("TrackFX_AddByName"));
        GetMediaItemTrack = reinterpret_cast<decltype(GetMediaItemTrack)>(rec->GetFunc("GetMediaItemTrack"));           // Load GetMediaItemTrack
        TakeFX_AddByName = reinterpret_cast<decltype(TakeFX_AddByName)>(rec->GetFunc("TakeFX_AddByName"));
        CreateNewMIDIItemInProj = reinterpret_cast<decltype(CreateNewMIDIItemInProj)>(rec->GetFunc("CreateNewMIDIItemInProj"));
        TakeFX_GetEnvelope = reinterpret_cast<decltype(TakeFX_GetEnvelope)>(rec->GetFunc("TakeFX_GetEnvelope"));                // Load TakeFX_GetEnvelope
        Envelope_Evaluate = reinterpret_cast<decltype(Envelope_Evaluate)>(rec->GetFunc("Envelope_Evaluate"));                    // Load Envelope_Evaluate
        GetSetProjectInfo = reinterpret_cast<decltype(GetSetProjectInfo)>(rec->GetFunc("GetSetProjectInfo"));
    }
}

void toggleDebugMessagesOnOff() {
    if (debugMessages == true){
        debugMessages = false;
    } else {
        debugMessages = true;
    }
}

// A function for showing debug messages in Reaper's console.
void DebugMsg(const char* fmt, ...) {
    // This function can accept fully formated messages
    // or messages that require additional formating.
    // e.g. DebugMsg("Event %d path not found", i);
    if (debugMessages == true) { // only show messages if debugMessages is true
        if (ShowConsoleMsg == nullptr) {
        return; // If ShowConsoleMsg is not initialized, do nothing
        }

        va_list args;
        va_start(args, fmt);

        // Create a buffer to format the message if needed
        char buffer[1024];

        // Check if the format string contains any format specifiers
        bool needsFormatting = false;
        for (const char* p = fmt; *p != '\0'; ++p) {
            if (*p == '%') {
                needsFormatting = true;
                break;
            }
        }

        if (needsFormatting) {
            // Format the message using vsnprintf
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            ShowConsoleMsg(buffer);  // Display the formatted message
        } else {
            // No formatting required, just pass the original message
            ShowConsoleMsg(fmt);
        }

        va_end(args);
    }  
}

bool OpenURLInDefaultBrowser(const std::string& url) {
#if defined(_WIN32)
    HINSTANCE result = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
#elif defined(__APPLE__)
    std::string command = "open \"" + url + "\"";
    return std::system(command.c_str()) == 0;
#else
    std::string command = "xdg-open \"" + url + "\"";
    return std::system(command.c_str()) == 0;
#endif
}

// A function for showing messages in Reaper's console.
void PostMsg(const char* fmt, ...) {
    // This function is intended for non-debug related messages.
    // This function can accept fully formated messages
    // or messages that require additional formating
    // e.g. DebugMsg("Event %d path not found", i);
    
    if (ShowConsoleMsg == nullptr) {
    return; // If ShowConsoleMsg is not initialized, do nothing
    }

    va_list args;
    va_start(args, fmt);

    // Create a buffer to format the message if needed
    char buffer[1024];

    // Check if the format string contains any format specifiers
    bool needsFormatting = false;
    for (const char* p = fmt; *p != '\0'; ++p) {
        if (*p == '%') {
            needsFormatting = true;
            break;
        }
    }

    if (needsFormatting) {
        // Format the message using vsnprintf
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        ShowConsoleMsg(buffer);  // Display the formatted message
    } else {
        // No formatting required, just pass the original message
        ShowConsoleMsg(fmt);
    }

    va_end(args);
}

std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
        return ""; // All spaces
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// Function to split a string by a delimiter into a vector of strings
std::vector<std::string> SplitString(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    
    // Add the last token
    tokens.push_back(str.substr(start));
    
    return tokens;
}

// Initialize FMOD system
void InitializeFMOD() {
    FMOD::Studio::System::create(&fmod_system);

    unsigned int studioInitFlags = FMOD_STUDIO_INIT_NORMAL | FMOD_STUDIO_INIT_LIVEUPDATE;
    unsigned int initFlags = FMOD_INIT_NORMAL;

    FMOD_RESULT result = fmod_system->initialize(512, studioInitFlags, initFlags, nullptr);
    if (result != FMOD_OK) {
        DebugMsg("Failed to initialize FMOD system with Live Update. FMOD_RESULT: %d\n", result);
        return;
    }
    DebugMsg("Initialized FMOD System with Live Update enabled.\n");
}

// Function to check if FMOD System is Initialzed
bool IsFMODInitialized() {
    if (fmod_system) {
        // DebugMsg("Checking if FMOD System is initialized...\n");
        FMOD::System* coreSystem = nullptr;
        FMOD_RESULT result = fmod_system->getCoreSystem(&coreSystem);  // Get the core system
        
        if (result == FMOD_OK && coreSystem) {
            // DebugMsg("FMOD System is initialized.\n");
            return true;  // FMOD is initialized
        }
    }
    // DebugMsg("FMOD System is NOT initialized.\n");
    return false;  // FMOD is not initialized
}

// Declare at global scope or as a class member
std::map<std::string, std::vector<GlobalParameter>> groupedGlobalParameters;

void RetrieveGlobalParameters() {
    globalParameters.clear();           // Clear any existing parameters
    groupedGlobalParameters.clear();    // Clear existing grouped parameters

    // If no banks are loaded, no need to retrieve parameters
    if (loaded_banks.empty()) {
        return;
    }

    // Get the number of global parameters
    int numGlobalParameters = 0;
    FMOD_RESULT result = fmod_system->getParameterDescriptionCount(&numGlobalParameters);
    if (result != FMOD_OK) {
        return;
    }

    // Allocate an array to hold all global parameter descriptions
    std::vector<FMOD_STUDIO_PARAMETER_DESCRIPTION> paramDescriptions(numGlobalParameters);

    // Retrieve the list of global parameters
    int count = 0;
    result = fmod_system->getParameterDescriptionList(paramDescriptions.data(), numGlobalParameters, &count);
    if (result != FMOD_OK) {
        return;
    }

    // Loop through the retrieved global parameters
    for (int i = 0; i < count; ++i) {
        const FMOD_STUDIO_PARAMETER_DESCRIPTION& paramDesc = paramDescriptions[i];

        GlobalParameter globalParam;
        globalParam.name = paramDesc.name;
        globalParam.minValue = paramDesc.minimum;
        globalParam.maxValue = paramDesc.maximum;
        
        // Get the current value for the global parameter by name
        float currentValue = 0.0f;
        result = fmod_system->getParameterByName(paramDesc.name, &currentValue);
        if (result == FMOD_OK) {
            globalParam.currentValue = currentValue;
        }

        // Extract prefix from parameter name
        std::string paramName = paramDesc.name;
        size_t pos = paramName.find_first_of("_");
        std::string prefix;
        if (pos != std::string::npos && pos > 0) {
            prefix = paramName.substr(0, pos);
        } else {
            prefix = "No Prefix";
        }

        // Add this global parameter to the appropriate group
        groupedGlobalParameters[prefix].push_back(globalParam);

        // Optionally, add this global parameter to the list
        globalParameters.push_back(globalParam);
    }

    // **Sort parameters within each group**
    for (auto& group : groupedGlobalParameters) {
        std::vector<GlobalParameter>& parameters = group.second;
        std::sort(parameters.begin(), parameters.end(), [](const GlobalParameter& a, const GlobalParameter& b) {
            return a.name < b.name;
        });
    }
}

// Load a bank and retrieve its events
void LoadBank(const std::string& bank_path, bool load_sample_data = true) {
    if (loaded_banks.find(bank_path) == loaded_banks.end()) {
        FMOD::Studio::Bank* bank = nullptr;
        FMOD_RESULT result = fmod_system->loadBankFile(bank_path.c_str(), FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);
        DebugMsg("Loading bank: %s\n", bank_path.c_str());
        if (result == FMOD_OK) {
            // Debug message to indicate that the bank is being loaded

            loaded_banks[bank_path] = bank;
            if (load_sample_data) {
                // Load the sample data for the bank
                bank->loadSampleData();
            }

            // Retrieve event descriptions
            int event_count = 0;
            bank->getEventCount(&event_count);
            if (event_count > 0) {
                std::vector<std::string> events;
                std::vector<FMOD::Studio::EventDescription*> event_descriptions(event_count);
                bank->getEventList(event_descriptions.data(), event_count, &event_count);

                for (int i = 0; i < event_count; ++i) {
                    char event_path[512];
                    event_descriptions[i]->getPath(event_path, sizeof(event_path), nullptr);
                    events.push_back(event_path);
                }

                bank_events[bank_path] = events;
            }
        } else {
            // Debug message for failure to load the bank
            DebugMsg("Failed to load bank: %s\n", bank_path.c_str());
        }
    }
}

void SynchronizeLoadedBanks() {
    if (!fmod_system) {
        return;
    }

    bool systemNeedsUpdate = false;
    std::unordered_set<std::string> trackedBanks(bank_files.begin(), bank_files.end());

    for (size_t i = 0; i < bank_files.size(); ++i) {
        const std::string& bankPath = bank_files[i];
        bool shouldBeLoaded = bank_load_states[i];
        auto loadedIt = loaded_banks.find(bankPath);

        if (shouldBeLoaded) {
            if (loadedIt == loaded_banks.end()) {
                LoadBank(bankPath);
                systemNeedsUpdate = true;
            }
        } else if (loadedIt != loaded_banks.end()) {
            FMOD::Studio::Bank* bank = loadedIt->second;
            if (bank) {
                bank->unload();
                systemNeedsUpdate = true;
            }
            bank_events.erase(bankPath);
            loaded_banks.erase(loadedIt);
        }
    }

    for (auto it = loaded_banks.begin(); it != loaded_banks.end();) {
        const std::string& bankPath = it->first;
        if (trackedBanks.find(bankPath) == trackedBanks.end() &&
            bankPath.find("Master.strings.bank") == std::string::npos) {
            FMOD::Studio::Bank* bank = it->second;
            if (bank) {
                bank->unload();
                systemNeedsUpdate = true;
            }
            bank_events.erase(bankPath);
            it = loaded_banks.erase(it);
        } else {
            ++it;
        }
    }

    if (systemNeedsUpdate) {
        fmod_system->update();
    }
}

void UnloadAllBanks() {
    // Iterate over all loaded banks and unload them
    for (auto& bankPair : loaded_banks) {
        FMOD::Studio::Bank* bank = bankPair.second;
        if (bank) {
            bank->unload();
        }
    }
    
    //Update FMOD System
    fmod_system->update();

    // Clear the maps and vectors after unloading banks
    loaded_banks.clear();
    bank_files.clear();
    bank_load_states.clear();
    bank_events.clear();
    masterStringEvents.clear();
    globalParameters.clear();
    groupedGlobalParameters.clear();
}

void RefreshBankFiles() {
    if (!fmod_system) {
        bank_files.clear();
        bank_load_states.clear();
        masterStringEvents.clear();
        return;
    }

    if (customBankDirectories.empty()) {
        UnloadAllBanks();
        return;
    }

    std::unordered_map<std::string, bool> previousLoadStates;
    for (size_t i = 0; i < bank_files.size(); ++i) {
        previousLoadStates[bank_files[i]] = bank_load_states[i];
    }

    std::unordered_set<std::string> seenBanks;
    std::unordered_set<std::string> seenMasterStrings;
    std::vector<std::string> masterStringsPaths;
    std::vector<std::string> masterBanks;
    std::vector<std::string> otherBanks;

    for (const std::string& directory : customBankDirectories) {
        if (directory.empty()) {
            continue;
        }

        fs::path dirPath = fs::path(directory);
        std::error_code dirError;
        if (!fs::exists(dirPath, dirError) || !fs::is_directory(dirPath, dirError)) {
            continue;
        }

        std::vector<fs::directory_entry> entries;
        dirError.clear();
        fs::directory_iterator dirIt(dirPath, dirError);
        for (; !dirError && dirIt != fs::directory_iterator(); dirIt.increment(dirError)) {
            entries.push_back(*dirIt);
        }

        if (dirError) {
            continue;
        }

        for (const auto& entry : entries) {
            std::error_code entryError;
            if (!entry.is_regular_file(entryError) || entryError) {
                continue;
            }

            if (entry.path().filename() == "Master.strings.bank") {
                std::string normalizedPath = entry.path().lexically_normal().string();
                if (seenMasterStrings.insert(normalizedPath).second) {
                    masterStringsPaths.push_back(normalizedPath);
                }
            }
        }

        for (const auto& entry : entries) {
            std::error_code entryError;
            if (!entry.is_regular_file(entryError) || entryError) {
                continue;
            }

            if (entry.path().filename() == "Master.bank") {
                std::string normalizedPath = entry.path().lexically_normal().string();
                if (seenBanks.insert(normalizedPath).second) {
                    masterBanks.push_back(normalizedPath);
                }
            }
        }

        std::vector<std::string> localOtherBanks;
        for (const auto& entry : entries) {
            std::error_code entryError;
            if (!entry.is_regular_file(entryError) || entryError) {
                continue;
            }

            if (entry.path().extension() == ".bank") {
                std::string fileName = entry.path().filename().string();
                if (fileName == "Master.bank" || fileName == "Master.strings.bank") {
                    continue;
                }

                std::string normalizedPath = entry.path().lexically_normal().string();
                if (seenBanks.insert(normalizedPath).second) {
                    localOtherBanks.push_back(normalizedPath);
                }
            }
        }

        std::sort(localOtherBanks.begin(), localOtherBanks.end());
        otherBanks.insert(otherBanks.end(), localOtherBanks.begin(), localOtherBanks.end());
    }

    std::unordered_set<std::string> banksToKeep(masterBanks.begin(), masterBanks.end());
    banksToKeep.insert(otherBanks.begin(), otherBanks.end());
    std::unordered_set<std::string> masterStringsToKeep(masterStringsPaths.begin(), masterStringsPaths.end());

    bool unloadedAny = false;
    for (auto it = loaded_banks.begin(); it != loaded_banks.end();) {
        const std::string& path = it->first;
        if (banksToKeep.find(path) == banksToKeep.end() &&
            masterStringsToKeep.find(path) == masterStringsToKeep.end()) {
            FMOD::Studio::Bank* bank = it->second;
            if (bank) {
                bank->unload();
                unloadedAny = true;
            }
            bank_events.erase(path);
            it = loaded_banks.erase(it);
        } else {
            ++it;
        }
    }

    if (unloadedAny && fmod_system) {
        fmod_system->update();
    }

    masterStringEvents.clear();
    for (const std::string& masterStringsPath : masterStringsPaths) {
        if (loaded_banks.find(masterStringsPath) == loaded_banks.end()) {
            LoadBank(masterStringsPath, false);
        }

        auto bankIt = loaded_banks.find(masterStringsPath);
        if (bankIt != loaded_banks.end()) {
            FMOD::Studio::Bank* masterStringsBank = bankIt->second;
            if (masterStringsBank) {
                int stringCount = 0;
                if (masterStringsBank->getStringCount(&stringCount) == FMOD_OK) {
                    for (int i = 0; i < stringCount; ++i) {
                        char path[512];
                        FMOD_GUID guid;
                        if (masterStringsBank->getStringInfo(i, &guid, path, sizeof(path), nullptr) == FMOD_OK) {
                            masterStringEvents.push_back(path);
                        }
                    }
                }
            }
        }
    }

    std::sort(masterStringEvents.begin(), masterStringEvents.end());
    masterStringEvents.erase(std::unique(masterStringEvents.begin(), masterStringEvents.end()), masterStringEvents.end());

    bank_files.clear();
    bank_load_states.clear();

    for (const std::string& bankPath : masterBanks) {
        bool shouldLoad = true;
        auto previousIt = previousLoadStates.find(bankPath);
        if (previousIt != previousLoadStates.end()) {
            shouldLoad = previousIt->second;
        }

        bank_files.push_back(bankPath);
        bank_load_states.push_back(shouldLoad);
    }

    for (const std::string& bankPath : otherBanks) {
        bool shouldLoad = false;
        auto previousIt = previousLoadStates.find(bankPath);
        if (previousIt != previousLoadStates.end()) {
            shouldLoad = previousIt->second;
        }

        bank_files.push_back(bankPath);
        bank_load_states.push_back(shouldLoad);
    }

    SynchronizeLoadedBanks();
}

// Function to find all .bank files in the "Build/Desktop/" directory relative to the selected .fspro file
void FindBankFiles(const std::string& fspro_dir) {
    customBankDirectories.clear();
    if (!fspro_dir.empty()) {
        fs::path bankDirectory = fs::path(fspro_dir) / "Build" / "Desktop";
        customBankDirectories.push_back(bankDirectory.lexically_normal().string());
    }

    RefreshBankFiles();
}

// Function to open the file dialog and extract file name
void OpenFileDialog() {
    const char* filter = "*.fspro";

    // Open file dialog to select .fspro file
    bool retval = GetUserFileNameForRead(selected_file_path, "Select FMOD Project", filter);
    
    // Extract the file name from the full path and search for .bank files
    if (retval) {
        // Unload any previously loaded banks
        UnloadAllBanks();
        // Find the last '/' or '\\' to extract the file name and directory
        std::string file_path(selected_file_path);
        size_t last_slash_pos = file_path.find_last_of('/');
        size_t last_backslash_pos = file_path.find_last_of('\\');

        // Use the larger of the two positions (whichever one exists)
        size_t pos = (last_slash_pos == std::string::npos) ? last_backslash_pos : 
                     (last_backslash_pos == std::string::npos) ? last_slash_pos :
                     (last_slash_pos > last_backslash_pos ? last_slash_pos : last_backslash_pos);

        // Extract the file name and copy it into selected_file_name
        std::string file_name = file_path.substr(pos + 1);
        std::strncpy(selected_file_name, file_name.c_str(), FILE_PATH_BUFFER_SIZE - 1);
        selected_file_name[FILE_PATH_BUFFER_SIZE - 1] = '\0';

        // Extract the directory of the .fspro file
        std::string fspro_directory = file_path.substr(0, pos);
        fmodProjectDirectory = fspro_directory;

        // Find the .bank files in the "Build/Desktop/" directory
        FindBankFiles(fspro_directory);
        RetrieveGlobalParameters();
    } else {
        // Unload any previously loaded banks
        UnloadAllBanks();
    }
}

void PlayEvent(const std::string& event_path) {
    FMOD::Studio::EventDescription* event_description = nullptr;
    fmod_system->getEvent(event_path.c_str(), &event_description);

    if (event_description) {
        FMOD::Studio::EventInstance* event_instance = nullptr;
        event_description->createInstance(&event_instance);
        event_instance->start();
        event_instance->release();  // Automatically release after playback
    }
    fmod_system->update();
}

// bool isAddingMarker = false;

// void AddMarkerWithSelectedEvent() {
//     if (isAddingMarker) return; // Prevent re-entrant calls
//     isAddingMarker = true;

//     if (selectedFMODEvent.empty()) {
//         PostMsg("No FMOD event has been triggered yet.\n");
//         return;
//     }

//     // Get the current edit cursor position
//     double cursorPosition = GetCursorPosition();

//     // Create a new marker at the cursor position with the event's full path as the name
//     int color = 0; // Use default color
//     AddProjectMarker2(nullptr, false, cursorPosition, 0.0, selectedFMODEvent.c_str(), -1, color);

//     DebugMsg("Marker added for last FMOD event: %s\n", selectedFMODEvent.c_str());

//     isAddingMarker = false; // Reset flag after completion
// }

void AddItemWithSelectedEventAtEditCursor() {
    if (selectedFMODEvent.empty()) {
        PostMsg("No FMOD event has been triggered yet.\n");
        return;
    }

    // Get the current edit cursor position
    double cursorPosition = GetCursorPosition();

    // Get the currently selected track
    MediaTrack* selectedTrack = GetTrack(nullptr, 0); // Assume track 0 if no track is selected
    int numSelectedTracks = CountTracks(nullptr);

    // Find the first selected track
    for (int i = 0; i < numSelectedTracks; ++i) {
        MediaTrack* track = GetTrack(nullptr, i);
        if (*(bool*)GetSetMediaTrackInfo(track, "I_SELECTED", nullptr)) {
            selectedTrack = track;
            break;
        }
    }

    if (!selectedTrack) {
        PostMsg("No track is selected.\n");
        return;
    }

    // Calculate the item length based on the frame rate and the number of frames
    bool dropFrame = false;
    double frameRate = TimeMap_curFrameRate(nullptr, &dropFrame);
    double itemLength = numFramesForItem / frameRate; // Set the length to the specified number of frames

    // Set looping to false
    bool loop = false;

    // Create a MIDI item on the selected track at the cursor position
    MediaItem* newItem = CreateNewMIDIItemInProj(selectedTrack, cursorPosition, cursorPosition + itemLength, &loop);
    if (!newItem) {
        PostMsg("Failed to create a new MIDI item.\n");
        return;
    }

    // Add a new take to the item (it will be MIDI by default)
    MediaItem_Take* newTake = GetActiveTake(newItem);
    if (!newTake) {
        PostMsg("Failed to create a new take for the item.\n");
        return;
    }

    // Set the selectedFMODEvent as the name for the take
    GetSetMediaItemTakeInfo_String(newTake, "P_NAME", const_cast<char*>(selectedFMODEvent.c_str()), true);

    // Build the item's notes with the current parameter values
    std::string itemNotes;
    for (const auto& param : selectedEventParameters) {
        itemNotes += "param:" + param.name + "=" + std::to_string(param.currentValue) + "\n";
    }

    // Set the item's notes
    GetSetMediaItemInfo_String(newItem, "P_NOTES", const_cast<char*>(itemNotes.c_str()), true);

    // Update the arrangement view
    UpdateArrange();

    if (moveCursorAfterInsert) {
        // Move the edit cursor forward by the item length
        double newCursorPosition = cursorPosition + itemLength;
        SetEditCurPos(newCursorPosition, true, false);
    }

    DebugMsg("MIDI item added at position %.2f with take name: %s\n", cursorPosition, selectedFMODEvent.c_str());
}


void AddItemWithSelectedEventAtEditCursorMatchLength() {
    if (selectedFMODEvent.empty()) {
        PostMsg("No FMOD event has been triggered yet.\n");
        return;
    }

    double cursorPosition = GetCursorPosition();

    MediaTrack* selectedTrack = GetTrack(nullptr, 0); // Assume track 0 if no track is selected
    int numSelectedTracks = CountTracks(nullptr);

    for (int i = 0; i < numSelectedTracks; ++i) {
        MediaTrack* track = GetTrack(nullptr, i);
        if (*(bool*)GetSetMediaTrackInfo(track, "I_SELECTED", nullptr)) {
            selectedTrack = track;
            break;
        }
    }

    if (!selectedTrack) {
        PostMsg("No track is selected.\n");
        return;
    }

    double itemLength = -1.0;
    bool usedEventLength = false;

    double eventLengthSeconds = GetEventLengthSeconds(selectedFMODEvent);
    if (eventLengthSeconds > 0.0) {
        itemLength = eventLengthSeconds;
        usedEventLength = true;
    } else {
        bool dropFrame = false;
        double frameRate = TimeMap_curFrameRate(nullptr, &dropFrame);
        if (frameRate > 0.0) {
            itemLength = numFramesForItem / frameRate;
        }
    }

    if (itemLength <= 0.0) {
        PostMsg("Unable to determine a valid item length.\n");
        return;
    }

    bool loop = false;

    MediaItem* newItem = CreateNewMIDIItemInProj(selectedTrack, cursorPosition, cursorPosition + itemLength, &loop);
    if (!newItem) {
        PostMsg("Failed to create a new MIDI item.\n");
        return;
    }

    MediaItem_Take* newTake = GetActiveTake(newItem);
    if (!newTake) {
        PostMsg("Failed to create a new take for the item.\n");
        return;
    }

    GetSetMediaItemTakeInfo_String(newTake, "P_NAME", const_cast<char*>(selectedFMODEvent.c_str()), true);

    std::string itemNotes;
    for (const auto& param : selectedEventParameters) {
        itemNotes += "param:" + param.name + "=" + std::to_string(param.currentValue) + "\n";
    }

    GetSetMediaItemInfo_String(newItem, "P_NOTES", const_cast<char*>(itemNotes.c_str()), true);

    UpdateArrange();

    if (moveCursorAfterInsert) {
        double newCursorPosition = cursorPosition + itemLength;
        SetEditCurPos(newCursorPosition, true, false);
    }

    const char* lengthSource = usedEventLength ? "FMOD event length" : "frame-based fallback length";
    DebugMsg("MIDI item added at position %.2f with take name: %s (%s, length %.2f seconds)\n",
             cursorPosition,
             selectedFMODEvent.c_str(),
             lengthSource,
             itemLength);
}


void AddItemWithSelectedEventWithinTimeSelection() {
    if (selectedFMODEvent.empty()) {
        PostMsg("No FMOD event has been triggered yet.\n");
        return;
    }

    // Get the currently selected track
    MediaTrack* selectedTrack = GetTrack(nullptr, 0); // Assume track 0 if no track is selected
    int numSelectedTracks = CountTracks(nullptr);

    // Find the first selected track
    for (int i = 0; i < numSelectedTracks; ++i) {
        MediaTrack* track = GetTrack(nullptr, i);
        if (*(bool*)GetSetMediaTrackInfo(track, "I_SELECTED", nullptr)) {
            selectedTrack = track;
            break;
        }
    }

    if (!selectedTrack) {
        PostMsg("No track is selected.\n");
        return;
    }

    // Check if a time selection exists
    double timeSelStart, timeSelEnd;
    GetSet_LoopTimeRange(false, false, &timeSelStart, &timeSelEnd, false); // Retrieve the time selection

    if (timeSelEnd <= timeSelStart) {
        PostMsg("No valid time selection exists.\n");
        return;
    }

    // Set looping to false
    bool loop = false;

    // Create a MIDI item on the selected track within the time selection
    MediaItem* newItem = CreateNewMIDIItemInProj(selectedTrack, timeSelStart, timeSelEnd, &loop);
    if (!newItem) {
        PostMsg("Failed to create a new MIDI item.\n");
        return;
    }

    // Update the numFramesForItem if the checkbox is checked
    double itemLength = timeSelEnd = timeSelStart;
    if (updateItemInsertionLength == true) {
        bool dropFrame = false;
        double frameRate = TimeMap_curFrameRate(nullptr, &dropFrame); // Get the project frame rate

        // Convert the item length (in seconds) to frames based on the frame rate
        numFramesForItem = static_cast<int>(itemLength * frameRate);
    }

    // Add a new take to the item (it will be MIDI by default)
    MediaItem_Take* newTake = GetActiveTake(newItem);
    if (!newTake) {
        PostMsg("Failed to create a new take for the item.\n");
        return;
    }

    // Set the selectedFMODEvent as the name for the take
    GetSetMediaItemTakeInfo_String(newTake, "P_NAME", const_cast<char*>(selectedFMODEvent.c_str()), true);

    // Build the item's notes with the current parameter values
    std::string itemNotes;
    for (const auto& param : selectedEventParameters) {
        itemNotes += "param:" + param.name + "=" + std::to_string(param.currentValue) + "\n";
    }

    // Set the item's notes
    GetSetMediaItemInfo_String(newItem, "P_NOTES", const_cast<char*>(itemNotes.c_str()), true);

    // Update the arrangement view
    UpdateArrange();

    if (moveCursorAfterInsert) {
        // Move the edit cursor forward to the end of the inserted item
        SetEditCurPos(timeSelEnd, true, false);
    }

    DebugMsg("MIDI item added within time selection from %.2f to %.2f with take name: %s\n", timeSelStart, timeSelEnd, selectedFMODEvent.c_str());
}

void UpdateNumFramesForItemInsertionFromCurrentTimeSelection() {
    // Check if a time selection exists
    double timeSelStart, timeSelEnd;
    GetSet_LoopTimeRange(false, false, &timeSelStart, &timeSelEnd, false); // Retrieve the time selection

    if (timeSelEnd <= timeSelStart) {
        PostMsg("No valid time selection exists to update numFramesForItem.\n");
        return;
    }

    // Get the frame rate
    bool dropFrame = false;
    double frameRate = TimeMap_curFrameRate(nullptr, &dropFrame);

    if (frameRate <= 0) {
        PostMsg("Invalid frame rate detected.\n");
        return;
    }

    // Calculate the time selection length in seconds
    double timeSelectionLength = timeSelEnd - timeSelStart;

    // Update numFramesForItem based on the time selection and frame rate
    numFramesForItem = static_cast<int>(timeSelectionLength * frameRate);

    // Output a message to confirm the number of frames has been updated
    DebugMsg("numFramesForItem updated based on time selection: %d frames (%.2f seconds)\n", numFramesForItem, timeSelectionLength);
}

// Function to stop all FMOD events
void StopAllEvents() {
    FMOD::Studio::Bus* masterBus = nullptr;
    FMOD_RESULT result = fmod_system->getBus("bus:/", &masterBus);  // Get the master bus

    char busPath[512];
    masterBus->getPath(busPath, sizeof(busPath), nullptr);
    DebugMsg("Stopping all events routed through %sMaster\n", busPath);

    if (result == FMOD_OK && masterBus) {
        result = masterBus->stopAllEvents(FMOD_STUDIO_STOP_ALLOWFADEOUT);  // Stop all events on the master bus
        if (result == FMOD_OK) {
            DebugMsg("All FMOD events stopped.\n");
        } else {
            DebugMsg("Failed to stop events on master bus.\n");
        }
    } else {
        DebugMsg("Failed to get master bus.\n");
    }
    fmod_system->update();  // Ensure FMOD processes the stop command
}

// Function to strip "event:" prefix from the event path for display purposes
std::string StripPathPrefix(const std::string& event_path) {
    const std::string eventPrefix = "event:";
    const std::string snapshotPrefix = "snapshot:";

    if (event_path.rfind(eventPrefix, 0) == 0) { // Check if the "event:" prefix exists at the start
        return event_path.substr(eventPrefix.length()); // Return string without the prefix
    }
    if (event_path.rfind(snapshotPrefix, 0) == 0) { // Check if the "snapshot:" prefix exists at the start
        return event_path.substr(snapshotPrefix.length()); // Return string without the prefix
    }
    return event_path; // Return original path if no prefix
}

// Function to group events and snapshots, storing both stripped and full paths
std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, std::string>>>> GroupEventsAndSnapshotsByPath(const std::vector<std::string>& events) {
    std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, std::string>>>> grouped_folders;

    for (const auto& event : events) {
        bool is_snapshot = (event.find("snapshot:") == 0); // Check if it's a snapshot
        std::string type_folder = is_snapshot ? "Snapshots" : "Events";

        // Strip the prefix from the event path
        std::string stripped_event = StripPathPrefix(event);

        // Split the stripped event into folder and event name
        size_t last_slash_pos = stripped_event.find_last_of('/');
        std::string folder;
        std::string event_name;

        if (last_slash_pos != std::string::npos) {
            // If there is a slash, separate into folder and event name
            folder = stripped_event.substr(0, last_slash_pos);
            event_name = stripped_event.substr(last_slash_pos + 1);
        } else {
            // No slash means it's a top-level event or snapshot
            folder = ""; // Use an empty string to represent the top-level
            event_name = stripped_event;
        }

        // Store both the stripped name for display and the full path for playback
        grouped_folders[type_folder][folder].emplace_back(event_name, event);  // (display_name, full_path)
    }

    return grouped_folders;
}

void CopyToClipboard(const std::string& text) {
    if (!reaMOD_Main_ImGui_Context) {
        DebugMsg("ImGui context is not available, cannot copy to clipboard.\n");
        return;
    }

    const char* clipboardText = text.c_str();
    if (clipboardText && strlen(clipboardText) > 0) {
        DebugMsg("Attempting to copy to clipboard using ImGui API: %s\n", clipboardText);
        ImGui::SetClipboardText(reaMOD_Main_ImGui_Context, clipboardText);
        DebugMsg("Text successfully copied to clipboard: %s\n", clipboardText);
    } else {
        DebugMsg("Clipboard text is empty or null, cannot copy to clipboard.\n");
    }
}

// Map for storing event instances triggered by the ReaMOD window play buttons
std::unordered_map<std::string, FMOD::Studio::EventInstance*> playButtonEventInstances;

void PlayButtonEvent(const std::string& eventPath) {
    FMOD::Studio::EventDescription* eventDesc = nullptr;
    fmod_system->getEvent(eventPath.c_str(), &eventDesc);

    if (eventDesc) {
        FMOD::Studio::EventInstance* eventInstance = nullptr;
        eventDesc->createInstance(&eventInstance);

        // Apply cached parameter values
        auto it = eventParameterCache.find(eventPath);
        if (it != eventParameterCache.end()) {
            for (const auto& param : it->second) {
                eventInstance->setParameterByName(param.name.c_str(), param.currentValue);
            }
        }

        eventInstance->start();
        playButtonEventInstances[eventPath] = eventInstance;
    }
    fmod_system->update();
}

void StopButtonEvent(const std::string& eventPath) {
    auto it = playButtonEventInstances.find(eventPath);
    if (it != playButtonEventInstances.end()) {
        FMOD::Studio::EventInstance* eventInstance = it->second;
        FMOD_RESULT result = eventInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
        if (result != FMOD_OK) {
            DebugMsg("Failed to stop event: %s.", eventPath.c_str());
        }
        eventInstance->release();
        playButtonEventInstances.erase(it);

        // Explicitly update the button state to inactive
        if (buttonStates.find(eventPath) != buttonStates.end()) {
            buttonStates[eventPath] = false;
        } else {
            // If the event wasn't in buttonStates, initialize it
            buttonStates[eventPath] = false;
        }

        // Optionally, log the state change
        DebugMsg("Stopped FMOD event: %s. Button state set to inactive.\n", eventPath.c_str());
    }
    fmod_system->update();
}


void PlayStopCurrentSelectedEvent() {
    if (selectedFMODEvent.empty()) {
        return;
    }

    // Check if the selected event is currently playing
    auto it = playButtonEventInstances.find(selectedFMODEvent);
    if (it != playButtonEventInstances.end()) {
        // Event is currently playing; stop it
        StopButtonEvent(selectedFMODEvent);
    } else {
        // Event is not playing; start it
        PlayButtonEvent(selectedFMODEvent);
    }
}

double GetEventLengthSeconds(const std::string& eventPath) {
    if (!fmod_system || eventPath.empty()) {
        return -1.0;
    }

    FMOD::Studio::EventDescription* eventDescription = nullptr;
    FMOD_RESULT result = fmod_system->getEvent(eventPath.c_str(), &eventDescription);
    if (result != FMOD_OK || !eventDescription) {
        return -1.0;
    }

    int lengthMs = 0;
    result = eventDescription->getLength(&lengthMs);
    if (result != FMOD_OK || lengthMs <= 0) {
        return -1.0;
    }

    return static_cast<double>(lengthMs) / 1000.0;
}

bool UpdateSelectedEventParameters(const std::string& eventPath) {
    if (eventPath.empty()) {
        selectedEventParameters.clear(); // Clear if no event is selected
        return false;
    }

    // Check if parameters are already cached for this event
    auto it = eventParameterCache.find(eventPath);
    if (it != eventParameterCache.end()) {
        // Use cached parameters
        selectedEventParameters = it->second;
        selectedFMODEvent = eventPath; // Update selectedFMODEvent
        return true;
    }

    // Else, load parameters from FMOD and cache them
    FMOD::Studio::EventDescription* eventDesc = nullptr;
    FMOD_RESULT result = fmod_system->getEvent(eventPath.c_str(), &eventDesc);
    if (result != FMOD_OK || !eventDesc) {
        DebugMsg("Failed to get EventDescription for event: %s\n", eventPath.c_str());
        return false;
    }

    int paramCount = 0;
    result = eventDesc->getParameterDescriptionCount(&paramCount);
    if (result != FMOD_OK) {
        DebugMsg("Failed to get parameter description count for event: %s\n", eventPath.c_str());
        return false;
    }

    selectedEventParameters.clear(); // Clear existing parameters
    for (int i = 0; i < paramCount; ++i) {
        FMOD_STUDIO_PARAMETER_DESCRIPTION paramDesc;
        result = eventDesc->getParameterDescriptionByIndex(i, &paramDesc);
        if (result != FMOD_OK) {
            DebugMsg("Failed to get parameter description by index %d for event: %s\n", i, eventPath.c_str());
            continue;
        }

        ParameterInfo paramInfo;
        paramInfo.name = paramDesc.name;
        paramInfo.id = paramDesc.id;
        paramInfo.minValue = paramDesc.minimum;
        paramInfo.maxValue = paramDesc.maximum;
        paramInfo.defaultValue = paramDesc.defaultvalue;
        paramInfo.currentValue = paramDesc.defaultvalue; // Initialize to default value

        selectedEventParameters.push_back(paramInfo);
    }

    // Cache the parameters
    eventParameterCache[eventPath] = selectedEventParameters;

    // Update selectedFMODEvent
    selectedFMODEvent = eventPath;

    return true;
    DebugMsg("Selected FMOD Event Updated to %s and its paramters have been loaded.\n", selectedFMODEvent.c_str());
}

// Use the ReaImGui MouseButton_Right enum or value
const int RightMouseButton = ImGui::MouseButton_Right;

// Function to render a toggleable play button and trigger/release FMOD event
bool RenderPlayButton(ImGui_Context* ctx, const std::string& button_id, const std::string& event_path) {
    // Unique ID for ImGui, but use event_path for state management
    std::string unique_button_id = "##play_button_" + button_id;

    // Use event_path as the key for buttonStates
    bool is_active = buttonStates[event_path];

    // Push button color based on its state
    if (is_active) {
        ImGui::PushStyleColor(ctx, ImGui::Col_Button, 0x32CD32FF); // Active (green)
        ImGui::PushStyleColor(ctx, ImGui::Col_ButtonHovered, 0x008000FF); // Dark green when hovered
        ImGui::PushStyleColor(ctx, ImGui::Col_ButtonActive, 0x006400FF); // Even darker green when clicked
    } else {
        ImGui::PushStyleColor(ctx, ImGui::Col_Button, 0xC0C0C0FF); // Inactive (grey)
        ImGui::PushStyleColor(ctx, ImGui::Col_ButtonHovered, 0xA9A9A9FF); // Darker grey when hovered
        ImGui::PushStyleColor(ctx, ImGui::Col_ButtonActive, 0x808080FF); // Dark grey when clicked
    }

    // Render the button
    if (ImGui::ArrowButton(ctx, unique_button_id.c_str(), ImGui::Dir_Right)) {
        // Toggle the state
        buttonStates[event_path] = !is_active;
        is_active = buttonStates[event_path];

        // Update the selected event when the play button is clicked
        selectedFMODEvent = event_path;
        UpdateSelectedEventParameters(selectedFMODEvent); // Call this function here

        // Trigger or release the FMOD event
        if (is_active) {
            PlayButtonEvent(event_path);
            DebugMsg("Play button clicked: Event Path - %s, State: Playing\n", event_path.c_str());
        } else {
            StopButtonEvent(event_path);
            DebugMsg("Play button clicked: Event Path - %s, State: Stopped\n", event_path.c_str());
        }
    }

    // Pop the style colors
    ImGui::PopStyleColor(ctx, 3);

    return is_active;
}

// void CheckMarkers(double playPosition) {
//     if (CountProjectMarkers == nullptr || EnumProjectMarkers == nullptr) {
//         DebugMsg("Marker functions are not available.\n");
//         return;
//     }

//     int numMarkers = 0, numRegions = 0;
//     CountProjectMarkers(nullptr, &numMarkers, &numRegions);  // Count markers and regions

//     int totalMarkersAndRegions = numMarkers + numRegions;
//     double checkAheadWindow = 1.0;  // Check markers 1 second ahead of play position
//     double tolerance = 0.04;        // Small tolerance to account for floating-point inaccuracies

//     // Convert lookAheadTimeMs to seconds
//     double lookAheadTimeSeconds = lookAheadTimeMs / 1000.0;

//     for (int i = 0; i < totalMarkersAndRegions; ++i) {
//         bool isRegion = false;
//         double markerPosition = 0.0, regionEnd = 0.0;
//         const char* name = nullptr;
//         int markerIndex = 0;  // Marker index from Reaper

//         // Corrected order of arguments for EnumProjectMarkers
//         if (EnumProjectMarkers(i, &isRegion, &markerPosition, &regionEnd, &name, &markerIndex)) {
//             if (name == nullptr) continue;  // Skip invalid markers

//             std::string markerName(name);

//             // Adjust marker position by look-ahead time
//             double adjustedMarkerPosition = markerPosition - lookAheadTimeSeconds;

//             // Only check markers that are within the 1-second window ahead of the play position
//             if (adjustedMarkerPosition >= playPosition && adjustedMarkerPosition <= playPosition + checkAheadWindow) {
//                 DebugMsg("Checking marker %d: %s at position %.2f\n", markerIndex, markerName.c_str(), markerPosition);

//                 // Trigger the event when the playhead reaches or passes the marker's position (with tolerance)
//                 if (playPosition >= adjustedMarkerPosition - tolerance && playPosition <= adjustedMarkerPosition + tolerance) {
//                     // Check if this marker was already triggered
//                     if (!triggeredMarkers[markerIndex]) {
//                         DebugMsg("Triggering event for marker: %s at position %.2f\n", markerName.c_str(), markerPosition);
//                         PlayEvent(markerName);  // Trigger the FMOD event or snapshot
//                         triggeredMarkers[markerIndex] = true;  // Mark this marker as triggered
//                     }
//                 }
//             }
//         } else {
//             DebugMsg("Failed to retrieve marker %d\n", i);
//         }
//     }
// }

// Function to split a string by a delimiter into a vector of strings
std::string GetItemGUID(MediaItem* item) {
    if (!item) {
        DebugMsg("GetItemGUID: MediaItem is null.\n");
        return "";
    }

    char guidStr[64] = {0}; // Buffer to hold the GUID string
    bool success = GetSetMediaItemInfo_String(item, "GUID", guidStr, false); // Retrieve the GUID as a string

    if (!success || strlen(guidStr) == 0) {
        DebugMsg("GetItemGUID: Failed to retrieve GUID for MediaItem.\n");
        return ""; // Return an empty string if retrieval fails or GUID is empty
    }

    return std::string(guidStr);
}

void CopySelectedMediaItemGUIDToClipboard() {
    // Get the first selected media item (ignoring track selection)
    MediaItem* selectedItem = GetSelectedMediaItem(nullptr, 0); // Pass 0 to get the first selected item

    if (!selectedItem) {
        PostMsg("No media item is selected.\n");
        return;
    }

    // Retrieve the GUID of the selected media item using the existing function
    std::string guidStr = GetItemGUID(selectedItem);

    // Check if the GUID is valid
    if (guidStr.empty()) {
        PostMsg("Failed to retrieve GUID for the selected media item.\n");
        return;
    }

    // Copy the GUID to the clipboard using the existing CopyToClipboard function
    CopyToClipboard(guidStr);

    DebugMsg("Copied selected media item GUID to clipboard: %s\n", guidStr.c_str());
}

void InsertParamUpdateItemForSelectedMediaItem() {
    // Get the first selected media item (ignoring track selection)
    MediaItem* selectedItem = GetSelectedMediaItem(nullptr, 0); // Pass 0 to get the first selected item

    if (!selectedItem) {
        PostMsg("No media item is selected.\n");
        return;
    }

    // Retrieve the GUID of the selected media item using the existing function
    std::string guidStr = GetItemGUID(selectedItem);

    // Check if the GUID is valid
    if (guidStr.empty()) {
        PostMsg("Failed to retrieve GUID for the selected media item.\n");
        return;
    }

    // Get the current edit cursor position
    double cursorPosition = GetCursorPosition();

    // Get the currently selected track
    MediaTrack* selectedTrack = GetTrack(nullptr, 0); // Assume track 0 if no track is selected
    int numSelectedTracks = CountTracks(nullptr);

    // Find the first selected track
    for (int i = 0; i < numSelectedTracks; ++i) {
        MediaTrack* track = GetTrack(nullptr, i);
        if (*(bool*)GetSetMediaTrackInfo(track, "I_SELECTED", nullptr)) {
            selectedTrack = track;
            break;
        }
    }

    if (!selectedTrack) {
        PostMsg("No track is selected.\n");
        return;
    }

    // Add a media item on the selected track at the cursor position
    MediaItem* newItem = AddMediaItemToTrack(selectedTrack);
    if (!newItem) {
        PostMsg("Failed to create a new item.\n");
        return;
    }

    // Set the item position to the cursor
    GetSetMediaItemInfo(newItem, "D_POSITION", &cursorPosition);

    // Determine the number of frames from current time selection
    double timeSelStart, timeSelEnd;
    GetSet_LoopTimeRange(false, false, &timeSelStart, &timeSelEnd, false);

    if (timeSelEnd <= timeSelStart) {
        PostMsg("No valid time selection exists to determine number of frames.\n");
        return;
    }

    double timeSelectionLength = timeSelEnd - timeSelStart;

    bool dropFrame = false;
    double frameRate = TimeMap_curFrameRate(nullptr, &dropFrame);

    // Calculate the number of frames for this item from the time selection
    double totalSeconds = timeSelectionLength; 
    int framesFromTimeSelection = static_cast<int>(totalSeconds * frameRate);
    if (framesFromTimeSelection < 1) framesFromTimeSelection = 1; // At least one frame

    double itemLength = framesFromTimeSelection / frameRate; // length in seconds

    // Set the calculated length for the item
    GetSetMediaItemInfo(newItem, "D_LENGTH", &itemLength);

    // Add a new take to the item
    MediaItem_Take* newTake = AddTakeToMediaItem(newItem);
    if (!newTake) {
        PostMsg("Failed to create a new take for the item.\n");
        return;
    }

    // Set the default parameter name for the take
    std::string paramName = "param:Name=Value";
    GetSetMediaItemTakeInfo_String(newTake, "P_NAME", const_cast<char*>(paramName.c_str()), true);

    // Add the GUID to the item's notes
    std::string itemNotes = "GUID=" + guidStr;
    GetSetMediaItemInfo_String(newItem, "P_NOTES", const_cast<char*>(itemNotes.c_str()), true);

    // Update the arrangement view
    UpdateArrange();

    if (moveCursorAfterInsert) {
        // Move the edit cursor forward by the item length
        double newCursorPosition = cursorPosition + itemLength;
        SetEditCurPos(newCursorPosition, true, false);
    }

    DebugMsg("Inserted param update item at position %.2f with GUID: %s\n", cursorPosition, guidStr.c_str());
}


void InsertPositionInterpolationItemsForSelectedMediaItem() {
    // Get the first selected media item
    MediaItem* selectedItem = GetSelectedMediaItem(nullptr, 0);
    if (!selectedItem) {
        PostMsg("No media item is selected.\n");
        return;
    }

    // Retrieve the GUID of the selected media item
    std::string guidStr = GetItemGUID(selectedItem);
    if (guidStr.empty()) {
        PostMsg("Failed to retrieve GUID for the selected media item.\n");
        return;
    }

    // Prompt the user only for start and end positions (6 values: sx, sy, sz, ex, ey, ez)
    char userInputs[512] = "";
    if (!GetUserInputs("Position Interpolation", 6, 
        "Start X,Start Y,Start Z,End X,End Y,End Z", 
        userInputs, sizeof(userInputs))) 
    {
        PostMsg("User cancelled the input dialog.\n");
        return;
    }

    // Parse the user inputs
    // Expected 6 values: sx, sy, sz, ex, ey, ez
    std::vector<std::string> inputValues = SplitString(userInputs, ",");
    if (inputValues.size() != 6) {
        PostMsg("Invalid input. Please provide 6 values: sx, sy, sz, ex, ey, ez.\n");
        return;
    }

    double sx = std::stod(Trim(inputValues[0]));
    double sy = std::stod(Trim(inputValues[1]));
    double sz = std::stod(Trim(inputValues[2]));
    double ex = std::stod(Trim(inputValues[3]));
    double ey = std::stod(Trim(inputValues[4]));
    double ez = std::stod(Trim(inputValues[5]));

    // Get the current edit cursor position
    double cursorPosition = GetCursorPosition();

    // Get the currently selected track
    MediaTrack* selectedTrack = GetTrack(nullptr, 0); // Default to track 0 if none is selected
    int trackCount = CountTracks(nullptr);

    // Find the first selected track
    for (int i = 0; i < trackCount; ++i) {
        MediaTrack* track = GetTrack(nullptr, i);
        if (*(bool*)GetSetMediaTrackInfo(track, "I_SELECTED", nullptr)) {
            selectedTrack = track;
            break;
        }
    }

    if (!selectedTrack) {
        PostMsg("No track is selected.\n");
        return;
    }

    // Determine number of frames from current time selection
    double timeSelStart, timeSelEnd;
    GetSet_LoopTimeRange(false, false, &timeSelStart, &timeSelEnd, false);

    if (timeSelEnd <= timeSelStart) {
        PostMsg("No valid time selection exists to determine number of frames.\n");
        return;
    }

    double timeSelectionLength = timeSelEnd - timeSelStart;
    bool dropFrame = false;
    double frameRate = TimeMap_curFrameRate(nullptr, &dropFrame);
    if (frameRate <= 0.0) {
        PostMsg("Invalid frame rate.\n");
        return;
    }

    int numFrames = static_cast<int>(timeSelectionLength * frameRate);
    if (numFrames < 2) numFrames = 2; // At least two frames for interpolation

    double frameDuration = 1.0 / frameRate;

    // Interpolate positions for each frame
    for (int i = 0; i < numFrames; ++i) {
        double t = (double)i / (double)(numFrames - 1);
        double x = sx + t * (ex - sx);
        double y = sy + t * (ey - sy);
        double z = sz + t * (ez - sz);

        double itemPosition = cursorPosition + i * frameDuration;
        double itemLength = frameDuration;

        // Add a media item on the selected track at itemPosition
        MediaItem* newItem = AddMediaItemToTrack(selectedTrack);
        if (!newItem) {
            PostMsg("Failed to create a new item.\n");
            continue; // Move to the next frame
        }

        // Set the item position and length
        GetSetMediaItemInfo(newItem, "D_POSITION", &itemPosition);
        GetSetMediaItemInfo(newItem, "D_LENGTH", &itemLength);

        // Add a new take to the item
        MediaItem_Take* newTake = AddTakeToMediaItem(newItem);
        if (!newTake) {
            PostMsg("Failed to create a new take for the item.\n");
            continue;
        }

        // Build the item's notes
        std::string itemNotes = "GUID=" + guidStr + "\n";
        char posLine[256];
        snprintf(posLine, sizeof(posLine), "position: x=%.3f,y=%.3f,z=%.3f", x, y, z);
        itemNotes += posLine;

        GetSetMediaItemInfo_String(newItem, "P_NOTES", const_cast<char*>(itemNotes.c_str()), true);

        // Optionally set the take name to reflect the frame number
        char takeName[128];
        snprintf(takeName, sizeof(takeName), "param:Frame=%d", i);
        GetSetMediaItemTakeInfo_String(newTake, "P_NAME", takeName, true);
    }

    // Update the arrangement view
    UpdateArrange();

    // Move the edit cursor after the last inserted item if desired
    if (moveCursorAfterInsert) {
        double newCursorPosition = cursorPosition + (numFrames * frameDuration);
        SetEditCurPos(newCursorPosition, true, false);
    }

    DebugMsg("Inserted %d position interpolation items from (%.2f,%.2f,%.2f) to (%.2f,%.2f,%.2f) based on time selection.\n",
             numFrames, sx, sy, sz, ex, ey, ez);
}

// Helper function to extract float values from lines like "x=1.0"
bool ExtractFloatValue(const std::string &line, const std::string &key, float &outValue) {
    size_t start = line.find(key);
    if (start == std::string::npos) return false;
    start += key.size();
    // Find the next comma or end of line
    size_t end = line.find(',', start);
    std::string valStr = (end == std::string::npos) ? line.substr(start) : line.substr(start, end - start);
    try {
        outValue = std::stof(valStr);
        return true;
    } catch (...) {
        return false;
    }
}

void ParseAndApplyNotes(FMOD::Studio::EventInstance* eventInstance, const std::vector<std::string>& noteLines) {
    if (!eventInstance) {
        DebugMsg("ParseAndApplyNotes: eventInstance is null, skipping.\n");
        return;
    }

    DebugMsg("ParseAndApplyNotes: Number of note lines to process: %d\n", static_cast<int>(noteLines.size()));

    // Retrieve current 3D attributes (or initialize with defaults if failed)
    FMOD_3D_ATTRIBUTES attributes;
    FMOD_RESULT attrResult = eventInstance->get3DAttributes(&attributes);
    if (attrResult != FMOD_OK) {
        attributes.position = {0.0f, 0.0f, 0.0f};
        attributes.velocity = {0.0f, 0.0f, 0.0f};
        attributes.forward  = {0.0f, 0.0f, 1.0f};
        attributes.up       = {0.0f, 1.0f, 0.0f};
    }

    for (const auto& line : noteLines) {
        if (line.empty()) {
            DebugMsg("ParseAndApplyNotes: Skipping empty line.\n");
            continue; // Skip empty lines
        }

        // Handling parameter setting
        if (line.rfind("parameter:", 0) == 0) {
            auto equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string paramName = line.substr(10, equalPos - 10);
                std::string paramValueStr = line.substr(equalPos + 1);

                if (paramName.empty() || paramValueStr.empty()) {
                    DebugMsg("ParseAndApplyNotes: Parameter name or value is empty in line: %s\n", line.c_str());
                    continue;
                }

                try {
                    float value = std::stof(paramValueStr);
                    FMOD_RESULT result = eventInstance->setParameterByName(paramName.c_str(), value);
                    if (result != FMOD_OK) {
                        DebugMsg("ParseAndApplyNotes: Failed to set parameter '%s', FMOD result: %d\n", paramName.c_str(), result);
                    } else {
                        DebugMsg("ParseAndApplyNotes: Set parameter '%s' to value %.2f\n", paramName.c_str(), value);
                    }
                } catch (const std::invalid_argument&) {
                    DebugMsg("ParseAndApplyNotes: Invalid parameter value for '%s': %s\n", paramName.c_str(), paramValueStr.c_str());
                } catch (const std::out_of_range&) {
                    DebugMsg("ParseAndApplyNotes: Parameter value out of range for '%s': %s\n", paramName.c_str(), paramValueStr.c_str());
                }
            } else {
                DebugMsg("ParseAndApplyNotes: Parameter line does not contain '=': %s\n", line.c_str());
            }
        }
        // Handling snapshot triggering
        else if (line.rfind("snapshot:", 0) == 0) {
            std::string snapshotPath = line.substr(9);
            if (snapshotPath.empty()) {
                DebugMsg("ParseAndApplyNotes: Snapshot path is empty in line: %s\n", line.c_str());
                continue;
            }

            FMOD::Studio::EventDescription* snapshotDesc = nullptr;
            FMOD_RESULT result = fmod_system->getEvent(snapshotPath.c_str(), &snapshotDesc);
            if (result != FMOD_OK || !snapshotDesc) {
                DebugMsg("ParseAndApplyNotes: Failed to get snapshot description for '%s', FMOD result: %d\n", snapshotPath.c_str(), result);
                continue;
            }

            FMOD::Studio::EventInstance* snapshotInstance = nullptr;
            result = snapshotDesc->createInstance(&snapshotInstance);
            if (result != FMOD_OK || !snapshotInstance) {
                DebugMsg("ParseAndApplyNotes: Failed to create snapshot instance for '%s', FMOD result: %d\n", snapshotPath.c_str(), result);
                continue;
            }

            result = snapshotInstance->start();
            if (result != FMOD_OK) {
                DebugMsg("ParseAndApplyNotes: Failed to start snapshot instance for '%s', FMOD result: %d\n", snapshotPath.c_str(), result);
            } else {
                DebugMsg("ParseAndApplyNotes: Snapshot '%s' started successfully.\n", snapshotPath.c_str());
            }

            // Release the snapshot instance after starting it
            snapshotInstance->release();
        }
        // Handling position updates
        else if (line.rfind("position:", 0) == 0) {
            float x=0.0f, y=0.0f, z=0.0f;
            if (ExtractFloatValue(line, "x=", x) &&
                ExtractFloatValue(line, "y=", y) &&
                ExtractFloatValue(line, "z=", z))
            {
                attributes.position = { x, y, z };
                DebugMsg("ParseAndApplyNotes: Set 3D position to X=%.2f, Y=%.2f, Z=%.2f\n", x, y, z);
            } else {
                DebugMsg("ParseAndApplyNotes: Failed to parse position line: %s\n", line.c_str());
            }
        }
        // Handling orientation updates
        else if (line.rfind("orientation:", 0) == 0) {
            float fx=0.0f, fy=0.0f, fz=1.0f;
            float ux=0.0f, uy=1.0f, uz=0.0f;
            if (ExtractFloatValue(line, "fx=", fx) &&
                ExtractFloatValue(line, "fy=", fy) &&
                ExtractFloatValue(line, "fz=", fz) &&
                ExtractFloatValue(line, "ux=", ux) &&
                ExtractFloatValue(line, "uy=", uy) &&
                ExtractFloatValue(line, "uz=", uz))
            {
                attributes.forward = { fx, fy, fz };
                attributes.up      = { ux, uy, uz };
                DebugMsg("ParseAndApplyNotes: Set 3D orientation F=(%.2f, %.2f, %.2f), U=(%.2f, %.2f, %.2f)\n", fx, fy, fz, ux, uy, uz);
            } else {
                DebugMsg("ParseAndApplyNotes: Failed to parse orientation line: %s\n", line.c_str());
            }
        }
        // Unknown line format
        else {
            DebugMsg("ParseAndApplyNotes: Unknown note line format: %s\n", line.c_str());
        }
    }

    // Apply the updated 3D attributes
    FMOD_RESULT setAttrResult = eventInstance->set3DAttributes(&attributes);
    if (setAttrResult != FMOD_OK) {
        DebugMsg("ParseAndApplyNotes: Failed to set 3D attributes, FMOD result: %d\n", setAttrResult);
    } else {
        DebugMsg("ParseAndApplyNotes: 3D attributes updated successfully.\n");
    }
}

void ProcessItemNotes(MediaItem* item, int itemIndex, int trackIndex, FMOD::Studio::EventInstance* eventInstance) {
    if (!item) {
        DebugMsg("ProcessItemNotes: Item is null, skipping.\n");
        return;
    }

    // Buffer to hold the item notes
    char itemNotes[4096];
    // Attempt to retrieve the item notes using GetSetMediaItemInfo_String
    bool hasNotes = GetSetMediaItemInfo_String(item, "P_NOTES", itemNotes, false);

    if (!hasNotes || strlen(itemNotes) == 0) {
        DebugMsg("Item %d on track %d has no notes or failed to retrieve notes. Skipping.\n", itemIndex, trackIndex);
        return; // Skip if no notes are available
    } else {
        DebugMsg("Item %d on track %d, notes retrieved: '%s'\n", itemIndex, trackIndex, itemNotes);
    }

    // Split item notes into individual lines
    std::vector<std::string> noteLines = SplitString(itemNotes, "\n");
    DebugMsg("ParseAndApplyNotes: Number of note lines to process: %d\n", static_cast<int>(noteLines.size()));

    if (noteLines.empty()) {
        DebugMsg("ParseAndApplyNotes: No valid note lines found, skipping item.\n");
        return; // No lines to process
    }

    // Process the note lines
    ParseAndApplyNotes(eventInstance, noteLines);
}

void InsertParamAutomationItemsForSelectedMediaItem() {
    // Get the first selected media item
    MediaItem* selectedItem = GetSelectedMediaItem(nullptr, 0); // Pass 0 to get the first selected item

    if (!selectedItem) {
        PostMsg("No media item is selected.\n");
        return;
    }

    // Get the active take
    MediaItem_Take* take = GetActiveTake(selectedItem);
    if (!take) {
        PostMsg("Selected item has no active take.\n");
        return;
    }

    // Get the take name
    char takeName[512] = "";
    GetSetMediaItemTakeInfo_String(take, "P_NAME", takeName, false);
    std::string takeNameStr(takeName);

    if (takeNameStr.find("event:") != 0) {
        PostMsg("Selected item is not an 'event:' item.\n");
        return;
    }

    // Get the GUID of the selected item
    std::string guidStr = GetItemGUID(selectedItem);
    if (guidStr.empty()) {
        PostMsg("Failed to retrieve GUID for the selected media item.\n");
        return;
    }

    // Prompt the user for Parameter Name, Starting Value, and Ending Value
    char userInputs[512] = ""; // Buffer for return values
    if (!GetUserInputs("Parameter Automation", 3, "Parameter Name,Starting Value,Ending Value", userInputs, sizeof(userInputs))) {
        PostMsg("User cancelled the input dialog.\n");
        return;
    }

    // Split the user inputs
    std::vector<std::string> inputValues = SplitString(userInputs, ",");
    if (inputValues.size() != 3) {
        PostMsg("Invalid input. Please provide Parameter Name, Starting Value, and Ending Value.\n");
        return;
    }

    std::string paramName = Trim(inputValues[0]);
    double startValue = std::stod(Trim(inputValues[1]));
    double endValue = std::stod(Trim(inputValues[2]));

    // Get the current time selection
    double timeSelStart, timeSelEnd;
    GetSet_LoopTimeRange(false, false, &timeSelStart, &timeSelEnd, false); // Retrieve the time selection

    if (timeSelEnd <= timeSelStart) {
        PostMsg("No valid time selection exists.\n");
        return;
    }

    // Get the frame rate
    bool dropFrame = false;
    double frameRate = TimeMap_curFrameRate(nullptr, &dropFrame);
    if (frameRate <= 0) {
        PostMsg("Invalid frame rate detected.\n");
        return;
    }

    // Calculate the number of frames
    double timeSelectionLength = timeSelEnd - timeSelStart;
    int numFrames = static_cast<int>(timeSelectionLength * frameRate);

    if (numFrames < 1) {
        PostMsg("Time selection is too short for frame-based automation.\n");
        return;
    }

    // Calculate the value increment per frame
    double valueIncrement = (endValue - startValue) / (numFrames - 1);

    // Get the currently selected track
    MediaTrack* selectedTrack = GetTrack(nullptr, 0); // Default to the first track if none is selected
    int numTracks = CountTracks(nullptr);

    // Find the first selected track
    bool trackFound = false;
    for (int i = 0; i < numTracks; ++i) {
        MediaTrack* track = GetTrack(nullptr, i);
        if (*(bool*)GetSetMediaTrackInfo(track, "I_SELECTED", nullptr)) {
            selectedTrack = track;
            trackFound = true;
            break;
        }
    }

    if (!trackFound) {
        PostMsg("No track is selected.\n");
        return;
    }

    // Loop over each frame and insert items
    double frameDuration = 1.0 / frameRate;

    for (int i = 0; i < numFrames; ++i) {
        double itemPosition = timeSelStart + i * frameDuration;
        double itemLength = frameDuration;

        double paramValue = startValue + i * valueIncrement;

        // Add a media item on the selected track at the itemPosition
        MediaItem* newItem = AddMediaItemToTrack(selectedTrack);
        if (!newItem) {
            PostMsg("Failed to create a new item.\n");
            continue; // Skip to next frame
        }

        // Set the item position
        GetSetMediaItemInfo(newItem, "D_POSITION", &itemPosition);

        // Set the item length
        GetSetMediaItemInfo(newItem, "D_LENGTH", &itemLength);

        // Add a new take to the item
        MediaItem_Take* newTake = AddTakeToMediaItem(newItem);
        if (!newTake) {
            PostMsg("Failed to create a new take for the item.\n");
            continue; // Skip to next frame
        }

        // Set the take name to "param:ParameterName=Value"
        char takeNameBuffer[512];
        snprintf(takeNameBuffer, sizeof(takeNameBuffer), "param:%s=%.6g", paramName.c_str(), paramValue);
        GetSetMediaItemTakeInfo_String(newTake, "P_NAME", takeNameBuffer, true);

        // Set the item's notes to include the GUID
        std::string itemNotes = "GUID=" + guidStr;
        GetSetMediaItemInfo_String(newItem, "P_NOTES", const_cast<char*>(itemNotes.c_str()), true);
    }

    // Update the arrangement view
    UpdateArrange();
}

void CreateFMODEventInstance(const std::string& eventPath, MediaItem* item, double startPosition, double endPosition) {
    if (!fmod_system) {
        DebugMsg("FMOD system is not initialized. Cannot create event instance.\n");
        return;
    }

    FMOD::Studio::EventDescription* eventDesc = nullptr;
    std::string fullEventPath = eventPath;

    // Ensure the event path starts with "event:" prefix, ignoring case
    if (eventPath.rfind("event:", 0) != 0 && eventPath.rfind("Event:", 0) != 0) {
        fullEventPath = "event:" + eventPath;
    } else if (eventPath.rfind("Event:", 0) == 0) {
        fullEventPath = "event:" + eventPath.substr(6); // Replace "Event:" with "event:"
    }

    // Log the full event path being used
    DebugMsg("Attempting to get event description for: %s\n", fullEventPath.c_str());

    // Try to get the event description
    FMOD_RESULT result = fmod_system->getEvent(fullEventPath.c_str(), &eventDesc);
    if (result != FMOD_OK || !eventDesc) {
        DebugMsg("Failed to get event description for event: %s, FMOD result: %d\n", fullEventPath.c_str(), result);
        return; // Exit the function if the event description couldn't be retrieved
    }

    // Create the event instance
    FMOD::Studio::EventInstance* eventInstance = nullptr;
    result = eventDesc->createInstance(&eventInstance);
    if (result != FMOD_OK || !eventInstance) {
        DebugMsg("Failed to create event instance for event: %s, FMOD result: %d\n", fullEventPath.c_str(), result);
        return; // Exit the function if the event instance couldn't be created
    }

    // Apply any parameters or snapshots from the item notes using ProcessItemNotes
    ProcessItemNotes(item, 0, 0, eventInstance);

    // Start the event instance
    result = eventInstance->start();
    if (result != FMOD_OK) {
        DebugMsg("Failed to start event instance for event: %s, FMOD result: %d\n", fullEventPath.c_str(), result);
        eventInstance->release(); // Ensure resources are released
        return;
    }

    // Log the started event and associated GUID
    std::string itemGUID = GetItemGUID(item);
    DebugMsg("Started FMOD event instance for event: %s with GUID: %s\n", fullEventPath.c_str(), itemGUID.c_str());

    // Store the active event instance for this item
    activeEventInstances[itemGUID] = { eventInstance, startPosition, endPosition };

    fmod_system->update();
}

void ReleaseFMODEventInstance(const std::string& itemGUID) {
    DebugMsg("Releasing FMOD event instance for item GUID: %s\n", itemGUID.c_str());

    auto it = activeEventInstances.find(itemGUID);
    if (it != activeEventInstances.end()) {
        FMOD_RESULT result = it->second.instance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
        if (result != FMOD_OK) {
            DebugMsg("Failed to stop event instance for item GUID: %s, FMOD result: %d\n", itemGUID.c_str(), result);
        } else {
            DebugMsg("Event instance stopped successfully for item GUID: %s\n", itemGUID.c_str());
        }

        result = it->second.instance->release();
        if (result != FMOD_OK) {
            DebugMsg("Failed to release event instance for item GUID: %s, FMOD result: %d\n", itemGUID.c_str());
        } else {
            DebugMsg("Event instance released successfully for item GUID: %s\n", itemGUID.c_str());
        }

        activeEventInstances.erase(it);
        DebugMsg("Removed event instance from active instances for item GUID: %s\n", itemGUID.c_str());
    } else {
        DebugMsg("No active event instance found for item GUID: %s\n", itemGUID.c_str());
    }
    fmod_system->update();
}

std::unordered_map<int, MediaTrack*> fmodTracks;
int cachedTrackCount = 0;

std::string ToLower(const std::string& str) {
    std::string lowerStr(str.size(), ' '); // Initialize with the same size
    std::transform(str.begin(), str.end(), lowerStr.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return lowerStr;
}

void UpdateTrackCache() {
    int currentTrackCount = CountTracks(nullptr);
    fmodTracks.clear();
    int folderDepth = 0; // Initialize folder depth

    for (int i = 0; i < currentTrackCount; ++i) {
        MediaTrack* track = GetTrack(nullptr, i);
        if (!track) continue;

        // Get the track name
        char* trackNameChar = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
        if (trackNameChar) {
            std::string trackName(trackNameChar);
            std::string lowerTrackName = ToLower(trackName);

            // Get folder depth for the current track
            int currentFolderDepth = *(int*)GetSetMediaTrackInfo(track, "I_FOLDERDEPTH", nullptr);
            
            // If folderDepth is > 0, we're inside a parent track, so add the track regardless of its name
            if (folderDepth > 0) {
                fmodTracks[i] = track;
                folderDepth += currentFolderDepth; // Update folder depth

                // Print debug message for added track
                DebugMsg("Added track: %s\n", trackName.c_str());

                // If folderDepth becomes 0, it means we've exited the folder, so stop adding children
                if (folderDepth == 0) {
                    continue;
                }
            } else if (lowerTrackName.find("fmod") != std::string::npos) {
                // If the track contains "FMOD", add the parent track and update folderDepth
                fmodTracks[i] = track;
                folderDepth += currentFolderDepth;

                // Print debug message for added parent track
                DebugMsg("Added FMOD parent track: %s\n", trackName.c_str());

                // If the folder depth becomes 0 immediately, it means this is not a folder track, so we stop here
                if (folderDepth == 0) {
                    continue;
                }
            }
        }
    }

    // Update cached track count
    cachedTrackCount = currentTrackCount;
}

// void PostFmodTracksListToConsole() {
//     // Update the track cache before printing, ensuring it's up-to-date.
//     UpdateTrackCache();

//     // If the track list is empty, print a message
//     if (fmodTracks.empty()) {
//         PostMsg("No FMOD tracks found.\n");
//         return;
//     }

//     // Iterate over the fmodTracks map and post each track's info to the console
//     PostMsg("FMOD Tracks:\n");
//     for (const auto& trackPair : fmodTracks) {
//         int trackIndex = trackPair.first;
//         MediaTrack* track = trackPair.second;

//         // Get the track name
//         char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
//         if (trackName) {
//             PostMsg("      Track %d: %s\n", trackIndex, trackName);
//         }
//     }
// }

bool isTrackActive(MediaTrack* track) {
    // Check if the track itself is muted
    bool trackMuted = GetMediaTrackInfo_Value(track, "B_MUTE") > 0;
    int soloState = (int)GetMediaTrackInfo_Value(track, "I_SOLO");

    // Check if any track in the project is soloed
    bool soloedTracksExist = false;
    for (int i = 0; i < CountTracks(0); ++i) {
        MediaTrack* t = GetTrack(0, i);
        if (GetMediaTrackInfo_Value(t, "I_SOLO") > 0) {
            soloedTracksExist = true;
            break;
        }
    }

    // Recursively check parent track states
    MediaTrack* parentTrack = GetParentTrack(track);
    bool parentSoloed = false;
    while (parentTrack) {
        bool parentMuted = GetMediaTrackInfo_Value(parentTrack, "B_MUTE") > 0;
        int parentSoloState = (int)GetMediaTrackInfo_Value(parentTrack, "I_SOLO");

        // If the parent is muted, the child is muted as well
        if (parentMuted) {
            return false;
        }

        // If the parent is soloed, mark that the child should inherit the solo state
        if (parentSoloState > 0) {
            parentSoloed = true;
        }

        // Move up the parent hierarchy
        parentTrack = GetParentTrack(parentTrack);
    }

    // If the track itself is muted, it is inactive
    if (trackMuted) {
        return false;
    }

    // If any track in the project is soloed:
    if (soloedTracksExist) {
        // This track is active if it is soloed, or if its parent is soloed
        return soloState > 0 || parentSoloed;
    }

    // If no tracks are soloed, this track is active if it's not muted
    return true;
}

void ApplyEnvelopeValueToFMODEvent(const std::string& itemGUID, const std::string& paramName, float value) {
    // Retrieve the active event instance for this item using the GUID
    auto it = activeEventInstances.find(itemGUID);
    if (it == activeEventInstances.end()) {
        DebugMsg("No active FMOD event instance found for GUID: %s\n", itemGUID.c_str());
        return;
    }

    // Apply the parameter value to the FMOD event instance
    FMOD::Studio::EventInstance* eventInstance = it->second.instance;
    FMOD_RESULT result = eventInstance->setParameterByName(paramName.c_str(), value);
    if (result == FMOD_OK) {
        DebugMsg("Updated FMOD event parameter %s to %.2f for item GUID: %s\n", paramName.c_str(), value, itemGUID.c_str());
    } else {
        DebugMsg("Failed to update FMOD event parameter %s for item GUID: %s\n", paramName.c_str(), itemGUID.c_str());
    }

    // Make sure to call the FMOD update function to apply changes
    fmod_system->update();
}

void MonitorEnvelopesForEventItem(MediaItem_Take* take, const std::string& itemGUID) {
    if (!take || itemGUID.empty()) return;

    int fxIndex = 0; // Assuming JSFX is the first FX in the chain

    double projectSampleRate = GetSetProjectInfo(nullptr, "PROJECT_SRATE", 0.0, false);
    if (projectSampleRate <= 0.0) projectSampleRate = 48000.0; // fallback if project SR is not set

    for (int paramIndex = 0; paramIndex < selectedEventParameters.size(); ++paramIndex) {
        // Retrieve the envelope for this parameter
        TrackEnvelope* envelope = TakeFX_GetEnvelope(take, fxIndex, paramIndex, false); // Don't create if it doesn't exist
        if (!envelope) {
            DebugMsg("No envelope for parameter %d on FX %d.\n", paramIndex, fxIndex);
            continue;
        }

        // Evaluate the envelope at the current play position
        double playPosition = GetPlayPosition();
        double envelopeValue = 0.0;
        bool result = Envelope_Evaluate(envelope, playPosition, projectSampleRate, 1, &envelopeValue, nullptr, nullptr, 0);
        if (result) {
            DebugMsg("Envelope value for parameter %s at position %.2f: %.2f\n", selectedEventParameters[paramIndex].name.c_str(), playPosition, envelopeValue);

            // Apply this value to the FMOD event instance
            ApplyEnvelopeValueToFMODEvent(itemGUID, selectedEventParameters[paramIndex].name, envelopeValue);
        }
    }
}

void CheckItems(double playPosition) {
    UpdateTrackCache(); // Refresh the track cache before checking items

    double lookAheadTimeSeconds = lookAheadTimeMs / 1000.0;
    double tolerance = 0.04;

    // Debugging: Print the entire fmodTracks map
    DebugMsg("Number of FMOD tracks to check: %d\n", fmodTracks.size());

    for (const auto& pair : fmodTracks) {
        MediaTrack* track = pair.second;
        char* trackNameChar = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
        std::string trackName(trackNameChar);
        DebugMsg("Checking items on track: %s\n", trackName.c_str());

        if (isTrackActive(track)){
            int itemCount = CountTrackMediaItems(track);
            for (int j = 0; j < itemCount; ++j) {
                MediaItem* item = GetTrackMediaItem(track, j);
                MediaItem_Take* take = GetActiveTake(item);
                if (!take) continue;

                // Check if the media item is muted
                bool isItemMuted = *(bool*)GetSetMediaItemInfo(item, "B_MUTE", nullptr);
                if (isItemMuted) {
                    DebugMsg("Skipping muted item on track: %s\n", trackName.c_str());
                    continue; // Skip to the next item if the item is muted
                }

                double itemStart = *(double*)GetSetMediaItemInfo(item, "D_POSITION", nullptr);
                double itemEnd = itemStart + *(double*)GetSetMediaItemInfo(item, "D_LENGTH", nullptr);
                std::string itemGUID = GetItemGUID(item);

                if (playPosition < previousPlayPosition) {
                    activeEventInstances.clear();  // Reset instances if playhead moved backward
                }

                // Get the item name
                char itemName[512] = "";
                if (GetSetMediaItemTakeInfo_String(take, "P_NAME", itemName, false)) {
                    std::string nameStr(itemName);

                    // Handle "event:" items
                    if (nameStr.find("event:") == 0) {
                        std::string eventPath = nameStr.substr(6);  // Strip the "event:" prefix
                        if (playPosition >= itemStart - lookAheadTimeSeconds && playPosition <= itemEnd + tolerance) {
                            if (activeEventInstances.find(itemGUID) == activeEventInstances.end()) {
                                // Create the FMOD event instance
                                CreateFMODEventInstance(eventPath, item, itemStart, itemEnd);

                                // Apply parameters from item notes
                                FMOD::Studio::EventInstance* eventInstance = activeEventInstances[itemGUID].instance;

                                // Parse the item notes to get parameters
                                char itemNotes[4096] = "";
                                bool hasNotes = GetSetMediaItemInfo_String(item, "P_NOTES", itemNotes, false);
                                if (hasNotes && strlen(itemNotes) > 0) {
                                    // Split item notes into individual lines
                                    std::vector<std::string> noteLines = SplitString(itemNotes, "\n");
                                    for (const std::string& line : noteLines) {
                                        if (line.rfind("param:", 0) == 0) {
                                            size_t equalPos = line.find('=');
                                            if (equalPos != std::string::npos) {
                                                std::string paramName = line.substr(6, equalPos - 6); // Get parameter name
                                                std::string paramValueStr = line.substr(equalPos + 1); // Get parameter value
                                                try {
                                                    float paramValue = std::stof(paramValueStr); // Convert value to float
                                                    eventInstance->setParameterByName(paramName.c_str(), paramValue);
                                                    fmod_system->update();
                                                    DebugMsg("Updated parameter '%s' to value %.2f for event instance.\n", paramName.c_str(), paramValue);
                                                } catch (const std::exception& e) {
                                                    DebugMsg("Error parsing parameter value in item notes: %s\n", e.what());
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            // Apply envelope values to the FMOD event parameters
                            MonitorEnvelopesForEventItem(take, itemGUID);
                        } else if (playPosition > itemEnd + tolerance && activeEventInstances.find(itemGUID) != activeEventInstances.end()) {
                            ReleaseFMODEventInstance(itemGUID);
                        }
                    }

                    // Handle "param:" items
                    else if (nameStr.find("param:") == 0) {
                        // Parse "param:ParameterName=Value"
                        size_t equalPos = nameStr.find('=');
                        if (equalPos != std::string::npos) {
                            std::string paramName = nameStr.substr(6, equalPos - 6); // Get parameter name
                            std::string paramValueStr = nameStr.substr(equalPos + 1); // Get parameter value
                            float paramValue = std::stof(paramValueStr); // Convert value to float

                            // Check if playPosition is within the item's time range
                            if (playPosition >= itemStart - lookAheadTimeSeconds && playPosition <= itemEnd + tolerance) {
                                DebugMsg("Found param item: %s with value: %s\n", paramName.c_str(), paramValueStr.c_str());

                                // Check for a GUID in the item's notes
                                char itemNotes[4096];
                                bool hasNotes = GetSetMediaItemInfo_String(item, "P_NOTES", itemNotes, false);
                                if (hasNotes && strstr(itemNotes, "GUID=")) {
                                    // Extract GUID from notes
                                    std::string guidStr(itemNotes);
                                    size_t start = guidStr.find("GUID=") + 5;
                                    size_t end = guidStr.find("\n", start);
                                    std::string extractedGUID = guidStr.substr(start, end - start);

                                    // Debug message for the extracted GUID
                                    DebugMsg("Extracted GUID from item notes: %s\n", extractedGUID.c_str());

                                    // Find the specific event instance by GUID
                                    if (activeEventInstances.find(extractedGUID) != activeEventInstances.end()) {
                                        FMOD::Studio::EventInstance* instance = activeEventInstances[extractedGUID].instance;
                                        if (instance) {
                                            instance->setParameterByName(paramName.c_str(), paramValue);
                                            fmod_system->update();
                                            DebugMsg("Updated parameter '%s' to value %.2f for instance with GUID: %s\n", paramName.c_str(), paramValue, extractedGUID.c_str());
                                        }
                                    } else {
                                        DebugMsg("No active instance found for GUID: %s\n", extractedGUID.c_str());
                                    }
                                } else {
                                    // If no GUID, assume it's a global parameter
                                    fmod_system->setParameterByName(paramName.c_str(), paramValue);
                                    fmod_system->update();
                                    DebugMsg("Updated global parameter '%s' to value %.2f\n", paramName.c_str(), paramValue);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    previousPlayPosition = playPosition;
}

bool trackCacheUpdatedDuringPlayback = false; // Flag to track if the cache has been updated during playback

void ReleaseAllEventInstances() {
    DebugMsg("Releasing all active FMOD event instances.\n");

    // Iterate through activeEventInstances and safely release them
    for (auto it = activeEventInstances.begin(); it != activeEventInstances.end();) {
        if (it->second.instance != nullptr) {
            FMOD_RESULT result = it->second.instance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
            if (result == FMOD_OK) {
                DebugMsg("Successfully stopped event instance.\n");
            } else {
                DebugMsg("Failed to stop event instance, FMOD result: %d\n", result);
            }

            result = it->second.instance->release();
            if (result == FMOD_OK) {
                DebugMsg("Successfully released event instance.\n");
            } else {
                DebugMsg("Failed to release event instance, FMOD result: %d\n", result);
            }

            it = activeEventInstances.erase(it);  // Safely erase the entry and update the iterator
        } else {
            ++it;  // Move to the next item if no instance is found
        }
    }

    triggeredItems.clear();  // Clear the triggered items map
    DebugMsg("All FMOD event instances released.\n");

    fmod_system->update();  // Ensure FMOD processes all the release calls
}

void stopReleaseALLFMODEventInstances(){
    ReleaseAllEventInstances();
    StopAllEvents();
}

// Function to get the current Reaper project name without the .RPP extension
std::string GetCurrentReaperProjectName() {
    char projectFilePath[256] = {0};
    if (!EnumProjects(-1, projectFilePath, sizeof(projectFilePath))) {
        return "Untitled"; // Fallback name if the project name cannot be retrieved
    }

    std::string projectFileName = projectFilePath;

    // Extract the base name without the directory path
    size_t lastSlashPos = projectFileName.find_last_of("/\\");
    if (lastSlashPos != std::string::npos) {
        projectFileName = projectFileName.substr(lastSlashPos + 1);
    }

    // Remove the ".RPP" extension if present (Reaper project files have .RPP extension)
    size_t extensionPos = projectFileName.rfind(".RPP");
    if (extensionPos != std::string::npos) {
        projectFileName = projectFileName.substr(0, extensionPos);
    }

    DebugMsg("Currently loaded Reaper project file: %s\n", projectFileName.c_str());

    return projectFileName;
}

// Function to get the base file name without the ".ReaMOD" extension
std::string getReaMODFileName(const std::string& filePath) {
    // Find the last occurrence of a slash or backslash to get the base file name
    size_t lastSlashPos = filePath.find_last_of("/\\");
    std::string baseFileName = (lastSlashPos == std::string::npos) ? filePath : filePath.substr(lastSlashPos + 1);

    // Remove the ".ReaMOD" extension if it exists
    size_t extensionPos = baseFileName.rfind(".ReaMOD");
    if (extensionPos != std::string::npos) {
        baseFileName = baseFileName.substr(0, extensionPos);
    }

    return baseFileName;
}

void SaveStateToFile(const std::string& filePath = "") {
    std::string finalFilePath;

    // Determine the default file name
    if (filePath.empty()) {
        if (!currentDisplayedFileName.empty() && currentDisplayedFileName != "No ReaMOD session loaded.") {
            finalFilePath = currentDisplayedFileName + ".ReaMOD";
        } else {
            std::string reaperProjectName = GetCurrentReaperProjectName();
            finalFilePath = reaperProjectName + ".ReaMOD";
        }
    } else {
        finalFilePath = filePath;
    }

    // Open the file for writing
    std::ofstream outFile(finalFilePath);
    if (!outFile) {
        DebugMsg("Failed to open file for saving: %s\n", finalFilePath.c_str());
        return;
    }

    // Save relevant state information
    outFile << "fspro_file=" << selected_file_path << "\n";
    outFile << "lookahead_time_ms=" << lookAheadTimeMs << "\n";
    outFile << "move_cursor_after_insert=" << (moveCursorAfterInsert ? 1 : 0) << "\n";
    outFile << "num_frames_for_item=" << numFramesForItem << "\n";
    outFile << "item_sync_selection=" << syncSelectedEventWithItemSelection << "\n";
    outFile << "update_item_insertion_length_from_last_time_selection=" << updateItemInsertionLength << "\n";
    outFile << "show_full_bank_directory_paths=" << (showFullBankDirectoryPaths ? 1 : 0) << "\n";

    // Separate Master.bank and other bank files
    std::string master_bank_file;
    bool master_bank_loaded = false;
    std::vector<std::pair<std::string, bool>> other_banks;

    for (size_t i = 0; i < bank_files.size(); ++i) {
        if (bank_files[i].find("Master.bank") != std::string::npos) {
            master_bank_file = bank_files[i];
            master_bank_loaded = bank_load_states[i];
        } else {
            other_banks.emplace_back(bank_files[i], bank_load_states[i]);
        }
    }

    // Sort other bank files
    std::sort(other_banks.begin(), other_banks.end());

    outFile << "<bank_directories>\n";
    for (const auto& directory : customBankDirectories) {
        outFile << "bank_directory=" << directory << "\n";
    }
    outFile << "</bank_directories>\n";

    outFile << "<bank_files>\n";

    // Write Master.bank first if it exists
    if (!master_bank_file.empty()) {
        outFile << "bank_file=" << master_bank_file << "\n";
        outFile << "load_state=" << (master_bank_loaded ? 1 : 0) << "\n";
    }

    // Write other bank files
    for (const auto& bank_pair : other_banks) {
        outFile << "bank_file=" << bank_pair.first << "\n";
        outFile << "load_state=" << (bank_pair.second ? 1 : 0) << "\n";
    }

    outFile << "</bank_files>\n";

    outFile.close();
    DebugMsg("State saved successfully to: %s\n", finalFilePath.c_str());

    // Update the displayed file name
    currentDisplayedFileName = getReaMODFileName(finalFilePath);

    // Get the last modification time and update formattedLastSaveTimestamp
    std::error_code ec;
    auto ftime = fs::last_write_time(finalFilePath, ec);
    if (!ec) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now());
        std::time_t timeT = std::chrono::system_clock::to_time_t(sctp);
        char buffer[128];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&timeT));
        formattedLastSaveTimestamp = "Last Save: " + std::string(buffer);
    } else {
        DebugMsg("Error retrieving last modification time: %s\n", ec.message().c_str());
        formattedLastSaveTimestamp.clear();
    }
}

// Utility function to trim leading and trailing whitespace from a string
std::string TrimString(const std::string& str) {
    const char* whitespace = " \t\n\r";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos)
        return ""; // All whitespace

    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

void LoadStateFromFile(const std::string& filePath) {
    if (!IsFMODInitialized()) {
        DebugMsg("FMOD system is not initialized. Cannot load state from file.\n");
        return;
    }

    UnloadAllBanks();

    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        DebugMsg("Failed to open file for loading: %s\n", filePath.c_str());
        return;
    }

    bank_files.clear();
    bank_load_states.clear();
    bank_events.clear();
    loaded_banks.clear();
    masterStringEvents.clear();
    std::strncpy(selected_file_name, "No project selected.", FILE_PATH_BUFFER_SIZE - 1);

    std::string line;
    std::string fsproDirectory;
    std::unordered_map<std::string, bool> savedBankStates;
    std::vector<std::string> parsedDirectories;

    while (std::getline(inFile, line)) {
        line = TrimString(line);
        if (line.empty()) {
            continue;
        }

        if (line == "<bank_directories>") {
            while (std::getline(inFile, line)) {
                line = TrimString(line);
                if (line.empty()) {
                    continue;
                }
                if (line == "</bank_directories>") {
                    break;
                }

                size_t equalsPos = line.find('=');
                if (equalsPos == std::string::npos) {
                    DebugMsg("Error: Invalid line in bank_directories section: %s\n", line.c_str());
                    continue;
                }

                std::string key = line.substr(0, equalsPos);
                std::string value = line.substr(equalsPos + 1);

                if (key == "bank_directory") {
                    std::string normalizedDir = fs::path(value).lexically_normal().string();
                    if (std::find(parsedDirectories.begin(), parsedDirectories.end(), normalizedDir) == parsedDirectories.end()) {
                        parsedDirectories.push_back(normalizedDir);
                    }
                } else {
                    DebugMsg("Error: Unexpected key in bank_directories section: %s\n", key.c_str());
                }
            }
            continue;
        }

        if (line == "<bank_files>") {
            while (std::getline(inFile, line)) {
                line = TrimString(line);
                if (line.empty()) {
                    continue;
                }
                if (line == "</bank_files>") {
                    break;
                }

                size_t equalsPos = line.find('=');
                if (equalsPos == std::string::npos) {
                    DebugMsg("Error: Invalid line in bank_files section: %s\n", line.c_str());
                    continue;
                }

                std::string key = line.substr(0, equalsPos);
                std::string value = line.substr(equalsPos + 1);

                if (key == "bank_file") {
                    std::string bankPath = fs::path(value).lexically_normal().string();

                    std::string stateLine;
                    while (std::getline(inFile, stateLine) && TrimString(stateLine).empty()) {
                    }

                    if (inFile.eof()) {
                        DebugMsg("Error: Unexpected end of file after bank_file entry.\n");
                        break;
                    }

                    stateLine = TrimString(stateLine);
                    size_t stateEquals = stateLine.find('=');
                    if (stateEquals == std::string::npos) {
                        DebugMsg("Error: Invalid load_state entry: %s\n", stateLine.c_str());
                        continue;
                    }

                    std::string stateKey = stateLine.substr(0, stateEquals);
                    std::string stateValue = stateLine.substr(stateEquals + 1);

                    if (stateKey == "load_state") {
                        try {
                            bool loadState = (std::stoi(stateValue) != 0);
                            savedBankStates[bankPath] = loadState;
                            DebugMsg("Loaded bank_file: %s, load_state: %d\n", bankPath.c_str(), loadState);
                        } catch (const std::exception& e) {
                            DebugMsg("Error parsing load_state value: %s\n", e.what());
                        }
                    } else {
                        DebugMsg("Error: Expected load_state after bank_file entry.\n");
                    }
                } else {
                    DebugMsg("Error: Unexpected key in bank_files section: %s\n", key.c_str());
                }
            }
            continue;
        }

        size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos) {
            DebugMsg("Error: Invalid line in .ReaMOD file: %s\n", line.c_str());
            continue;
        }

        std::string key = line.substr(0, equalsPos);
        std::string value = line.substr(equalsPos + 1);

        try {
            if (key == "fspro_file") {
                std::strncpy(selected_file_path, value.c_str(), FILE_PATH_BUFFER_SIZE - 1);
                selected_file_path[FILE_PATH_BUFFER_SIZE - 1] = '\0';
                DebugMsg("Loaded fspro_file: %s\n", selected_file_path);

                std::string file_path(selected_file_path);
                size_t last_slash_pos = file_path.find_last_of("/\\");
                if (last_slash_pos != std::string::npos) {
                    std::string file_name = file_path.substr(last_slash_pos + 1);
                    std::strncpy(selected_file_name, file_name.c_str(), FILE_PATH_BUFFER_SIZE - 1);
                    selected_file_name[FILE_PATH_BUFFER_SIZE - 1] = '\0';
                    fsproDirectory = file_path.substr(0, last_slash_pos);
                } else {
                    std::strncpy(selected_file_name, file_path.c_str(), FILE_PATH_BUFFER_SIZE - 1);
                    selected_file_name[FILE_PATH_BUFFER_SIZE - 1] = '\0';
                    fsproDirectory.clear();
                }
                fmodProjectDirectory = fsproDirectory;
            } else if (key == "lookahead_time_ms") {
                lookAheadTimeMs = std::stoi(value);
                DebugMsg("Loaded lookahead_time_ms: %d\n", lookAheadTimeMs);
            } else if (key == "move_cursor_after_insert") {
                moveCursorAfterInsert = (std::stoi(value) != 0);
                DebugMsg("Loaded move_cursor_after_insert: %d\n", moveCursorAfterInsert);
            } else if (key == "num_frames_for_item") {
                numFramesForItem = std::stoi(value);
                DebugMsg("Loaded num_frames_for_item: %d\n", numFramesForItem);
            } else if (key == "item_sync_selection") {
                syncSelectedEventWithItemSelection = (std::stoi(value) != 0);
                DebugMsg("Loaded item_sync_selection: %d\n", syncSelectedEventWithItemSelection);
            } else if (key == "update_item_insertion_length_from_last_time_selection") {
                updateItemInsertionLength = (std::stoi(value) != 0);
                DebugMsg("Loaded update_item_insertion_length: %d\n", updateItemInsertionLength);
            } else if (key == "show_full_bank_directory_paths") {
                showFullBankDirectoryPaths = (std::stoi(value) != 0);
                DebugMsg("Loaded show_full_bank_directory_paths: %d\n", showFullBankDirectoryPaths);
            } else {
                DebugMsg("Error: Unknown key in .ReaMOD file: %s\n", key.c_str());
            }
        } catch (const std::exception& e) {
            DebugMsg("Error parsing value for key %s: %s\n", key.c_str(), e.what());
        }
    }

    inFile.close();

    if (parsedDirectories.empty() && !fsproDirectory.empty()) {
        fs::path defaultDir = fs::path(fsproDirectory) / "Build" / "Desktop";
        parsedDirectories.push_back(defaultDir.lexically_normal().string());
    }

    customBankDirectories = parsedDirectories;

    RefreshBankFiles();

    if (!savedBankStates.empty()) {
        for (size_t i = 0; i < bank_files.size(); ++i) {
            auto it = savedBankStates.find(bank_files[i]);
            if (it != savedBankStates.end()) {
                bank_load_states[i] = it->second;
            }
        }
    }

    SynchronizeLoadedBanks();

    DebugMsg("State loaded from file: %s\n", filePath.c_str());

    currentDisplayedFileName = getReaMODFileName(filePath);

    std::error_code ec;
    auto ftime = fs::last_write_time(filePath, ec);
    if (!ec) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now());
        std::time_t timeT = std::chrono::system_clock::to_time_t(sctp);
        char buffer[128];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&timeT));
        formattedLastSaveTimestamp = "Last Save: " + std::string(buffer);
    } else {
        DebugMsg("Error retrieving last modification time: %s\n", ec.message().c_str());
        formattedLastSaveTimestamp.clear();
    }

    RetrieveGlobalParameters();
}

void SaveStateDialog() {
    // Determine the default file name
    std::string defaultFileName;
    if (!currentDisplayedFileName.empty() && currentDisplayedFileName != "No ReaMOD session loaded." && currentDisplayedFileName != " ") {
        // If a .ReaMOD file is already loaded, use its name
        defaultFileName = currentDisplayedFileName + ".ReaMOD";
    } else {
        // Otherwise, use the currently open Reaper project name
        std::string reaperProjectName = GetCurrentReaperProjectName();
        if (reaperProjectName.empty()) {
            reaperProjectName = "Untitled"; // Fallback to "Untitled" if no Reaper project name is available
        }
        defaultFileName = reaperProjectName + ".ReaMOD";
    }

    // Set up the file dialog
    const char* filterPatterns[2] = { "*.ReaMOD", "*.*" };
    const char* savePath = tinyfd_saveFileDialog(
        "Save State As",      // Dialog title
        defaultFileName.c_str(), // Default filename
        2,                    // Number of filter patterns
        filterPatterns,       // Filter patterns array
        "ReaMOD files (*.ReaMOD)" // Filter description
    );

    if (savePath) {
        // If the file name doesn't end with .ReaMOD, add the extension
        std::string savePathStr(savePath);
        if (savePathStr.find(".ReaMOD") == std::string::npos) {
            savePathStr += ".ReaMOD";
        }
        SaveStateToFile(savePathStr);
        currentReaMODFileName = fs::path(savePathStr).filename().string(); // Update current session file name
        currentDisplayedFileName = getReaMODFileName(savePathStr); // Update the displayed file name
        DebugMsg("State saved successfully to: %s\n", savePathStr.c_str());
    } else {
        DebugMsg("Save operation canceled or invalid file name.\n");
    }
}

void LoadStateDialog() {
    const char* filterPatterns[2] = { "*.ReaMOD", "*.*" };
    const char* loadPath = tinyfd_openFileDialog(
        "Load State",         // Dialog title
        "",                   // Default path
        2,                    // Number of filter patterns
        filterPatterns,       // Filter patterns array
        "ReaMOD files (*.ReaMOD)", // Filter description
        0                     // Allow multiple selection (0 = single file)
    );

    if (loadPath) {
        LoadStateFromFile(loadPath);
        currentReaMODFileName = fs::path(loadPath).filename().string(); // Update current session file name
        DebugMsg("State loaded successfully from: %s\n", loadPath);
    } else {
        DebugMsg("Load operation canceled or invalid file name.\n");
    }
}

// Function to remove the ".bank" extension from a filename for display purposes
std::string RemoveBankExtension(const std::string& filename) {
    if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".bank") {
        return filename.substr(0, filename.size() - 5);
    }
    return filename;  // Return original if no ".bank" extension is found
}

std::string GetAbbreviatedDirectoryPath(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    std::string normalized = fs::path(path).lexically_normal().string();

    if (normalized.empty()) {
        return normalized;
    }

    std::string trimmed = normalized;
    while (!trimmed.empty() && (trimmed.back() == '/' || trimmed.back() == '\\')) {
        trimmed.pop_back();
    }

    if (trimmed.empty()) {
        return normalized;
    }

    size_t lastSeparator = trimmed.find_last_of("/\\");
    if (lastSeparator == std::string::npos) {
        return trimmed;
    }

    std::string lastComponent = trimmed.substr(lastSeparator + 1);
    if (lastComponent.empty()) {
        return normalized;
    }

    return std::string(".../") + lastComponent;
}

// Helper function to find the common prefix among a list of strings
std::string FindCommonPrefix(const std::vector<std::string>& strings) {
    if (strings.empty()) return "";

    std::string prefix = strings[0];
    for (size_t i = 1; i < strings.size(); ++i) {
        size_t j = 0;
        while (j < prefix.size() && j < strings[i].size() &&
               prefix[j] == strings[i][j]) {
            ++j;
        }
        prefix = prefix.substr(0, j);
        if (prefix.empty()) break;
    }
    return prefix;
}

int greyDark = 0x333333FF;
int blue = 0x6DD0F6FF;
int orange = 0xFFD700FF;
int supportButtonBackground = 0x282828FF;
int supportButtonHovered = 0x949494FF;
int supportButtonActive = 0x6DD0F6FF;

std::string LocateReaMODFontsDirectory() {
    static std::string cachedPath;
    static bool loggedMissingDirectory = false;

    if (!cachedPath.empty()) {
        return cachedPath;
    }

    std::vector<fs::path> candidates = {
        fs::path("fonts") / "Roboto",
        fs::path("Fonts") / "Roboto",
        fs::path("fonts") / "roboto",
        fs::path("Fonts") / "roboto"
    };

    if (GetResourcePath) {
        fs::path resourcePath(GetResourcePath());

        // Primary installation layout: <resource path>/ReaMOD/resources/fonts/Roboto
        candidates.emplace_back(resourcePath / "ReaMOD" / "resources" / "fonts" / "Roboto");
        candidates.emplace_back(resourcePath / "ReaMOD" / "resources" / "fonts" / "roboto");
    }

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (!candidate.empty() && fs::exists(candidate, ec) && fs::is_directory(candidate, ec)) {
            cachedPath = candidate.string();
            DebugMsg("Using ReaMOD font directory: %s\n", cachedPath.c_str());
            break;
        }
    }

    if (cachedPath.empty() && !loggedMissingDirectory) {
        DebugMsg("ReaMOD fonts directory not found. Expected at ReaMOD/resources/fonts/Roboto within the REAPER resource path.\n");
        loggedMissingDirectory = true;
    }

    return cachedPath;
}

ImGui_Font* LoadReaMODFont(const fs::path& fontsDir, const std::string& fileName, int size) {
    fs::path fontPath = fontsDir / fileName;
    std::error_code ec;
    if (!fs::exists(fontPath, ec) || !fs::is_regular_file(fontPath, ec)) {
        return nullptr;
    }

    try {
        ImGui_Font* font = ImGui::CreateFont(fontPath.string().c_str(), size, ImGui::FontFlags_None);
        if (font) {
            DebugMsg("Loaded ReaMOD font: %s (size %d)\n", fontPath.string().c_str(), size);
        }
        return font;
    } catch (const std::exception& e) {
        DebugMsg("Failed to load ReaMOD font %s: %s\n", fontPath.string().c_str(), e.what());
        return nullptr;
    }
}

void EnsureReaMODFontsLoaded() {
    std::string fontsDirectory = LocateReaMODFontsDirectory();
    if (fontsDirectory.empty()) {
        return;
    }

    fs::path fontsDir(fontsDirectory);

    static bool reportedRegularMissing = false;
    static bool reportedMediumMissing = false;
    static bool reportedBoldMissing = false;

    if (!reaMODRegularFont) {
        reaMODRegularFont = LoadReaMODFont(fontsDir, "Roboto-Regular.ttf", 12);
        if (!reaMODRegularFont && !reportedRegularMissing) {
            DebugMsg("Roboto-Regular.ttf could not be loaded; using ImGui's default font.\n");
            reportedRegularMissing = true;
        }
    }

    if (!reaMODMediumFont) {
        reaMODMediumFont = LoadReaMODFont(fontsDir, "Roboto-Medium.ttf", 14);
        if (!reaMODMediumFont && !reportedMediumMissing) {
            DebugMsg("Roboto-Medium.ttf could not be loaded; buttons will use the regular font.\n");
            reportedMediumMissing = true;
        }
    }

    if (!reaMODBoldFont) {
        reaMODBoldFont = LoadReaMODFont(fontsDir, "Roboto-Black.ttf", 17);
        if (!reaMODBoldFont && !reportedBoldMissing) {
            DebugMsg("Roboto-Bold.ttf could not be loaded; headings will use the regular font.\n");
            reportedBoldMissing = true;
        }
    }

    if (!reaMODMediumFont && reaMODRegularFont) {
        reaMODMediumFont = reaMODRegularFont;
    }

    if (!reaMODBoldFont && reaMODRegularFont) {
        reaMODBoldFont = reaMODRegularFont;
    }
}

void ReaMODSeparatorText(ImGui_Context* ctx, const char* label) {
    EnsureReaMODFontsLoaded();
    if (reaMODBoldFont) {
        ImGui::PushFont(ctx, reaMODBoldFont);
    }
    ImGui::SeparatorText(ctx, label);
    if (reaMODBoldFont) {
        ImGui::PopFont(ctx);
    }
}

void ReaMODText(ImGui_Context* ctx, const char* text, ImGui_Font* font) {
    if (font) {
        ImGui::PushFont(ctx, font);
    }
    ImGui::Text(ctx, text);
    if (font) {
        ImGui::PopFont(ctx);
    }
}

void PushReaMODInterfaceStyle(ImGui_Context* ctx) {
    EnsureReaMODFontsLoaded();
    ImGui::PushStyleColor(ctx, ImGui::Col_WindowBg, greyDark);
    ImGui::PushStyleColor(ctx, ImGui::Col_Button, supportButtonBackground);
    ImGui::PushStyleColor(ctx, ImGui::Col_ButtonHovered, supportButtonHovered);
    ImGui::PushStyleColor(ctx, ImGui::Col_ButtonActive, supportButtonActive);
    ImGui::PushStyleColor(ctx, ImGui::Col_FrameBg, supportButtonBackground);
    ImGui::PushStyleColor(ctx, ImGui::Col_FrameBgHovered, supportButtonHovered);
    ImGui::PushStyleColor(ctx, ImGui::Col_FrameBgActive, supportButtonActive);
    ImGui::PushStyleColor(ctx, ImGui::Col_SliderGrab, supportButtonActive);
    ImGui::PushStyleColor(ctx, ImGui::Col_SliderGrabActive, supportButtonHovered);
    ImGui::PushStyleColor(ctx, ImGui::Col_CheckMark, blue);
    ImGui::PushStyleColor(ctx, ImGui::Col_Header, supportButtonBackground);
    ImGui::PushStyleColor(ctx, ImGui::Col_HeaderHovered, supportButtonHovered);
    ImGui::PushStyleColor(ctx, ImGui::Col_HeaderActive, supportButtonActive);
    if (reaMODRegularFont) {
        ImGui::PushFont(ctx, reaMODRegularFont);
    }
}

void PopReaMODInterfaceStyle(ImGui_Context* ctx) {
    if (reaMODRegularFont) {
        ImGui::PopFont(ctx);
    }
    ImGui::PopStyleColor(ctx, 13);
}

bool StyledButton(ImGui_Context* ctx, const char* label) {
    bool fontActive = false;
    if (reaMODMediumFont) {
        ImGui::PushFont(ctx, reaMODMediumFont);
        fontActive = true;
    }
    ImGui::PushStyleColor(ctx, ImGui::Col_Text, blue);
    bool pressed = ImGui::Button(ctx, label);
    ImGui::PopStyleColor(ctx);
    if (fontActive) {
        ImGui::PopFont(ctx);
    }
    return pressed;
}

bool StyledButton(ImGui_Context* ctx, const std::string& label) {
    return StyledButton(ctx, label.c_str());
}

void RenderEventSearchWindow() {

    // Check if the search window should be open
    if (!searchFmodEventWindowOpen) {
        return;
    }

    EnsureReaMODFontsLoaded();

    // Set the initial window size
    ImGui::SetNextWindowSize(reaMOD_Main_ImGui_Context, 400, 300, ImGui::Cond_FirstUseEver);

    PushReaMODInterfaceStyle(reaMOD_Main_ImGui_Context);

    // Begin the window using the searchFmodEventWindowOpen flag
    if (ImGui::Begin(reaMOD_Main_ImGui_Context, "Event Search", &searchFmodEventWindowOpen, ImGui::WindowFlags_TopMost)) {

        // Display a label for the input field
        if (reaMODMediumFont) {
            ImGui::PushFont(reaMOD_Main_ImGui_Context, reaMODMediumFont);
        }
        ImGui::Text(reaMOD_Main_ImGui_Context, "Event Search:");
        if (reaMODMediumFont) {
            ImGui::PopFont(reaMOD_Main_ImGui_Context);
        }

        // Static buffer to hold user input
        static char eventSearchBuffer[256] = "";    

        // Static string to hold error message
        static std::string errorMessage;

        // Static variable to keep track of selected suggestion
        static int selectedIndex = -1;

        // Flag to detect if the input text has changed
        static std::string previousInputText;

        // Set focus to the input field when the window first opens
        if (ImGui::IsWindowAppearing(reaMOD_Main_ImGui_Context)) {
            ImGui::SetKeyboardFocusHere(reaMOD_Main_ImGui_Context);
        }

        // Render the InputText field with Enter key detection
        bool enterPressed = false;

        enterPressed = ImGui::InputText(reaMOD_Main_ImGui_Context, "##EventSearch", eventSearchBuffer, sizeof(eventSearchBuffer));

        // Get the current input text
        std::string inputText(eventSearchBuffer);

        // Check if the input text has changed
        bool inputTextChanged = (inputText != previousInputText);
        if (inputTextChanged) {
            // Reset selected index when input text changes
            selectedIndex = -1;
            previousInputText = inputText;
        }

        // Collect all event names from the loaded banks
        std::vector<std::string> allEventNames;
        for (const auto& bankEventPair : bank_events) {
            const std::vector<std::string>& eventsInBank = bankEventPair.second;
            allEventNames.insert(allEventNames.end(), eventsInBank.begin(), eventsInBank.end());
        }

        // Filter event names based on user input
        std::vector<std::string> filteredEventNames;
        std::string inputTextLower = ToLower(inputText);

        if (!inputTextLower.empty()) {
            for (const std::string& eventName : allEventNames) {
                std::string eventNameLower = ToLower(eventName);
                if (eventNameLower.find(inputTextLower) != std::string::npos) {
                    filteredEventNames.push_back(eventName);
                }
            }
        }

        if (ImGui::IsKeyPressed(reaMOD_Main_ImGui_Context, ImGui::Key_DownArrow) && !filteredEventNames.empty()) {
            selectedIndex = (selectedIndex + 1) % filteredEventNames.size();
        }
        if (ImGui::IsKeyPressed(reaMOD_Main_ImGui_Context, ImGui::Key_UpArrow) && !filteredEventNames.empty()) {
            selectedIndex = (selectedIndex - 1 + filteredEventNames.size()) % filteredEventNames.size();
        }
        // Handle Tab key for autocomplete
        if (ImGui::IsKeyPressed(reaMOD_Main_ImGui_Context, ImGui::Key_Tab | ImGui::Key_RightArrow)) {
            if (inputText.empty()) {
                // Do nothing if input is empty
            }
            else if (inputText == "e" || inputText == "E") {
                // Autocomplete 'e' or 'E' to 'event:/'
                std::strncpy(eventSearchBuffer, "event:/", sizeof(eventSearchBuffer));
                eventSearchBuffer[sizeof(eventSearchBuffer) - 1] = '\0'; // Ensure null-termination
                inputText = "event:/";
                previousInputText = inputText;
                selectedIndex = -1;
            }
            else if (inputText == "s" || inputText == "S") {
                // Autocomplete 's' or 'S' to 'snapshot:/'
                std::strncpy(eventSearchBuffer, "snapshot:/", sizeof(eventSearchBuffer));
                eventSearchBuffer[sizeof(eventSearchBuffer) - 1] = '\0'; // Ensure null-termination
                inputText = "snapshot:/";
                previousInputText = inputText;
                selectedIndex = -1;
            }
            else {
                // Find common prefix among filteredEventNames
                if (!filteredEventNames.empty()) {
                    std::string commonPrefix = FindCommonPrefix(filteredEventNames);
                    if (commonPrefix.size() > inputText.size()) {
                        // Autocomplete to the common prefix
                        std::strncpy(eventSearchBuffer, commonPrefix.c_str(), sizeof(eventSearchBuffer));
                        eventSearchBuffer[sizeof(eventSearchBuffer) - 1] = '\0'; // Ensure null-termination
                        inputText = commonPrefix;
                        previousInputText = inputText;
                        // Optionally, set selectedIndex to first suggestion
                        selectedIndex = 0;
                    }
                }
            }
        }

        if (ImGui::IsKeyPressed(reaMOD_Main_ImGui_Context, ImGui::Key_Enter)) {
            if (selectedIndex >= 0 && selectedIndex < filteredEventNames.size()) {
                // User selected an event using Enter key
                std::string candidateEventPath = filteredEventNames[selectedIndex];
                bool eventFound = UpdateSelectedEventParameters(candidateEventPath);
                if (eventFound) {
                    // Event found, close the window and clear the input buffer
                    searchFmodEventWindowOpen = false;
                    eventSearchBuffer[0] = '\0';
                    errorMessage.clear();
                    selectedIndex = -1;
                } else {
                    DebugMsg("Event not found: %s\n", candidateEventPath.c_str());
                    errorMessage = "Event not found: " + candidateEventPath;
                }
            } else if (!inputText.empty()) {
                // Attempt to select the event matching the input text
                std::string candidateEventPath = inputText;
                bool eventFound = UpdateSelectedEventParameters(candidateEventPath);
                if (eventFound) {
                    // Event found, close the window and clear the input buffer
                    searchFmodEventWindowOpen = false;
                    eventSearchBuffer[0] = '\0';
                    errorMessage.clear();
                    selectedIndex = -1;
                } else {
                    DebugMsg("Event not found: %s\n", candidateEventPath.c_str());
                    errorMessage = "Event not found: " + candidateEventPath;
                }
            } else {
                DebugMsg("Event Search input is empty. No event selected.\n");
                errorMessage = "Event Search input is empty. No event selected.";
            }
        }

        // After handling key presses, update the filteredEventNames again if the buffer was modified by Tab
        std::string updatedInputText(eventSearchBuffer);
        std::string updatedInputTextLower = ToLower(updatedInputText);

        // Re-filter event names based on updated input
        std::vector<std::string> updatedFilteredEventNames;
        if (!updatedInputTextLower.empty()) {
            for (const std::string& eventName : allEventNames) {
                std::string eventNameLower = ToLower(eventName);
                if (eventNameLower.find(updatedInputTextLower) != std::string::npos) {
                    updatedFilteredEventNames.push_back(eventName);
                }
            }
        }

        // Update filteredEventNames and handle selection
        if (!updatedFilteredEventNames.empty()) {
            filteredEventNames = updatedFilteredEventNames;
        }

        // Display the list of filtered events as suggestions
        if (!filteredEventNames.empty()) {
            double childWidth = 0.0f;   // Use remaining width
            double childHeight = 150.0f; // Set desired height
            bool border = true;          // Draw border

            bool childVisible = ImGui::BeginChild(reaMOD_Main_ImGui_Context, "EventSuggestionList", childWidth, childHeight, border);

            if (childVisible) {

                for (int i = 0; i < filteredEventNames.size(); ++i) {
                    const std::string& eventName = filteredEventNames[i];
                    bool isSelected = (i == selectedIndex);

                    if (ImGui::Selectable(reaMOD_Main_ImGui_Context, eventName.c_str(), &isSelected)) {
                        // User clicked on a suggestion
                        bool eventFound = UpdateSelectedEventParameters(eventName);
                        if (eventFound) {
                            // Event found, close the window and clear the input buffer
                            searchFmodEventWindowOpen = false;
                            eventSearchBuffer[0] = '\0';
                            errorMessage.clear();
                            selectedIndex = -1;
                        } else {
                            DebugMsg("Event not found: %s\n", eventName.c_str());
                            errorMessage = "Event not found: " + eventName;
                        }
                    }
                    
                    // Update selectedIndex based on user interaction
                    if (isSelected) {
                        selectedIndex = i;
                    }
                }

                ImGui::EndChild(reaMOD_Main_ImGui_Context);
            }

        } else if (!inputText.empty()) {
            // If there are no matches, inform the user
            ImGui::Text(reaMOD_Main_ImGui_Context, "No matching events found.");
        }

        // Display error message if any
        if (!errorMessage.empty()) {
            // Assuming 'orange' is defined as an ImVec4 or use individual components
            ImGui::TextColored(reaMOD_Main_ImGui_Context, orange, errorMessage.c_str());
        }

        // Handle Escape key to close the window
        if (ImGui::IsKeyPressed(reaMOD_Main_ImGui_Context, ImGui::Key_Escape)) {
            // Close the Event Search window without updating the selected event
            searchFmodEventWindowOpen = false;
            // Clear the input buffer and error message
            eventSearchBuffer[0] = '\0';
            errorMessage.clear();
            selectedIndex = -1;
        }

        // End the window
        ImGui::End(reaMOD_Main_ImGui_Context);
    }

    PopReaMODInterfaceStyle(reaMOD_Main_ImGui_Context);
}


void RenderGUI() {
    EnsureReaMODFontsLoaded();
    ImGui::SetNextWindowSize(reaMOD_Main_ImGui_Context, 700, 400, ImGui::Cond_FirstUseEver);

    PushReaMODInterfaceStyle(reaMOD_Main_ImGui_Context);

    bool open = true;  // Open flag for the window
    if (ImGui::Begin(reaMOD_Main_ImGui_Context, "ReaMOD Window", &open, ImGui::WindowFlags_NoFocusOnAppearing)) {

        // Display the formatted ReaMOD session text
        ReaMODText(reaMOD_Main_ImGui_Context, "ReaMOD Session:", reaMODBoldFont);
        ImGui::SameLine(reaMOD_Main_ImGui_Context);
        ReaMODText(reaMOD_Main_ImGui_Context, currentDisplayedFileName.c_str(), reaMODMediumFont);

        // Display the formatted last save timestamp if available
        if (!formattedLastSaveTimestamp.empty()) {
            ReaMODText(reaMOD_Main_ImGui_Context, formattedLastSaveTimestamp.c_str(), reaMODMediumFont);
        }

        // Add Save and Load State buttons
        if (StyledButton(reaMOD_Main_ImGui_Context, "Save")) {
            SaveStateDialog();
        }
        ImGui::SameLine(reaMOD_Main_ImGui_Context);
        if (StyledButton(reaMOD_Main_ImGui_Context, "Load")) {
            LoadStateDialog();
        }
        ImGui::Text(reaMOD_Main_ImGui_Context, "");

        // ImGui::Separator(reaMOD_Main_ImGui_Context);
        ReaMODSeparatorText(reaMOD_Main_ImGui_Context, "FMOD Project:");

        // Move the "Select" button to the left of the selected .fspro file
        if (StyledButton(reaMOD_Main_ImGui_Context, "Select")) {
            OpenFileDialog();
        }

        ImGui::SameLine(reaMOD_Main_ImGui_Context);  // Put the file name on the same line as the button
        ReaMODText(reaMOD_Main_ImGui_Context, selected_file_name, reaMODMediumFont);
        ImGui::Text(reaMOD_Main_ImGui_Context, "");

        ReaMODSeparatorText(reaMOD_Main_ImGui_Context, "FMOD Bank Files:");

        ReaMODText(reaMOD_Main_ImGui_Context, "Search directories:", reaMODMediumFont);
        bool directoryListChanged = false;
        if (customBankDirectories.empty()) {
            ReaMODText(reaMOD_Main_ImGui_Context, "No directories selected.", reaMODMediumFont);
        } else {
            for (size_t i = 0; i < customBankDirectories.size(); ++i) {
                const std::string& directoryPath = customBankDirectories[i];
                std::string displayPath = showFullBankDirectoryPaths ? directoryPath : GetAbbreviatedDirectoryPath(directoryPath);
                std::error_code dirError;
                bool directoryExists = fs::exists(directoryPath, dirError);
                bool isDirectory = false;
                if (!dirError && directoryExists) {
                    dirError.clear();
                    isDirectory = fs::is_directory(directoryPath, dirError);
                }
                bool isValidDirectory = directoryExists && !dirError && isDirectory;
                if (!isValidDirectory) {
                    displayPath += " (missing)";
                }

                std::string removeLabel = "Remove##BankDir" + std::to_string(i);
                std::string upLabel = "Up##BankDir" + std::to_string(i);
                std::string downLabel = "Down##BankDir" + std::to_string(i);

                if (StyledButton(reaMOD_Main_ImGui_Context, removeLabel)) {
                    customBankDirectories.erase(customBankDirectories.begin() + i);
                    directoryListChanged = true;
                    break;
                }

                ImGui::SameLine(reaMOD_Main_ImGui_Context);
                if (!isValidDirectory) {
                    ImGui::TextColored(reaMOD_Main_ImGui_Context, orange, displayPath.c_str());
                } else {
                    ReaMODText(reaMOD_Main_ImGui_Context, displayPath.c_str(), reaMODMediumFont);
                }

                if (i > 0) {
                    ImGui::SameLine(reaMOD_Main_ImGui_Context);
                    if (StyledButton(reaMOD_Main_ImGui_Context, upLabel)) {
                        std::swap(customBankDirectories[i], customBankDirectories[i - 1]);
                        directoryListChanged = true;
                        break;
                    }
                }

                if (i + 1 < customBankDirectories.size()) {
                    ImGui::SameLine(reaMOD_Main_ImGui_Context);
                    if (StyledButton(reaMOD_Main_ImGui_Context, downLabel)) {
                        std::swap(customBankDirectories[i], customBankDirectories[i + 1]);
                        directoryListChanged = true;
                        break;
                    }
                }
            }
        }

        if (directoryListChanged) {
            RefreshBankFiles();
        }

        if (StyledButton(reaMOD_Main_ImGui_Context, "Add directory...")) {
            const char* defaultPath = nullptr;
            if (!customBankDirectories.empty()) {
                defaultPath = customBankDirectories.back().c_str();
            } else if (!fmodProjectDirectory.empty()) {
                defaultPath = fmodProjectDirectory.c_str();
            }

            const char* selectedPath = tinyfd_selectFolderDialog("Select FMOD bank directory", defaultPath);
            if (selectedPath) {
                std::string normalizedPath = fs::path(selectedPath).lexically_normal().string();
                if (std::find(customBankDirectories.begin(), customBankDirectories.end(), normalizedPath) == customBankDirectories.end()) {
                    customBankDirectories.push_back(normalizedPath);
                    RefreshBankFiles();
                }
            }
        }
        ImGui::SameLine(reaMOD_Main_ImGui_Context);
        if (StyledButton(reaMOD_Main_ImGui_Context, "Rescan")) {
            RefreshBankFiles();
        }

        ImGui::Text(reaMOD_Main_ImGui_Context, "");

        if (!bank_files.empty()) {
            for (size_t i = 0; i < bank_files.size(); ++i) {
                std::string bank_file_name = RemoveBankExtension(fs::path(bank_files[i]).filename().string());
                std::string button_label = bank_load_states[i] ? "Unload##" + std::to_string(i) : "Load##" + std::to_string(i);

                if (StyledButton(reaMOD_Main_ImGui_Context, button_label)) {
                    if (bank_load_states[i]) {
                        auto loadedIt = loaded_banks.find(bank_files[i]);
                        if (loadedIt != loaded_banks.end() && loadedIt->second) {
                            loadedIt->second->unload();
                            loaded_banks.erase(loadedIt);
                        }
                        bank_events.erase(bank_files[i]);
                        fmod_system->update();
                    } else {
                        LoadBank(bank_files[i]);
                    }
                    bank_load_states[i] = !bank_load_states[i];
                }

                ImGui::SameLine(reaMOD_Main_ImGui_Context);

                if (ImGui::TreeNode(reaMOD_Main_ImGui_Context, bank_file_name.c_str())) {
                    if (bank_load_states[i]) {
                        if (bank_events.find(bank_files[i]) != bank_events.end()) {
                            const std::vector<std::string>& events = bank_events[bank_files[i]];
                            auto grouped_folders = GroupEventsAndSnapshotsByPath(events);

                            for (const auto& folder_type : grouped_folders) {
                                if (ImGui::TreeNode(reaMOD_Main_ImGui_Context, folder_type.first.c_str())) {
                                    auto top_level_folder = folder_type.second.find("");
                                    if (top_level_folder != folder_type.second.end()) {
                                        for (const auto& event_pair : top_level_folder->second) {
                                            std::string event_label = "Play##" + event_pair.first;
                                            RenderPlayButton(reaMOD_Main_ImGui_Context, event_label, event_pair.second);

                                            ImGui::SameLine(reaMOD_Main_ImGui_Context);

                                            bool isSelected = (selectedFMODEvent == event_pair.second);

                                            if (ImGui::Selectable(reaMOD_Main_ImGui_Context, event_pair.first.c_str(), &isSelected)) {
                                                selectedFMODEvent = event_pair.second;
                                                UpdateSelectedEventParameters(selectedFMODEvent);
                                                DebugMsg("FMOD event selected: %s\n", selectedFMODEvent.c_str());
                                            }
                                        }
                                    }

                                    for (const auto& folder : folder_type.second) {
                                        if (folder.first.empty()) {
                                            continue;
                                        }
                                        if (ImGui::TreeNode(reaMOD_Main_ImGui_Context, folder.first.c_str())) {
                                            for (const auto& event_pair : folder.second) {
                                                std::string event_label = "Play##" + event_pair.second;
                                                RenderPlayButton(reaMOD_Main_ImGui_Context, event_label, event_pair.second);

                                                ImGui::SameLine(reaMOD_Main_ImGui_Context);

                                                bool isSelected = (selectedFMODEvent == event_pair.second);

                                                if (ImGui::Selectable(reaMOD_Main_ImGui_Context, event_pair.first.c_str(), &isSelected)) {
                                                    selectedFMODEvent = event_pair.second;
                                                    UpdateSelectedEventParameters(selectedFMODEvent);
                                                    DebugMsg("FMOD event selected: %s\n", selectedFMODEvent.c_str());
                                                }
                                            }
                                            ImGui::TreePop(reaMOD_Main_ImGui_Context);
                                        }
                                    }
                                    ImGui::TreePop(reaMOD_Main_ImGui_Context);
                                }
                            }
                        }
                    }
                    ImGui::TreePop(reaMOD_Main_ImGui_Context);
                }
            }
        } else {
            ReaMODText(reaMOD_Main_ImGui_Context, "No .bank files found in the selected directories.", reaMODMediumFont);
        }

        ImGui::Text(reaMOD_Main_ImGui_Context, "");
    

        // Render Global Parameters Section if loaded FMOD Project has global paramters
        RetrieveGlobalParameters();
    
        if (!groupedGlobalParameters.empty()) {

            // Global Parameters Section
            ReaMODSeparatorText(reaMOD_Main_ImGui_Context, "Global Parameters:");
            
            // Separate "No Prefix" group from others
            std::map<std::string, std::vector<GlobalParameter>> otherGroups;
            std::vector<GlobalParameter> noPrefixParameters;
    
            for (const auto& group : groupedGlobalParameters) {
                if (group.first == "No Prefix") {
                    noPrefixParameters = group.second;
                } else {
                    otherGroups[group.first] = group.second;
                }
            }
    
            // Loop over each group except "No Prefix"
            for (auto& group : otherGroups) {
                const std::string& groupName = group.first;
                std::vector<GlobalParameter>& parameters = group.second;
    
                // Display the group name as a collapsible tree node
                if (ImGui::TreeNode(reaMOD_Main_ImGui_Context, groupName.c_str())) {
    
                    // Loop over parameters in the group
                    for (auto& globalParam : parameters) {
                        float previousValue = globalParam.currentValue;
    
                        double doubleValue = static_cast<double>(globalParam.currentValue);
                        double doubleMin = static_cast<double>(globalParam.minValue);
                        double doubleMax = static_cast<double>(globalParam.maxValue);
    
                        // Use SliderDouble to create a slider for the parameter
                        ImGui::SliderDouble(reaMOD_Main_ImGui_Context, globalParam.name.c_str(), &doubleValue, doubleMin, doubleMax);
    
                        // Update the currentValue with the new value from the slider
                        globalParam.currentValue = static_cast<float>(doubleValue);
    
                        // If the value has changed, update the global parameter in FMOD
                        if (previousValue != globalParam.currentValue) {
                            fmod_system->setParameterByName(globalParam.name.c_str(), globalParam.currentValue);
                            fmod_system->update();
                        }
                    }
    
                    ImGui::TreePop(reaMOD_Main_ImGui_Context); // End the group tree node
                }
            }
    
            // Now display "No Prefix" parameters within a collapsible folder
            if (!noPrefixParameters.empty()) {
                if (ImGui::TreeNode(reaMOD_Main_ImGui_Context, "Uncategorized")) {
    
                    // Loop over parameters in "No Prefix" group
                    for (auto& globalParam : noPrefixParameters) {
                        float previousValue = globalParam.currentValue;
    
                        double doubleValue = static_cast<double>(globalParam.currentValue);
                        double doubleMin = static_cast<double>(globalParam.minValue);
                        double doubleMax = static_cast<double>(globalParam.maxValue);
    
                        // Use SliderDouble to create a slider for the parameter
                        ImGui::SliderDouble(reaMOD_Main_ImGui_Context, globalParam.name.c_str(), &doubleValue, doubleMin, doubleMax);
    
                        // Update the currentValue with the new value from the slider
                        globalParam.currentValue = static_cast<float>(doubleValue);
    
                        // If the value has changed, update the global parameter in FMOD
                        if (previousValue != globalParam.currentValue) {
                            fmod_system->setParameterByName(globalParam.name.c_str(), globalParam.currentValue);
                            fmod_system->update();
                        }
                    }
    
                    ImGui::TreePop(reaMOD_Main_ImGui_Context); // End the "Uncategorized" tree node
                }
            }
            ImGui::Text(reaMOD_Main_ImGui_Context, "");
        }



        // Render Section for selected event and parameters if event is selected
        if (!selectedFMODEvent.empty()) {
            // ImGui::Separator(reaMOD_Main_ImGui_Context);
            ReaMODSeparatorText(reaMOD_Main_ImGui_Context, "Selected Event:");
        
            // Render the play button
            std::string play_button_label = "Play##SelectedEvent";
            RenderPlayButton(reaMOD_Main_ImGui_Context, play_button_label, selectedFMODEvent);
            ImGui::SameLine(reaMOD_Main_ImGui_Context);
            // Display the selected event name
            ImGui::Text(reaMOD_Main_ImGui_Context, selectedFMODEvent.c_str());
        
            // Get the event instance for the selected event
            FMOD::Studio::EventInstance* eventInstance = nullptr;
    
            // Check if the selected event is being played via the play button
            auto it = playButtonEventInstances.find(selectedFMODEvent);
            if (it != playButtonEventInstances.end()) {
                eventInstance = it->second;
            }
    
            // If sync with selected item is enabled and an item is selected, get the event instance from activeEventInstances
            if (syncSelectedEventWithItemSelection && lastSelectedItem != nullptr) {
                std::string itemGUID = GetItemGUID(lastSelectedItem);
                auto instanceIt = activeEventInstances.find(itemGUID);
                if (instanceIt != activeEventInstances.end()) {
                    eventInstance = instanceIt->second.instance;
                }
            }
    
            // Flag to detect if any slider is active
            bool anySliderActive = false;
    
            // Display sliders for the event's parameters using SliderDouble
            for (auto& param : selectedEventParameters) {
                float previousValue = param.currentValue;
    
                double doubleValue = static_cast<double>(param.currentValue);
                double doubleMin = static_cast<double>(param.minValue);
                double doubleMax = static_cast<double>(param.maxValue);
    
                // Use SliderDouble to create a slider for the parameter
                ImGui::SliderDouble(reaMOD_Main_ImGui_Context, param.name.c_str(), &doubleValue, doubleMin, doubleMax);
    
                if (ImGui::IsItemActive(reaMOD_Main_ImGui_Context)) {
                    anySliderActive = true;
                }
    
                // Update the currentValue with the new value from the slider
                param.currentValue = static_cast<float>(doubleValue);
    
                // If the value has changed and the slider is active, update the FMOD event instance
                if (previousValue != param.currentValue && anySliderActive) {
                    if (eventInstance) {
                        eventInstance->setParameterByName(param.name.c_str(), param.currentValue);
                        fmod_system->update();
                    }
    
                    // Update the cached parameter value
                    auto& cachedParams = eventParameterCache[selectedFMODEvent];
                    for (auto& cachedParam : cachedParams) {
                        if (cachedParam.name == param.name) {
                            cachedParam.currentValue = param.currentValue;
                            break;
                        }
                    }
                }
            }
    
            // After the loop, if no sliders are active and REAPER is playing, update parameter values from event instance
            if (!anySliderActive && (GetPlayState() & 1)) {
                if (eventInstance) {
                    for (auto& param : selectedEventParameters) {
                        float value = 0.0f;
                        FMOD_RESULT result = eventInstance->getParameterByName(param.name.c_str(), &value);
                        if (result == FMOD_OK) {
                            param.currentValue = value;
                        }
                    }
                    fmod_system->update();
                }
            }
    
            ImGui::Checkbox(reaMOD_Main_ImGui_Context, "Sync with selected item", &syncSelectedEventWithItemSelection);
            ImGui::Text(reaMOD_Main_ImGui_Context, "");
        }
        

        
        // Render Selected Media Item's notes if an item is selected.
        // Retrieve the currently selected media item
        MediaItem* selectedItem = GetSelectedMediaItem(nullptr, 0);
        static char itemNotes[4096] = "";  // Static buffer to persist across frames
        static MediaItem* lastSelectedItem = nullptr;  // To track if the selected item has changed
        
        if (selectedItem) {
            // New section to display and edit the notes of the selected media item
            ReaMODSeparatorText(reaMOD_Main_ImGui_Context, "Selected Media Item Notes:");
            // If the selected item has changed, load the notes
            if (selectedItem != lastSelectedItem) {
                GetSetMediaItemInfo_String(selectedItem, "P_NOTES", itemNotes, false);
                lastSelectedItem = selectedItem;  // Update last selected item
            }
        
            // Display the current notes in a multi-line input field
            ImGui::InputTextMultiline(reaMOD_Main_ImGui_Context, "##ItemNotes", itemNotes, sizeof(itemNotes), ImGui::InputTextFlags_CtrlEnterForNewLine);
        
            // Check if the multiline is active and "Return" key is pressed
            if (ImGui::IsItemActive(reaMOD_Main_ImGui_Context) && ImGui::IsKeyPressed(reaMOD_Main_ImGui_Context, ImGui::Key_Enter)) {
                GetSetMediaItemInfo_String(selectedItem, "P_NOTES", itemNotes, true);  // Save the updated notes
                DebugMsg("Updated notes for selected media item: %s (via Return key)\n", itemNotes);
            }
        
            // Provide a button to save the notes back to the media item
            if (StyledButton(reaMOD_Main_ImGui_Context, "Save Notes")) {
                GetSetMediaItemInfo_String(selectedItem, "P_NOTES", itemNotes, true);  // Save the updated notes
                DebugMsg("Updated notes for selected media item: %s\n", itemNotes);
            }
            ImGui::Text(reaMOD_Main_ImGui_Context, "");
        } else {
            lastSelectedItem = nullptr;  // Reset if no item is selected
        }




        // ImGui::Separator(reaMOD_Main_ImGui_Context);
        ReaMODSeparatorText(reaMOD_Main_ImGui_Context, "Settings:");

        // Add the InputInt control for Look Ahead Time and keep the text on the same line
        ImGui::SetNextItemWidth(reaMOD_Main_ImGui_Context, 90);
        ImGui::InputInt(reaMOD_Main_ImGui_Context, "##look_ahead_time_ms", &lookAheadTimeMs);
        ImGui::SameLine(reaMOD_Main_ImGui_Context);
        ImGui::Text(reaMOD_Main_ImGui_Context, "Event detection look ahead time (ms)");

        // Add the InputInt control for number of frames
        ImGui::SetNextItemWidth(reaMOD_Main_ImGui_Context, 90);
        ImGui::InputInt(reaMOD_Main_ImGui_Context, "##num_frames_for_item", &numFramesForItem);
        ImGui::SameLine(reaMOD_Main_ImGui_Context);
        ImGui::Text(reaMOD_Main_ImGui_Context, "Number of frames for inserted item");

        // Add the checkbox for moving the cursor after inserting an item
        ImGui::Checkbox(reaMOD_Main_ImGui_Context, "Move edit to end of inserted item.", &moveCursorAfterInsert);
        ImGui::Checkbox(reaMOD_Main_ImGui_Context, "Update item length from last time-selection insert.", &updateItemInsertionLength);

        // Add the checkbox for toggling display of full directory path.
        ImGui::Checkbox(reaMOD_Main_ImGui_Context, "Show full bank directory paths", &showFullBankDirectoryPaths);
        
        // Add the checkbox for toggling debug messages in Reaper
        ImGui::Checkbox(reaMOD_Main_ImGui_Context, "Enable debug messages to be posted to Reaper console", &debugMessages);

        // Support & Links section
        ImGui::Separator(reaMOD_Main_ImGui_Context);
        ReaMODSeparatorText(reaMOD_Main_ImGui_Context, "Support & Links");

        if (StyledButton(reaMOD_Main_ImGui_Context, "Support Developer")) {
            if (!OpenURLInDefaultBrowser("https://www.buymeacoffee.com/danielrdehaan")) {
                DebugMsg("Failed to open support URL.\n");
            }
        }

        ImGui::Spacing(reaMOD_Main_ImGui_Context);

        if (StyledButton(reaMOD_Main_ImGui_Context, "Discord Server")) {
            if (!OpenURLInDefaultBrowser("https://discord.gg/C9FYD8Qf4g")) {
                DebugMsg("Failed to open Discord URL.\n");
            }
        }

        ImGui::Spacing(reaMOD_Main_ImGui_Context);

        if (StyledButton(reaMOD_Main_ImGui_Context, "Developer Website")) {
            if (!OpenURLInDefaultBrowser("https://www.simplesoundtools.com")) {
                DebugMsg("Failed to open website URL.\n");
            }
        }

        ImGui::End(reaMOD_Main_ImGui_Context);
    }

    // Pop the style colors
    PopReaMODInterfaceStyle(reaMOD_Main_ImGui_Context);

    RenderEventSearchWindow();

    if (!open) {
        reaMOD_Main_ImGui_Context = nullptr;
    }
}

void UpdateEventPlayStates() {
    bool stateChanged = false;

    // Iterate through the event instances and update their play state
    for (auto& [eventPath, eventInstance] : playButtonEventInstances) {
        FMOD_STUDIO_PLAYBACK_STATE playbackState;
        FMOD_RESULT result = eventInstance->getPlaybackState(&playbackState);

        if (result != FMOD_OK) {
            DebugMsg("Failed to get playback state for event: %s\n", eventPath.c_str());
            continue; // Skip if the state could not be retrieved
        }

        DebugMsg("Playback state for event %s is %d\n", eventPath.c_str(), playbackState);

        // Check if the playback has stopped
        if (playbackState == 2 && buttonStates[eventPath]) {
            DebugMsg("FMOD event triggered by play button has stopped playing.\n");
            buttonStates[eventPath] = false;
            stateChanged = true;
        } 
        // Check if playback is starting/playing and button state is false
        else if ((playbackState == FMOD_STUDIO_PLAYBACK_PLAYING || playbackState == FMOD_STUDIO_PLAYBACK_STARTING) && !buttonStates[eventPath]) {
            DebugMsg("FMOD event is playing but button state is not active.\n");
            buttonStates[eventPath] = true; // Sync button state with actual playback
            stateChanged = true;
        }
    }

    // If any state has changed, trigger a GUI refresh
    if (stateChanged) {
        DebugMsg("FMOD Play Button Event State changed. Refreshing GUI...\n");
        RenderGUI(); // Force the GUI to refresh
    }
}

void MonitorPlayback() {
    if (!IsFMODInitialized()) {
        DebugMsg("FMOD system is not initialized.\n");
        DebugMsg("Attempting to re-initialize FMOD...\n");
        InitializeFMOD();
        return;
    }

    if (GetPlayState == nullptr || GetPlayPosition == nullptr) {
        DebugMsg("Playback state functions are not available.\n");
        return;
    }

    int playState = GetPlayState();      // Get current playback state
    double playPosition = GetPlayPosition();  // Get current play position

    // Check if REAPER is playing or recording
    if (playState & 1) {  // REAPER is playing
        DebugMsg("Playback running. Current position: %.2f\n", playPosition);

        // If playback has just started
        if (!(previousPlayState & 1)) {
            // Playback has just started
            stopReleaseALLFMODEventInstances();  // Call the function once when playback begins
            DebugMsg("Playback started. Stopped and released all FMOD event instances.\n");

            // Reset any necessary state variables
            // triggeredMarkers.clear();            // Clear triggered markers
            triggeredItems.clear();              // Clear triggered items
            activeEventInstances.clear();        // Clear active event instances
            // trackCacheUpdatedDuringPlayback = false; // Reset track cache flag
        }

        // If this is the first time during this playback session, update the track cache
        // if (!trackCacheUpdatedDuringPlayback) {
        //     UpdateTrackCache();                  // Refresh the track cache once when playback starts
        //     trackCacheUpdatedDuringPlayback = true;  // Set flag to indicate cache has been updated
        // }

        // If playhead moved backward (looping, scrubbing, or jump)
        if (playPosition < previousPlayPosition) {
            DebugMsg("Playhead moved backward. Resetting triggered markers.\n");
            // triggeredMarkers.clear();            // Clear all triggered markers to allow retriggering
            triggeredItems.clear();              // Clear all triggered items to allow retriggering
        }

        // Update previous play position
        previousPlayPosition = playPosition;

        // Check markers and trigger FMOD events based on marker positions
        // CheckMarkers(playPosition);

        // Check items on tracks named "FMOD" or "fmod" for event or snapshot notes
        CheckItems(playPosition);

    } else if (previousPlayState & 1) {  // REAPER was playing but now stopped
        ReleaseAllEventInstances();       // Release all unreleased FMOD event instances
        DebugMsg("Playback stopped. Cleaning up any lingering state.\n");
        // Optionally reset state variables here if needed
    }

    // Update previous play state to track state changes
    previousPlayState = playState;
    UpdateEventPlayStates();
}

void MonitorItemSelection() {
    // DebugMsg("MonitorItemSelection called.\n");

    if (!syncSelectedEventWithItemSelection) {
        DebugMsg("Sync with selected item is disabled.\n");
        lastSelectedItem = nullptr; // Reset if not syncing
        return;
    }

    int numSelectedItems = CountSelectedMediaItems(nullptr);
    // DebugMsg("Number of selected items: %d\n", numSelectedItems);

    if (numSelectedItems != 1) {
        // DebugMsg("Not exactly one item is selected. Resetting lastSelectedItem.\n");
        lastSelectedItem = nullptr; // Reset if not exactly one item is selected
        return;
    }

    MediaItem* selectedItem = GetSelectedMediaItem(nullptr, 0);

    // Get the active take
    MediaItem_Take* take = GetActiveTake(selectedItem);
    if (!take) {
        DebugMsg("Selected item has no active take. Exiting.\n");
        lastSelectedItem = nullptr; // Reset lastSelectedItem
        return;
    }

    // Get the take name
    char takeName[512] = "";
    GetSetMediaItemTakeInfo_String(take, "P_NAME", takeName, false);
    std::string takeNameStr(takeName);

    if (takeNameStr.find("event:") == 0) {
        if (selectedItem != lastSelectedItem) {
            DebugMsg("Selected item has changed.\n");
            lastSelectedItem = selectedItem; // Update the last selected item

            DebugMsg("Take name starts with 'event:'. Setting selected FMOD event.\n");

            // Set the selected event
            selectedFMODEvent = takeNameStr;
            UpdateSelectedEventParameters(selectedFMODEvent);

            // Parse item notes for parameter values
            char itemNotes[4096] = "";
            bool hasNotes = GetSetMediaItemInfo_String(selectedItem, "P_NOTES", itemNotes, false);

            if (hasNotes && strlen(itemNotes) > 0) {
                DebugMsg("Item has notes: %s\n", itemNotes);

                std::vector<std::string> noteLines = SplitString(itemNotes, "\n");
                for (const std::string& line : noteLines) {
                    DebugMsg("Processing note line: %s\n", line.c_str());

                    if (line.rfind("param:", 0) == 0) {
                        size_t equalPos = line.find('=');
                        if (equalPos != std::string::npos) {
                            std::string paramName = line.substr(6, equalPos - 6); // Get parameter name
                            std::string paramValueStr = line.substr(equalPos + 1); // Get parameter value
                            DebugMsg("Found parameter: %s with value: %s\n", paramName.c_str(), paramValueStr.c_str());

                            try {
                                float paramValue = std::stof(paramValueStr); // Convert value to float
                                bool paramFound = false;

                                // Update the matching parameter in selectedEventParameters
                                for (auto& param : selectedEventParameters) {
                                    if (param.name == paramName) {
                                        DebugMsg("Updating parameter '%s' currentValue to %f\n", paramName.c_str(), paramValue);
                                        param.currentValue = paramValue;

                                        // Also update the cached parameter value
                                        auto& cachedParams = eventParameterCache[selectedFMODEvent];
                                        for (auto& cachedParam : cachedParams) {
                                            if (cachedParam.name == paramName) {
                                                cachedParam.currentValue = paramValue;
                                                DebugMsg("Updated cached parameter '%s' currentValue to %f\n", paramName.c_str(), paramValue);
                                                break;
                                            }
                                        }
                                        paramFound = true;
                                        break;
                                    }
                                }

                                if (!paramFound) {
                                    DebugMsg("Parameter '%s' not found in selectedEventParameters.\n", paramName.c_str());
                                }
                            } catch (const std::exception& e) {
                                DebugMsg("Error parsing parameter value in item notes: %s\n", e.what());
                            }
                        } else {
                            DebugMsg("No '=' found in parameter line: %s\n", line.c_str());
                        }
                    } else {
                        DebugMsg("Line does not start with 'param:': %s\n", line.c_str());
                    }
                }
            } else {
                DebugMsg("Item has no notes or failed to retrieve notes.\n");
            }
        } else {
            // Selected item hasn't changed, but we might need to update parameters
            // DebugMsg("Selected item has not changed.\n");

            // If REAPER is playing, update parameters from event instance
            if (GetPlayState() & 1) { // If REAPER is playing
                // Retrieve current parameter values from the active event instance
                std::string itemGUID = GetItemGUID(selectedItem);
                auto it = activeEventInstances.find(itemGUID);
                if (it != activeEventInstances.end()) {
                    FMOD::Studio::EventInstance* eventInstance = it->second.instance;
                    if (eventInstance) {
                        for (auto& param : selectedEventParameters) {
                            float value = 0.0f;
                            FMOD_RESULT result = eventInstance->getParameterByName(param.name.c_str(), &value);
                            if (result == FMOD_OK) {
                                param.currentValue = value;
                            } else {
                                DebugMsg("Failed to get parameter '%s' value from event instance.\n", param.name.c_str());
                            }
                        }
                        fmod_system->update();
                    }
                } else {
                    DebugMsg("No active event instance found for GUID: %s\n", itemGUID.c_str());
                }
            }
        }
    } else {
        DebugMsg("Active take name does not start with 'event:'.\n");
        lastSelectedItem = nullptr; // Reset lastSelectedItem
    }
}

// Add task and return its ID
int AddTask(std::function<void()> task) {
    int taskId = nextTaskId++;
    taskMap[taskId] = task;
    return taskId;
}

// Remove a task by ID
void RemoveTask(int taskId) {
    taskMap.erase(taskId);
}

// Timer function
void OnTimer() {
    for (auto& [taskId, task] : taskMap) {
        task();
    }
}

void AutoLoadReaMODFile() {
    std::string reaperProjectName = GetCurrentReaperProjectName();
    if (!reaperProjectName.empty() && reaperProjectName != "Untitled") {
        char projectFilePath[256] = {0};
        if (EnumProjects(-1, projectFilePath, sizeof(projectFilePath))) {
            // Form the expected .ReaMOD file path
            std::string projectDirectory = fs::path(projectFilePath).parent_path().string();
            std::string reaMODFilePath = projectDirectory + "/" + reaperProjectName + ".ReaMOD";

            // Check if the .ReaMOD file exists
            if (fs::exists(reaMODFilePath)) {
                DebugMsg("Auto-loading ReaMOD file: %s\n", reaMODFilePath.c_str());
                LoadStateFromFile(reaMODFilePath);
                currentReaMODFileName = fs::path(reaMODFilePath).filename().string(); // Update current session file name
                currentDisplayedFileName = getReaMODFileName(reaMODFilePath); // Update the displayed file name
            } else {
                DebugMsg("No matching ReaMOD file found for project: %s\n", reaMODFilePath.c_str());
            }
        }
    }
}

void toggleReaMODWindow() {
    if (!reaMOD_Main_ImGui_Context) {
        // First-time setup: initialize ReaImGui and FMOD, and start rendering
        ImGui::init(plugin_getapi);
        reaMOD_Main_ImGui_Context = ImGui::CreateContext("ReaMOD Window");

        // Initialize FMOD only if it's not already initialized
        if (!IsFMODInitialized()) {
            InitializeFMOD();
            playbackTaskId = AddTask(MonitorPlayback);
        }

        // Add tasks to monitor and render the GUI
        guiTaskId = AddTask(RenderGUI);
        itemSelectionTaskId = AddTask(MonitorItemSelection);

        // Auto-load the .ReaMOD file if available
        if (reaModWindowPreviouslyOpen != true) {
            AutoLoadReaMODFile();
            reaModWindowPreviouslyOpen = true;
        }
    } else {
        // Clean up: remove tasks and close the window
        if (guiTaskId != -1) {
            RemoveTask(guiTaskId);
            guiTaskId = -1;
        }
        if (itemSelectionTaskId != -1) {
            RemoveTask(itemSelectionTaskId);
            itemSelectionTaskId = -1;
        }
        if (playbackTaskId != -1){
            RemoveTask(playbackTaskId);
            playbackTaskId = -1;
        }

        // Nullify the ImGui context to signify the window is closed
        reaMOD_Main_ImGui_Context = nullptr;
    }
}

void openFmodEventSearchWindow()
{
    searchFmodEventWindowOpen = !searchFmodEventWindowOpen;
}

// Function to check if a directory exists
bool DirectoryExists(const std::string& directoryPath) {
    return fs::exists(directoryPath) && fs::is_directory(directoryPath);
}

// Function to create a directory if it doesn't exist (renamed to avoid macro conflict)
bool CreateDirectoryIfNotExists(const std::string& directoryPath) {
    if (!DirectoryExists(directoryPath)) {
        try {
            return fs::create_directories(directoryPath); // Creates the directory and any parent directories if needed
        } catch (const fs::filesystem_error& e) {
            ShowConsoleMsg(("Failed to create directory: " + std::string(e.what()) + "\n").c_str());
            return false;
        }
    }
    return true; // Directory already exists
}

void InsertEventParameterJSFXForSelectedMediaItem() {
    DebugMsg("Starting InsertEventParameterJSFXForSelectedMediaItem...\n");

    // Get the currently selected media item
    MediaItem* selectedItem = GetSelectedMediaItem(nullptr, 0);
    if (!selectedItem) {
        DebugMsg("No media item is selected.\n");
        return;
    }

    // Get the active take of the selected media item
    MediaItem_Take* activeTake = GetActiveTake(selectedItem);
    if (!activeTake) {
        DebugMsg("Selected item has no active take.\n");
        return;
    }

    // Get the name of the active take
    char takeName[512];
    GetSetMediaItemTakeInfo_String(activeTake, "P_NAME", takeName, false);
    DebugMsg("Active take name: %s\n", takeName);

    // Strip "event:" prefix and check if the take name matches the selected FMOD event
    std::string fmodEventName = selectedFMODEvent;
    DebugMsg("Original FMOD event name: %s\n", fmodEventName.c_str());

    if (fmodEventName != takeName) {
        DebugMsg("Take name does not match the selected FMOD event. Aborting.\n");
        return;
    }

    // Strip "event:" prefix from the FMOD event path
    fmodEventName = StripPathPrefix(selectedFMODEvent);
    DebugMsg("Stripped FMOD event name: %s\n", fmodEventName.c_str());

    // Construct the JSFX filename based on the project and event path, without "event:"
    std::string projectName = selected_file_name;
    projectName = projectName.substr(0, projectName.rfind(".fspro"));  // Remove ".fspro" extension
    DebugMsg("Project name: %s\n", projectName.c_str());

    // Construct the JSFX filename based on the project and event path, without "event:"
    std::string jsfxName = projectName + "_" + fmodEventName;
    
    // Step 1: Remove the first "/"
    size_t firstSlashPos = jsfxName.find('/');
    if (firstSlashPos != std::string::npos) {
        jsfxName.erase(firstSlashPos, 1);  // Remove the first "/"
    }
    
    // Step 2: Replace all subsequent "/" with "-"
    std::replace(jsfxName.begin(), jsfxName.end(), '/', '-');
    
    // Step 3: Remove any ":" to make it valid for Reaper
    jsfxName.erase(std::remove(jsfxName.begin(), jsfxName.end(), ':'), jsfxName.end());
    
    DebugMsg("Final JSFX name (after removing invalid characters): %s\n", jsfxName.c_str());


    // Create the "ReaMOD" directory if it doesn't exist
    std::string effectsPath = std::string(GetResourcePath()) + "/Effects/ReaMOD";
    DebugMsg("Effects path: %s\n", effectsPath.c_str());

    if (!DirectoryExists(effectsPath.c_str())) {
        DebugMsg("Directory does not exist. Creating: %s\n", effectsPath.c_str());
        if (!CreateDirectoryIfNotExists(effectsPath.c_str())) {
            DebugMsg("Failed to create directory: %s\n", effectsPath.c_str());
            return;
        }
    }

    // Full path to the new JSFX file
    std::string jsfxFilePath = effectsPath + "/" + jsfxName + ".jsfx";
    DebugMsg("JSFX file path: %s\n", jsfxFilePath.c_str());

    // Build JSFX content from FMOD parameters with sequential slider indices
    std::string jsfxContent = "desc: " + jsfxName + "\n";
    int sliderIndex = 1;  // Start slider indices from 1
    for (const auto& param : selectedEventParameters) {
        jsfxContent += "slider" + std::to_string(sliderIndex++) + ": " +
                       std::to_string(param.defaultValue) + "<" +
                       std::to_string(param.minValue) + "," +
                       std::to_string(param.maxValue) + "> " + param.name + "\n";
        DebugMsg("Added parameter to JSFX: %s (default: %f, min: %f, max: %f)\n", param.name.c_str(), param.defaultValue, param.minValue, param.maxValue);
    }
    jsfxContent += "@sample\n";

    // Write the JSFX content to the file
    DebugMsg("Writing JSFX content to file...\n");
    FILE* jsfxFile = fopen(jsfxFilePath.c_str(), "w");
    if (jsfxFile) {
        fwrite(jsfxContent.c_str(), 1, jsfxContent.size(), jsfxFile);
        fclose(jsfxFile);
        DebugMsg("JSFX file successfully created.\n");
    } else {
        DebugMsg("Failed to create JSFX file at path: %s\n", jsfxFilePath.c_str());
        return;
    }

    // Insert the JSFX as a take effect on the selected media item (using TakeFX_AddByName)
    DebugMsg("Inserting JSFX as take effect...\n");
    int fxIndex = TakeFX_AddByName(activeTake, jsfxName.c_str(), true);

    if (fxIndex >= 0) {
        DebugMsg("JSFX %s inserted successfully on take, FX index: %d.\n", jsfxName.c_str(), fxIndex);
        UpdateArrange();  // Force update the UI
    } else {
        DebugMsg("Failed to insert JSFX: %s\n", jsfxName.c_str());
    }
}

// Command hook function for Reaper custom action
static bool commandHook(KbdSectionInfo *sec, const int command, const int val, const int valhw, const int relmode, HWND hwnd) {
    // Check if the action ID matches the registered actions
    if (command == actionIdOpenCloseReaMODWindow) {
        toggleReaMODWindow();
        return true;
    }
    // if (command == actionIdAddMarkerWithSelectedEvent) {
    //     AddMarkerWithSelectedEvent();
    //     return true;
    // }
    if (command == actionIdAddItemWithSelectedEventAtEditCursor) {
        AddItemWithSelectedEventAtEditCursor();
        return true;
    }
    if (command == actionIdAddItemWithSelectedEventAtEditCursorMatchLength) {
        AddItemWithSelectedEventAtEditCursorMatchLength();
        return true;
    }
    if (command == actionIdAddItemWithSelectedEventWithinTimeSelection) {
        AddItemWithSelectedEventWithinTimeSelection();
        return true;
    }
    if (command == actionIDUpdateNumFramesForItemInsertionFromCurrentTimeSelection) {
        UpdateNumFramesForItemInsertionFromCurrentTimeSelection();
        return true;
    }
    if (command == actionIDStopAndReleaseAllFmodEventInstances) {
        stopReleaseALLFMODEventInstances();
        return true;
    }
    if (command == actionIDInsertParamUpdateItemForSelectedMediaItem) {
        InsertParamUpdateItemForSelectedMediaItem();
        return true;
    }
    if (command == actionIDInsertParamAutomationItemsForSelectedMediaItem) {
        InsertParamAutomationItemsForSelectedMediaItem();
        return true;
    }
    if (command == actionIDInsertPositionInterpolationItemsForSelectedMediaItem) {
        InsertPositionInterpolationItemsForSelectedMediaItem();
        return true;
    }
    // if (command == actionIDPostFmodTracksListToConsole) {
    //     PostFmodTracksListToConsole();
    //     return true;
    // }
    if (command == actionIDSearchForFmodEvent) {
        openFmodEventSearchWindow();
        return true;
    }
    if (command == actionIDTriggerSelectedEvent) {
        PlayStopCurrentSelectedEvent();
        return true;
    }
    // if (command == actionIDInsertParamEnvelopesForSelectedEventOnSelectedItem) {
    //     InsertEventParameterJSFXForSelectedMediaItem();
    //     return true;
    // }
    // if (command == actionIDToggleDebugOnOff) {
    //     toggleDebugMessagesOnOff();
    //     return true;
    // }


    return false;
}

void RegisterActions() {
    plugin_register("hookcommand2", reinterpret_cast<void*>(&commandHook));  // Hook the action
    
    // Register the existing custom action for toggling the ReaMOD window
    static custom_action_register_t actionOpenReaMODWindowReg = { 0, "ReaMOD_OpenCloseReaMODWindow", "ReaMOD: Open/Close Window" };
    actionIdOpenCloseReaMODWindow = plugin_register("custom_action", &actionOpenReaMODWindowReg);  // Assign the action ID to actionIdOpenCloseReaMODWindow

    // Register the new custom action for adding a marker with the selected event at edit cursor
    // static custom_action_register_t actionAddMarkerWithLastFMODEventReg = { 0, "ReaMOD_AddMarkerWithLastFMODEvent", "ReaMOD: Add Marker with Last FMOD Event" };
    // actionIdAddMarkerWithSelectedEvent = plugin_register("custom_action", &actionAddMarkerWithLastFMODEventReg);

    // Register the new custom action for adding a item with the selected event at edit cursor
    static custom_action_register_t actionAddItemWithLastFMODEvent = { 0, "ReaMOD_AddItemWithLastFMODEvent", "ReaMOD: Add Item with selected event at edit cursor" };
    actionIdAddItemWithSelectedEventAtEditCursor = plugin_register("custom_action", &actionAddItemWithLastFMODEvent);

    static custom_action_register_t actionAddItemWithEventLength = { 0, "ReaMOD_AddItemWithEventLength", "ReaMOD: Add item with selected event length at edit cursor" };
    actionIdAddItemWithSelectedEventAtEditCursorMatchLength = plugin_register("custom_action", &actionAddItemWithEventLength);

    // Register the new custom action for adding a item with the selected event within the current time selection
    static custom_action_register_t actionAddItemWithSelectedEventWithinTimeSelection = { 0, "ReaMOD_actionAddItemWithSelectedEventWithinTimeSelection", "ReaMOD: Add Item with selected event within current time selection" };
    actionIdAddItemWithSelectedEventWithinTimeSelection = plugin_register("custom_action", &actionAddItemWithSelectedEventWithinTimeSelection);

    // Register the new custom action for updating the item insertion length based upon the current time selection
    static custom_action_register_t actionUpdateNumFramesForItemInsertionFromCurrentTimeSelection = { 0, "ReaMOD_actionUpdateNumFramesForItemInsertionFromCurrentTimeSelection", "ReaMOD: Update number of frames for item insertion from current time selection" };
    actionIDUpdateNumFramesForItemInsertionFromCurrentTimeSelection = plugin_register("custom_action", &actionUpdateNumFramesForItemInsertionFromCurrentTimeSelection);

    // Register the new custom action for updating the item insertion length based upon the current time selection
    static custom_action_register_t actionStopAndReleaseAllFmodEventInstances = { 0, "ReaMOD_actionStopAndReleaseAllFmodEventInstances", "ReaMOD: Stop/Release All FMOD Event Instances" };
    actionIDStopAndReleaseAllFmodEventInstances = plugin_register("custom_action", &actionStopAndReleaseAllFmodEventInstances);

    // Register the new custom action for updating the item insertion length based upon the current time selection
    static custom_action_register_t actionInsertParamUpdateItemForSelectedMediaItem = { 0, "ReaMOD_actionIDInsertParamUpdateItemForSelectedMediaItem", "ReaMOD: Insert default parameter update item for selected event item on selected track at edit cursor" };
    actionIDInsertParamUpdateItemForSelectedMediaItem = plugin_register("custom_action", &actionInsertParamUpdateItemForSelectedMediaItem);

    // Register the custom action for inserting parameter automation items
    static custom_action_register_t actionInsertParamAutomationItemsForSelectedMediaItem = {0, "ReaMOD_InsertParamAutomationItemsForSelectedMediaItem", "ReaMOD: Insert parameter automation items for selected event item over time selection"};
    actionIDInsertParamAutomationItemsForSelectedMediaItem = plugin_register("custom_action", &actionInsertParamAutomationItemsForSelectedMediaItem);

    // Register the custom action for inserting parameter automation items
    static custom_action_register_t actionInsertPositionInterpolationItemsForSelectedMediaItem = {0, "ReaMOD_actionInsertPositionInterpolationItemsForSelectedMediaItem", "ReaMOD: Insert position interpolation items for selected media item over time selection"};
    actionIDInsertPositionInterpolationItemsForSelectedMediaItem = plugin_register("custom_action", &actionInsertPositionInterpolationItemsForSelectedMediaItem);

    // static custom_action_register_t actionPostFmodTracksListToConsole = { 0, "ReaMOD_PostFmodTracksListToConsole", "ReaMOD: Post current list of FMOD tracks to console" };
    // actionIDPostFmodTracksListToConsole = plugin_register("custom_action", &actionPostFmodTracksListToConsole);

    static custom_action_register_t actionSearchForFmodEvent = { 0, "ReaMOD_SearchForFmodEvent", "ReaMOD: Search for FMOD Event" };
    actionIDSearchForFmodEvent = plugin_register("custom_action", &actionSearchForFmodEvent);

    static custom_action_register_t actionTriggerSelectedEvent = { 0, "ReaMOD_TriggerSelectedEvent", "ReaMOD: Play/Stop Current Selected Event" };
    actionIDTriggerSelectedEvent = plugin_register("custom_action", &actionTriggerSelectedEvent);

    // static custom_action_register_t actionInsertParamEnvelopesForSelectedEventOnSelectedItem = { 0, "ReaMOD_InsertParamEnvelopesForSelectedEventOnSelectedItem", "ReaMOD: Insert parameter envelopes for selected FMOD event on selected media item" };
    // actionIDInsertParamEnvelopesForSelectedEventOnSelectedItem = plugin_register("custom_action", &actionInsertParamEnvelopesForSelectedEventOnSelectedItem);

    // static custom_action_register_t actionToggleDebugOnOff = { 0, "ReaMOD_ToggleDebugMessagesOnOff", "ReaMOD: Toggle posting debug messages on/off." };
    // actionIDToggleDebugOnOff = plugin_register("custom_action", &actionToggleDebugOnOff);

}

// Entry point function for the Reaper plugin
extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT( REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t *rec) {
    if (!rec) return 0;  // If rec is null, clean up
    if (rec->caller_version != REAPER_PLUGIN_VERSION) return 0;  // Check for compatibility
    
    LoadReaperAPIFunctions(rec);  // Load API functions
    RegisterActions(); // Register custom action and command hook

    // Register the central timer function once during initialization
    plugin_register("timer", reinterpret_cast<void*>(&OnTimer));
    return 1;  // Success
}

// Exit point function for the Reaper plugin
extern "C" REAPER_PLUGIN_DLL_EXPORT void REAPER_PLUGIN_EXIT() {
    // Unregister the project state extension
    // Unregister the timer when the plugin is unloaded
    StopAllEvents();
    RemoveTask(playbackTaskId);
    plugin_register("-timer", reinterpret_cast<void*>(&OnTimer));
}