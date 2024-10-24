#include <cstdarg>
#include <string>
#include <memory>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <map>        // Use for storing hierarchical paths
#include <set>        // Use for sorted folder paths
#include <filesystem> // C++17 file system operations
#include <functional>
#include <chrono>
#include <thread>
#include <fstream> // Include for file I/O operations
#include <ctime>
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
static int actionIdAddItemWithSelectedEventWithinTimeSelection = 0;
static int actionIDUpdateNumFramesForItemInsertionFromCurrentTimeSelection = 0;
static int actionIDStopAndReleaseAllFmodEventInstances = 0;
static int actionIDInsertParamUpdateItemForSelectedMediaItem = 0;
static int actionIDInsertParamAutomationItemsForSelectedMediaItem = 0;
static int actionIDInsertParamEnvelopesForSelectedEventOnSelectedItem = 0;
// static int actionIDPostFmodTracksListToConsole = 0;
static int actionIDSearchForFmodEvent = 0;
static int actionIDTriggerSelectedEvent = 0;


// ImGui context
ImGui_Context* reaMOD_Main_ImGui_Context = nullptr;
ImGui_Context* reaMOD_EventSearch_ImGui_Context = nullptr;
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
std::unordered_map<std::string, FMOD::Studio::Bank*> loaded_banks;  // Map of loaded banks
std::unordered_map<std::string, std::vector<std::string>> bank_events;  // Map of events in each bank
// std::unordered_map<int, bool> triggeredMarkers;  // Stores whether a marker has already triggered
std::unordered_map<MediaItem*, bool> triggeredItems; // Global variable to store whether an item has already triggered
std::unordered_map<std::string, bool> buttonStates; // Global state map to store the color toggle state for each button

std::string selectedFMODEvent = "No event selected";  // Global or static variable to store the selected event

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
    }
}

// A function for showing debug messages in Reaper's console.
void DebugMsg(const char* fmt, ...) {
    // This function can accept fully formated messages
    // or messages that require additional formating.
    // e.g. DebugMsg("Event %d path not found", i);
    if (DEBUG == true) { // only show messages if DEBUG is true
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

// Function to find all .bank files in the "Build/Desktop/" directory relative to the selected .fspro file
void FindBankFiles(const std::string& fspro_dir) {
    std::string bank_directory = fspro_dir + "/Build/Desktop/";

    // Unload all previously loaded banks
    UnloadAllBanks();

    // Temporary vector for holding other bank files
    std::vector<std::string> other_bank_files;
    std::string master_bank_file;

    // Check if the bank directory exists
    if (fs::exists(bank_directory) && fs::is_directory(bank_directory)) {
        // First pass: Load Master.strings.bank
        for (const auto& entry : fs::directory_iterator(bank_directory)) {
            std::string bank_file = entry.path().string();
            std::string bank_file_name = entry.path().filename().string();

            if (bank_file_name == "Master.strings.bank") {
                LoadBank(bank_file, false);  // No need to load sample data for strings bank
                DebugMsg("Loading Master.strings.bank file.\n");
                // Populate masterStringEvents with event paths
                FMOD::Studio::Bank* masterStringsBank = loaded_banks[bank_file];
                if (masterStringsBank) {
                    int stringCount = 0;
                    masterStringsBank->getStringCount(&stringCount);

                    for (int i = 0; i < stringCount; ++i) {
                        char path[512];  // Buffer for event path
                        FMOD_GUID guid;

                        FMOD_RESULT result = masterStringsBank->getStringInfo(i, &guid, path, sizeof(path), nullptr);
                        if (result == FMOD_OK) {
                            masterStringEvents.push_back(path);  // Add event path
                        }
                    }
                }
            }
        }

        // Second pass: Find Master.bank and store it separately
        for (const auto& entry : fs::directory_iterator(bank_directory)) {
            std::string bank_file = entry.path().string();
            std::string bank_file_name = entry.path().filename().string();

            if (bank_file_name == "Master.bank") {
                master_bank_file = bank_file;  // Store Master.bank path
                LoadBank(master_bank_file, true);  // Always load sample data for Master bank
                DebugMsg("Found Master.bank file.\n");
            }
        }

        // Third pass: Add remaining bank files to the list
        for (const auto& entry : fs::directory_iterator(bank_directory)) {
            std::string bank_file = entry.path().string();
            std::string bank_file_name = entry.path().filename().string();

            // Skip Master.strings.bank and Master.bank as they are already processed
            if (bank_file_name == "Master.strings.bank" || bank_file_name == "Master.bank") {
                continue;
            }

            // Add other .bank files to the temporary list
            if (entry.path().extension() == ".bank") {
                other_bank_files.push_back(bank_file);
                DebugMsg("Adding %s to temporary bank list.\n", bank_file.c_str());
            }
        }

        // Sort the other bank files alphabetically
        std::sort(other_bank_files.begin(), other_bank_files.end());

        // Add Master.bank to the beginning of the list if it exists
        if (!master_bank_file.empty()) {
            bank_files.push_back(master_bank_file);
            bank_load_states.push_back(true);  // Mark as loaded by default
            DebugMsg("Adding Master.bank to the bank list.\n");
        }

        // Add the sorted other bank files to the main list
        for (const auto& bank_file : other_bank_files) {
            bank_files.push_back(bank_file);
            bank_load_states.push_back(false);  // Mark as not loaded by default
        }
    }
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

    // Calculate the item length based on the frame rate and the number of frames
    bool dropFrame = false;
    double frameRate = TimeMap_curFrameRate(nullptr, &dropFrame);
    double itemLength = numFramesForItem / frameRate; // Set the length to the specified number of frames

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
        // Move the edit cursor forward by the number of frames
        double newCursorPosition = cursorPosition + itemLength;
        SetEditCurPos(newCursorPosition, true, false);
    }

    DebugMsg("Inserted param update item at position %.2f with GUID: %s\n", cursorPosition, guidStr.c_str());
}

void ParseAndApplyNotes(FMOD::Studio::EventInstance* eventInstance, const std::vector<std::string>& noteLines) {
    if (!eventInstance) {
        DebugMsg("ParseAndApplyNotes: eventInstance is null, skipping.\n");
        return;
    }

    DebugMsg("ParseAndApplyNotes: Number of note lines to process: %d\n", static_cast<int>(noteLines.size()));

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
        // Unknown line format
        else {
            DebugMsg("ParseAndApplyNotes: Unknown note line format: %s\n", line.c_str());
        }
    }
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
        bool result = Envelope_Evaluate(envelope, playPosition, 0, 0, &envelopeValue, nullptr, nullptr, 0);
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

        if (isTrackActive(track)) {
            int itemCount = CountTrackMediaItems(track);
            for (int j = 0; j < itemCount; ++j) {
                MediaItem* item = GetTrackMediaItem(track, j);
                MediaItem_Take* take = GetActiveTake(item);
                if (!take) continue;

                // Check if the media item is muted
                bool isItemMuted = *(bool*)GetSetMediaItemInfo(item, "B_MUTE", nullptr);
                if (isItemMuted) {
                    DebugMsg("Skipping muted item on track: %s\n", trackName.c_str());
                    continue; // Skip to next item if the item is muted
                }

                double itemStart = *(double*)GetSetMediaItemInfo(item, "D_POSITION", nullptr);
                double itemEnd = itemStart + *(double*)GetSetMediaItemInfo(item, "D_LENGTH", nullptr);
                std::string itemGUID = GetItemGUID(item);

                // If the playhead moved backward, clear active instances
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

                        // If within the item's time range
                        if (playPosition >= itemStart - lookAheadTimeSeconds && playPosition <= itemEnd + tolerance) {
                            if (activeEventInstances.find(itemGUID) == activeEventInstances.end()) {
                                // Create the FMOD event instance
                                CreateFMODEventInstance(eventPath, item, itemStart, itemEnd);
                            }

                            // Apply envelope values to the FMOD event parameters
                            MonitorEnvelopesForEventItem(take, itemGUID);
                        } else if (playPosition > itemEnd + tolerance && activeEventInstances.find(itemGUID) != activeEventInstances.end()) {
                            ReleaseFMODEventInstance(itemGUID);
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

    // Unload all currently loaded banks to prevent conflicts
    UnloadAllBanks();

    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        DebugMsg("Failed to open file for loading: %s\n", filePath.c_str());
        return;
    }

    // Clear the current state to avoid duplication
    bank_files.clear();
    bank_load_states.clear();
    bank_events.clear();
    loaded_banks.clear();
    std::strncpy(selected_file_name, "No project selected.", FILE_PATH_BUFFER_SIZE - 1);

    std::string line;
    std::string fsproDirectory;
    bool masterStringsBankLoaded = false;
    std::vector<std::string> other_bank_files;
    std::vector<bool> other_bank_load_states;
    std::string master_bank_file;
    bool master_bank_loaded = false;

    while (std::getline(inFile, line)) {
        // Trim leading and trailing whitespace from line
        line = TrimString(line);

        if (line.empty()) {
            continue; // Skip empty lines
        }

        if (line == "<bank_files>") {
            // Parse bank files section
            while (std::getline(inFile, line)) {
                line = TrimString(line);
                if (line.empty()) {
                    continue; // Skip empty lines
                }
                if (line == "</bank_files>") {
                    break; // End of bank_files section
                }

                size_t equalsPos = line.find('=');
                if (equalsPos == std::string::npos) {
                    DebugMsg("Error: Invalid line in bank_files section: %s\n", line.c_str());
                    continue; // Skip invalid lines
                }

                std::string key = line.substr(0, equalsPos);
                std::string value = line.substr(equalsPos + 1);

                if (key == "bank_file") {
                    std::string bank_file = value;

                    // Read the next non-empty line for load_state
                    while (std::getline(inFile, line) && TrimString(line).empty()) {
                        // Skip empty lines
                    }

                    if (inFile.eof()) {
                        DebugMsg("Error: Unexpected end of file after bank_file in .ReaMOD file.\n");
                        break; // Handle error as needed
                    }

                    line = TrimString(line);

                    size_t equalsPos = line.find('=');
                    if (equalsPos == std::string::npos) {
                        DebugMsg("Error: Invalid line in bank_files section: %s\n", line.c_str());
                        continue; // Skip invalid lines
                    }

                    std::string key = line.substr(0, equalsPos);
                    std::string value = line.substr(equalsPos + 1);

                    if (key == "load_state") {
                        bool load_state = false;
                        try {
                            load_state = (std::stoi(value) != 0);
                        } catch (const std::exception& e) {
                            DebugMsg("Error parsing load_state value: %s\n", e.what());
                            continue; // Skip invalid entries
                        }

                        if (bank_file.find("Master.bank") != std::string::npos) {
                            master_bank_file = bank_file;
                            master_bank_loaded = load_state;
                        } else {
                            other_bank_files.push_back(bank_file);
                            other_bank_load_states.push_back(load_state);
                        }

                        DebugMsg("Loaded bank_file: %s, load_state: %d\n", bank_file.c_str(), load_state);
                    } else {
                        DebugMsg("Error: Expected load_state after bank_file in .ReaMOD file.\n");
                        continue; // Skip invalid entries
                    }
                } else {
                    DebugMsg("Error: Unexpected key in bank_files section: %s\n", key.c_str());
                    continue; // Skip invalid entries
                }
            }
        } else {
            // General key=value parsing
            size_t equalsPos = line.find('=');
            if (equalsPos == std::string::npos) {
                DebugMsg("Error: Invalid line in .ReaMOD file: %s\n", line.c_str());
                continue; // Skip invalid lines
            }

            std::string key = line.substr(0, equalsPos);
            std::string value = line.substr(equalsPos + 1);

            // Now process key and value
            try {
                if (key == "fspro_file") {
                    // Process fspro_file
                    std::strncpy(selected_file_path, value.c_str(), FILE_PATH_BUFFER_SIZE - 1);
                    selected_file_path[FILE_PATH_BUFFER_SIZE - 1] = '\0';
                    DebugMsg("Loaded fspro_file: %s\n", selected_file_path);

                    std::string file_path(selected_file_path);
                    size_t last_slash_pos = file_path.find_last_of("/\\");
                    std::string file_name = file_path.substr(last_slash_pos + 1);
                    std::strncpy(selected_file_name, file_name.c_str(), FILE_PATH_BUFFER_SIZE - 1);
                    fsproDirectory = file_path.substr(0, last_slash_pos);

                    std::string masterStringsBankPath = fsproDirectory + "/Build/Desktop/Master.strings.bank";
                    if (fs::exists(masterStringsBankPath)) {
                        LoadBank(masterStringsBankPath, false);
                        masterStringsBankLoaded = true;
                        DebugMsg("Master.strings.bank loaded from: %s\n", masterStringsBankPath.c_str());
                    } else {
                        DebugMsg("Master.strings.bank not found in: %s\n", masterStringsBankPath.c_str());
                    }
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
                } else {
                    DebugMsg("Error: Unknown key in .ReaMOD file: %s\n", key.c_str());
                    continue; // Skip unknown keys
                }
            } catch (const std::exception& e) {
                DebugMsg("Error parsing value for key %s: %s\n", key.c_str(), e.what());
                continue; // Skip invalid entries
            }
        }
    }

    inFile.close();

    // Ensure that other_bank_files and other_bank_load_states have the same size
    if (other_bank_files.size() != other_bank_load_states.size()) {
        DebugMsg("Error: Mismatch in bank files and load states count.\n");
        return; // Handle error as needed
    }

    // Sort and manage the bank files
    std::vector<std::pair<std::string, bool>> sorted_banks;
    for (size_t i = 0; i < other_bank_files.size(); ++i) {
        sorted_banks.emplace_back(other_bank_files[i], other_bank_load_states[i]);
    }
    std::sort(sorted_banks.begin(), sorted_banks.end());

    if (!master_bank_file.empty()) {
        bank_files.push_back(master_bank_file);
        bank_load_states.push_back(master_bank_loaded);
        DebugMsg("Adding Master.bank to the bank list.\n");
    }

    for (const auto& bank_pair : sorted_banks) {
        bank_files.push_back(bank_pair.first);
        bank_load_states.push_back(bank_pair.second);
    }

    // Verify that bank_files and bank_load_states have the same size
    if (bank_files.size() != bank_load_states.size()) {
        DebugMsg("Error: bank_files and bank_load_states have mismatched sizes: %zu vs %zu\n",
                 bank_files.size(), bank_load_states.size());
        return; // Handle error as needed
    }

    // Debug messages to verify bank files and load states
    DebugMsg("Total bank_files parsed: %zu\n", bank_files.size());
    DebugMsg("Total bank_load_states parsed: %zu\n", bank_load_states.size());
    for (size_t i = 0; i < bank_files.size(); ++i) {
        DebugMsg("Bank file: %s, Load state: %d\n", bank_files[i].c_str(), bank_load_states[i]);
    }

    // Load the banks according to their load states
    for (size_t i = 0; i < bank_files.size(); ++i) {
        if (bank_load_states[i] && bank_files[i].find("Master.strings.bank") == std::string::npos) {
            LoadBank(bank_files[i]);
        }
    }

    DebugMsg("State loaded from file: %s\n", filePath.c_str());

    // Update the displayed file name
    currentDisplayedFileName = getReaMODFileName(filePath);

    // Get the last modification time and format it
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

    // Retrieve global parameters after loading banks
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
int blue = 0x395271FF;
int orange = 0xFFD700FF;

void RenderEventSearchWindow() {

    // Check if the search window should be open
    if (!searchFmodEventWindowOpen) {
        return;
    }
    
    // Set the initial window size
    ImGui::SetNextWindowSize(reaMOD_Main_ImGui_Context, 400, 300, ImGui::Cond_FirstUseEver);

    // Begin the window using the searchFmodEventWindowOpen flag
    if (ImGui::Begin(reaMOD_Main_ImGui_Context, "Event Search", &searchFmodEventWindowOpen, ImGui::WindowFlags_TopMost)) {

        // Display a label for the input field
        ImGui::Text(reaMOD_Main_ImGui_Context, "Event Search:");

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
}


void RenderGUI() {
    ImGui::SetNextWindowSize(reaMOD_Main_ImGui_Context, 700, 400, ImGui::Cond_FirstUseEver);

    ImGui::PushStyleColor(reaMOD_Main_ImGui_Context, ImGui::Col_WindowBg, greyDark);

    bool open = true;  // Open flag for the window
    if (ImGui::Begin(reaMOD_Main_ImGui_Context, "ReaMOD Window", &open, ImGui::WindowFlags_NoFocusOnAppearing)) {

        // Display the formatted ReaMOD session text
        ImGui::Text(reaMOD_Main_ImGui_Context, "ReaMOD Session: ");
        ImGui::SameLine(reaMOD_Main_ImGui_Context);
        ImGui::Text(reaMOD_Main_ImGui_Context, currentDisplayedFileName.c_str());

        // Display the formatted last save timestamp if available
        if (!formattedLastSaveTimestamp.empty()) {
            ImGui::Text(reaMOD_Main_ImGui_Context, formattedLastSaveTimestamp.c_str());
        }

        // Add Save and Load State buttons
        if (ImGui::Button(reaMOD_Main_ImGui_Context, "Save")) {
            SaveStateDialog();
        }
        ImGui::SameLine(reaMOD_Main_ImGui_Context);
        if (ImGui::Button(reaMOD_Main_ImGui_Context, "Load")) {
            LoadStateDialog();
        }
        ImGui::Text(reaMOD_Main_ImGui_Context, "");

        // ImGui::Separator(reaMOD_Main_ImGui_Context);
        ImGui::SeparatorText(reaMOD_Main_ImGui_Context, "FMOD Project:");

        // Move the "Select" button to the left of the selected .fspro file
        if (ImGui::Button(reaMOD_Main_ImGui_Context, "Select")) {
            OpenFileDialog();
        }

        ImGui::SameLine(reaMOD_Main_ImGui_Context);  // Put the file name on the same line as the button
        ImGui::Text(reaMOD_Main_ImGui_Context, selected_file_name);
        ImGui::Text(reaMOD_Main_ImGui_Context, "");

        // List the .bank files found in the "Build/Desktop/" directory
        if (!bank_files.empty()) {
            // ImGui::Separator(reaMOD_Main_ImGui_Context);  // Add a separator line
            ImGui::SeparatorText(reaMOD_Main_ImGui_Context, "FMOD Bank Files:");

            for (size_t i = 0; i < bank_files.size(); ++i) {
                std::string bank_file_name = RemoveBankExtension(fs::path(bank_files[i]).filename().string());
                std::string button_label = bank_load_states[i] ? "Unload##" + std::to_string(i) : "Load##" + std::to_string(i);

                // Render the toggle button for loading/unloading the bank on the left
                if (ImGui::Button(reaMOD_Main_ImGui_Context, button_label.c_str())) {
                    if (bank_load_states[i]) {
                        // If unloading, unload the bank and clear the events
                        loaded_banks[bank_files[i]]->unload();
                        loaded_banks.erase(bank_files[i]);
                        bank_events.erase(bank_files[i]);
                    } else {
                        // If loading, load the bank and retrieve its events
                        LoadBank(bank_files[i]);
                    }
                    bank_load_states[i] = !bank_load_states[i];  // Toggle the load state
                }

                ImGui::SameLine(reaMOD_Main_ImGui_Context);  // Place the text on the same line as the button

                // Display the bank file name as a collapsible tree node
                if (ImGui::TreeNode(reaMOD_Main_ImGui_Context, bank_file_name.c_str())) {
                    // If the bank is loaded, display its events grouped by "Events" and "Snapshots"
                    if (bank_load_states[i]) {
                        if (bank_events.find(bank_files[i]) != bank_events.end()) {
                            const std::vector<std::string>& events = bank_events[bank_files[i]];
                            auto grouped_folders = GroupEventsAndSnapshotsByPath(events);

                            // Iterate over "Events" and "Snapshots"
                            for (const auto& folder_type : grouped_folders) {
                                if (ImGui::TreeNode(reaMOD_Main_ImGui_Context, folder_type.first.c_str())) {  // "Events" or "Snapshots"
                                    // First, display top-level events/snapshots (with an empty folder name)
                                    auto top_level_folder = folder_type.second.find("");
                                    if (top_level_folder != folder_type.second.end()) {
                                        for (const auto& event_pair : top_level_folder->second) {
                                            // Render the play button next to the selectable event
                                            std::string event_label = "Play##" + event_pair.first;
                                            RenderPlayButton(reaMOD_Main_ImGui_Context, event_label, event_pair.second);

                                            ImGui::SameLine(reaMOD_Main_ImGui_Context);  // Keep play button on the same line

                                            // Highlight the selected event
                                            bool isSelected = (selectedFMODEvent == event_pair.second);

                                            // Pass the address of isSelected to ImGui::Selectable
                                            if (ImGui::Selectable(reaMOD_Main_ImGui_Context, event_pair.first.c_str(), &isSelected)) {
                                                selectedFMODEvent = event_pair.second;  // Update selected event
                                                UpdateSelectedEventParameters(selectedFMODEvent);        // Call this function here
                                                DebugMsg("FMOD event selected: %s\n", selectedFMODEvent.c_str());
                                            }
                                        }
                                    }

                                    // Then display events/snapshots grouped by their folders
                                    for (const auto& folder : folder_type.second) {
                                        if (folder.first.empty()) {
                                            continue; // Skip top-level, already handled
                                        }
                                        if (ImGui::TreeNode(reaMOD_Main_ImGui_Context, folder.first.c_str())) {
                                            for (const auto& event_pair : folder.second) {
                                                // Render the play button next to the selectable event
                                                std::string event_label = "Play##" + event_pair.second;
                                                RenderPlayButton(reaMOD_Main_ImGui_Context, event_label, event_pair.second);

                                                ImGui::SameLine(reaMOD_Main_ImGui_Context);  // Keep play button on the same line

                                                // Highlight the selected event
                                                bool isSelected = (selectedFMODEvent == event_pair.second);

                                                // Pass the address of isSelected to ImGui::Selectable
                                                if (ImGui::Selectable(reaMOD_Main_ImGui_Context, event_pair.first.c_str(), &isSelected)) {
                                                    selectedFMODEvent = event_pair.second;  // Update selected event
                                                    UpdateSelectedEventParameters(selectedFMODEvent);        // Call this function here
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
                    ImGui::TreePop(reaMOD_Main_ImGui_Context);  // End the bank file tree node
                }
            }
        }
        ImGui::Text(reaMOD_Main_ImGui_Context, "");

        // Global Parameters Section
        ImGui::SeparatorText(reaMOD_Main_ImGui_Context, "Global Parameters:");
    
        RetrieveGlobalParameters();
    
        if (!groupedGlobalParameters.empty()) {
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
    
        } else {
            ImGui::Text(reaMOD_Main_ImGui_Context, "No global parameters.");
        }



        ImGui::Text(reaMOD_Main_ImGui_Context, "");

        // New section for selected event and parameters
        // ImGui::Separator(reaMOD_Main_ImGui_Context);
        ImGui::SeparatorText(reaMOD_Main_ImGui_Context, "Selected Event:");
    
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

        // New section to display and edit the notes of the selected media item
        ImGui::SeparatorText(reaMOD_Main_ImGui_Context, "Selected Media Item Notes:");
        
        // Retrieve the currently selected media item
        MediaItem* selectedItem = GetSelectedMediaItem(nullptr, 0);
        static char itemNotes[4096] = "";  // Static buffer to persist across frames
        static MediaItem* lastSelectedItem = nullptr;  // To track if the selected item has changed
        
        if (selectedItem) {
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
            if (ImGui::Button(reaMOD_Main_ImGui_Context, "Save Notes")) {
                GetSetMediaItemInfo_String(selectedItem, "P_NOTES", itemNotes, true);  // Save the updated notes
                DebugMsg("Updated notes for selected media item: %s\n", itemNotes);
            }
        } else {
            ImGui::Text(reaMOD_Main_ImGui_Context, "No media item is selected.");
            lastSelectedItem = nullptr;  // Reset if no item is selected
        }




        // ImGui::Separator(reaMOD_Main_ImGui_Context);
        ImGui::SeparatorText(reaMOD_Main_ImGui_Context, "Settings:");

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

        ImGui::Separator(reaMOD_Main_ImGui_Context);
        ImGui::Text(reaMOD_Main_ImGui_Context, "ReaMOD v0.1");
        ImGui::Text(reaMOD_Main_ImGui_Context, "Created by Daniel Dehaan");
        ImGui::Text(reaMOD_Main_ImGui_Context, "www.danielrdehaan.com");

        ImGui::End(reaMOD_Main_ImGui_Context);
    }

    // Pop the style colors
    ImGui::PopStyleColor(reaMOD_Main_ImGui_Context);

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
        DebugMsg("FMOD system is not initialized. Skipping playback monitoring.\n");
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
    if (command == actionIDInsertParamEnvelopesForSelectedEventOnSelectedItem) {
        InsertEventParameterJSFXForSelectedMediaItem();
        return true;
    }


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

    // static custom_action_register_t actionPostFmodTracksListToConsole = { 0, "ReaMOD_PostFmodTracksListToConsole", "ReaMOD: Post current list of FMOD tracks to console" };
    // actionIDPostFmodTracksListToConsole = plugin_register("custom_action", &actionPostFmodTracksListToConsole);

    static custom_action_register_t actionSearchForFmodEvent = { 0, "ReaMOD_SearchForFmodEvent", "ReaMOD: Search for FMOD Event" };
    actionIDSearchForFmodEvent = plugin_register("custom_action", &actionSearchForFmodEvent);

    static custom_action_register_t actionTriggerSelectedEvent = { 0, "ReaMOD_TriggerSelectedEvent", "ReaMOD: Play/Stop Current Selected Event" };
    actionIDTriggerSelectedEvent = plugin_register("custom_action", &actionTriggerSelectedEvent);

    static custom_action_register_t actionInsertParamEnvelopesForSelectedEventOnSelectedItem = { 0, "ReaMOD_InsertParamEnvelopesForSelectedEventOnSelectedItem", "ReaMOD: Insert parameter envelopes for selected FMOD event on selected media item" };
    actionIDInsertParamEnvelopesForSelectedEventOnSelectedItem = plugin_register("custom_action", &actionInsertParamEnvelopesForSelectedEventOnSelectedItem);

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