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
static int actionIdAddMarkerWithLastFMODEvent = 0;
static int actionIdAddItemWithLastFMODEvent = 0;

// ImGui context
ImGui_Context* reaMOD_ImGui_Context = nullptr;
char selected_file_path[FILE_PATH_BUFFER_SIZE] = "";  // Full path of selected .fspro file
char selected_file_name[FILE_PATH_BUFFER_SIZE] = "No project selected.";  // Initial text in the input box
std::string currentReaMODFileName = " ";
std::string currentDisplayedFileName = " ";
bool reaModWindowOpen = true;

// Store the list of found .bank files and their toggle states
std::vector<std::string> bank_files;
std::vector<bool> bank_load_states;
std::unordered_map<std::string, FMOD::Studio::Bank*> loaded_banks;  // Map of loaded banks
std::unordered_map<std::string, std::vector<std::string>> bank_events;  // Map of events in each bank
std::unordered_map<int, bool> triggeredMarkers;  // Stores whether a marker has already triggered
std::unordered_map<MediaItem*, bool> triggeredItems; // Global variable to store whether an item has already triggered



// Global variables to track playback
double previousPlayPosition = 0.0;
double lastCallTime = 0.0;
int previousPlayState = 0;

// Look-ahead time for marker triggering
int lookAheadTimeMs = 60;  // Default look-ahead time set to 0 milliseconds

// Task management
std::unordered_map<int, std::function<void()>> taskMap;
int nextTaskId = 0;
int guiTaskId = -1;
int playbackTaskId = -1;

// Global variable to store the last triggered FMOD event path
std::string lastTriggeredFMODEvent;

std::string lastSaveTimestamp; // Global variable to store the last modified timestamp
std::string formattedLastSaveTimestamp; // Holds the formatted "Last Save" text

int numFramesForItem = 10; // Default number of frames for the inserted item
bool moveCursorAfterInsert = true; // Default to true, meaning the cursor moves forward by default

// FMOD system pointers
FMOD::Studio::System* fmod_system = nullptr;

// Map to store the FMOD event instances and their lifecycle
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

// Initialize FMOD system
void InitializeFMOD() {
    FMOD::Studio::System::create(&fmod_system);
    fmod_system->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, 0);
    DebugMsg("Initializing FMOD System.\n");
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

// Load a bank and retrieve its events
void LoadBank(const std::string& bank_path, bool load_sample_data = true) {
    if (loaded_banks.find(bank_path) == loaded_banks.end()) {
        FMOD::Studio::Bank* bank = nullptr;
        FMOD_RESULT result = fmod_system->loadBankFile(bank_path.c_str(), FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);
        if (result == FMOD_OK) {
            // Debug message to indicate that the bank is being loaded
            DebugMsg("Loading bank: %s\n", bank_path.c_str());

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

// Function to find all .bank files in the "Build/Desktop/" directory relative to the selected .fspro file
void FindBankFiles(const std::string& fspro_dir) {
    std::string bank_directory = fspro_dir + "/Build/Desktop/";

    // Clear the previous list of .bank files and toggle states
    bank_files.clear();
    bank_load_states.clear();
    bank_events.clear();
    loaded_banks.clear();

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
            }
        }

        // Second pass: Find Master.bank and store it separately
        for (const auto& entry : fs::directory_iterator(bank_directory)) {
            std::string bank_file = entry.path().string();
            std::string bank_file_name = entry.path().filename().string();

            if (bank_file_name == "Master.bank") {
                master_bank_file = bank_file;  // Store Master.bank path
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

        // Find the .bank files in the "Build/Desktop/" directory
        FindBankFiles(fspro_directory);
    } else {
        // If no file is selected, display the default message and clear the bank file list
        std::strncpy(selected_file_name, "No project selected.", FILE_PATH_BUFFER_SIZE - 1);
        bank_files.clear();
        bank_load_states.clear();
        bank_events.clear();
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

bool isAddingMarker = false;

void AddMarkerWithLastFMODEvent() {
    if (isAddingMarker) return; // Prevent re-entrant calls
    isAddingMarker = true;

    if (lastTriggeredFMODEvent.empty()) {
        PostMsg("No FMOD event has been triggered yet.\n");
        return;
    }

    // Get the current edit cursor position
    double cursorPosition = GetCursorPosition();

    // Create a new marker at the cursor position with the event's full path as the name
    int color = 0; // Use default color
    AddProjectMarker2(nullptr, false, cursorPosition, 0.0, lastTriggeredFMODEvent.c_str(), -1, color);

    DebugMsg("Marker added for last FMOD event: %s\n", lastTriggeredFMODEvent.c_str());

    isAddingMarker = false; // Reset flag after completion
}

void AddItemWithLastFMODEvent() {
    if (lastTriggeredFMODEvent.empty()) {
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

    // Add an empty media item at the cursor position on the selected track
    MediaItem* newItem = AddMediaItemToTrack(selectedTrack);
    if (!newItem) {
        PostMsg("Failed to create a new item.\n");
        return;
    }

    // Set the item position
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

    // Set the lastTriggeredFMODEvent as the name for the take
    GetSetMediaItemTakeInfo_String(newTake, "P_NAME", const_cast<char*>(lastTriggeredFMODEvent.c_str()), true);

    // Update the arrangement view
    UpdateArrange();

    if (moveCursorAfterInsert) {
        // Move the edit cursor forward by the number of frames
        double newCursorPosition = cursorPosition + itemLength;
        SetEditCurPos(newCursorPosition, true, false);
    }

    DebugMsg("Item added at position %.2f with take name: %s\n", cursorPosition, lastTriggeredFMODEvent.c_str());
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
    if (!reaMOD_ImGui_Context) {
        DebugMsg("ImGui context is not available, cannot copy to clipboard.\n");
        return;
    }

    const char* clipboardText = text.c_str();
    if (clipboardText && strlen(clipboardText) > 0) {
        DebugMsg("Attempting to copy to clipboard using ImGui API: %s\n", clipboardText);
        ImGui::SetClipboardText(reaMOD_ImGui_Context, clipboardText);
        DebugMsg("Text successfully copied to clipboard: %s\n", clipboardText);
    } else {
        DebugMsg("Clipboard text is empty or null, cannot copy to clipboard.\n");
    }
}

// Use the ReaImGui MouseButton_Right enum or value
const int RightMouseButton = ImGui::MouseButton_Right;

// Updated function to render an arrow play button and trigger the FMOD event
void RenderPlayButton(ImGui_Context* ctx, const std::string& button_id, const std::string& event_path) {
    if (ImGui::ArrowButton(ctx, ("##play_button_" + button_id).c_str(), ImGui::Dir_Right)) {
        DebugMsg("Play button clicked: Event Path - %s\n", event_path.c_str());  // Debugging event path

        // Trigger the FMOD event when the button is clicked
        PlayEvent(event_path);
        // Update the last triggered event path
        lastTriggeredFMODEvent = event_path;
    }
    if (ImGui::IsItemHovered(ctx) && ImGui::IsMouseReleased(ctx, ImGui::MouseButton_Right)) {
        DebugMsg("Right-click detected on event: %s\n", event_path.c_str());

        // Ensure the prefix is lowercase "event:" before copying to clipboard
        std::string adjustedEventPath = event_path;
        if (adjustedEventPath.rfind("Event:", 0) == 0) {
            adjustedEventPath[0] = 'e'; // Convert "Event:" to "event:"
        }

        CopyToClipboard(adjustedEventPath);  // Copy the adjusted path to the clipboard
    }

}

void CheckMarkers(double playPosition) {
    if (CountProjectMarkers == nullptr || EnumProjectMarkers == nullptr) {
        DebugMsg("Marker functions are not available.\n");
        return;
    }

    int numMarkers = 0, numRegions = 0;
    CountProjectMarkers(nullptr, &numMarkers, &numRegions);  // Count markers and regions

    int totalMarkersAndRegions = numMarkers + numRegions;
    double checkAheadWindow = 1.0;  // Check markers 1 second ahead of play position
    double tolerance = 0.04;        // Small tolerance to account for floating-point inaccuracies

    // Convert lookAheadTimeMs to seconds
    double lookAheadTimeSeconds = lookAheadTimeMs / 1000.0;

    for (int i = 0; i < totalMarkersAndRegions; ++i) {
        bool isRegion = false;
        double markerPosition = 0.0, regionEnd = 0.0;
        const char* name = nullptr;
        int markerIndex = 0;  // Marker index from Reaper

        // Corrected order of arguments for EnumProjectMarkers
        if (EnumProjectMarkers(i, &isRegion, &markerPosition, &regionEnd, &name, &markerIndex)) {
            if (name == nullptr) continue;  // Skip invalid markers

            std::string markerName(name);

            // Adjust marker position by look-ahead time
            double adjustedMarkerPosition = markerPosition - lookAheadTimeSeconds;

            // Only check markers that are within the 1-second window ahead of the play position
            if (adjustedMarkerPosition >= playPosition && adjustedMarkerPosition <= playPosition + checkAheadWindow) {
                DebugMsg("Checking marker %d: %s at position %.2f\n", markerIndex, markerName.c_str(), markerPosition);

                // Trigger the event when the playhead reaches or passes the marker's position (with tolerance)
                if (playPosition >= adjustedMarkerPosition - tolerance && playPosition <= adjustedMarkerPosition + tolerance) {
                    // Check if this marker was already triggered
                    if (!triggeredMarkers[markerIndex]) {
                        DebugMsg("Triggering event for marker: %s at position %.2f\n", markerName.c_str(), markerPosition);
                        PlayEvent(markerName);  // Trigger the FMOD event or snapshot
                        triggeredMarkers[markerIndex] = true;  // Mark this marker as triggered
                    }
                }
            }
        } else {
            DebugMsg("Failed to retrieve marker %d\n", i);
        }
    }
}

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

void UpdateTrackCache() {
    int currentTrackCount = CountTracks(nullptr);
    if (currentTrackCount != cachedTrackCount) {
        // Update cached track list if track count changes
        fmodTracks.clear();
        for (int i = 0; i < currentTrackCount; ++i) {
            MediaTrack* track = GetTrack(nullptr, i);
            if (!track) continue;

            // Get the track name
            char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
            if (trackName && (strstr(trackName, "FMOD") || strstr(trackName, "fmod"))) {
                // If the track name contains "FMOD" or "fmod", add it to the map
                fmodTracks[i] = track;
            }
        }
        cachedTrackCount = currentTrackCount;
    } else {
        // Check for name changes
        for (auto it = fmodTracks.begin(); it != fmodTracks.end();) {
            MediaTrack* track = it->second;
            char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
            if (!trackName || (!strstr(trackName, "FMOD") && !strstr(trackName, "fmod"))) {
                // If track name no longer matches, remove it from the map
                it = fmodTracks.erase(it);
            } else {
                ++it;
            }
        }

        // Look for any new tracks that should be added to the map
        for (int i = 0; i < currentTrackCount; ++i) {
            if (fmodTracks.find(i) == fmodTracks.end()) {
                MediaTrack* track = GetTrack(nullptr, i);
                if (!track) continue;

                char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
                if (trackName && (strstr(trackName, "FMOD") || strstr(trackName, "fmod"))) {
                    fmodTracks[i] = track;
                }
            }
        }
    }
}

void CheckItems(double playPosition) {
    UpdateTrackCache(); // Refresh the track cache before checking items

    double lookAheadTimeSeconds = lookAheadTimeMs / 1000.0;
    double tolerance = 0.04;

    for (const auto& pair : fmodTracks) {
        MediaTrack* track = pair.second;
        int itemCount = CountTrackMediaItems(track);
        for (int j = 0; j < itemCount; ++j) {
            MediaItem* item = GetTrackMediaItem(track, j);
            MediaItem_Take* take = GetActiveTake(item);
            if (!take) continue;

            double itemStart = *(double*)GetSetMediaItemInfo(item, "D_POSITION", nullptr);
            double itemEnd = itemStart + *(double*)GetSetMediaItemInfo(item, "D_LENGTH", nullptr);
            std::string itemGUID = GetItemGUID(item);

            if (playPosition < previousPlayPosition) {
                activeEventInstances.clear();
            }

            if (itemStart <= playPosition + lookAheadTimeSeconds && playPosition <= itemEnd + tolerance) {
                if (activeEventInstances.find(itemGUID) == activeEventInstances.end()) {
                    char itemName[512] = "";
                    if (GetSetMediaItemTakeInfo_String(take, "P_NAME", itemName, false) && strstr(itemName, "event:") == itemName) {
                        std::string eventPath = itemName + 6;
                        CreateFMODEventInstance(eventPath, item, itemStart, itemEnd);
                    }
                }
            } else if (playPosition > itemEnd + tolerance && activeEventInstances.find(itemGUID) != activeEventInstances.end()) {
                ReleaseFMODEventInstance(itemGUID);
            }
        }
    }

    previousPlayPosition = playPosition;
}

bool trackCacheUpdatedDuringPlayback = false; // Flag to track if the cache has been updated during playback

void MonitorPlayback() {
    if (!IsFMODInitialized()) {
        DebugMsg("FMOD system is not initialized. Skipping playback monitoring.\n");
        return;
    }

    if (GetPlayState == nullptr || GetPlayPosition == nullptr) {
        DebugMsg("Playback state functions are not available.\n");
        return;
    }

    int playState = GetPlayState();  // Get current playback state
    double playPosition = GetPlayPosition();  // Get current play position

    // Check if REAPER is playing or recording
    if (playState & 1) {  // REAPER is playing
        DebugMsg("Playback running. Current position: %.2f\n", playPosition);

        // If this is the first time during this playback session, update the track cache
        if (!trackCacheUpdatedDuringPlayback) {
            UpdateTrackCache(); // Refresh the track cache once when playback starts
            trackCacheUpdatedDuringPlayback = true; // Set flag to indicate cache has been updated
        }

        // If playhead moved backward (looping, scrubbing, or jump)
        if (playPosition < previousPlayPosition) {
            DebugMsg("Playhead moved backward. Resetting triggered markers.\n");
            triggeredMarkers.clear();  // Clear all triggered markers to allow retriggering
            triggeredItems.clear();    // Clear all triggered items to allow retriggering
        }

        // Update previous play position
        previousPlayPosition = playPosition;

        // Check markers and trigger FMOD events based on marker positions
        CheckMarkers(playPosition);

        // Check items on tracks named "FMOD" or "fmod" for event or snapshot notes
        CheckItems(playPosition);

    } else if (previousPlayState & 1) {  // REAPER was playing but now stopped
        DebugMsg("Playback stopped. Cleaning up any lingering state.\n");
        triggeredMarkers.clear();  // Clear all triggered markers when playback stops
        triggeredItems.clear();    // Clear all triggered items when playback stops
        trackCacheUpdatedDuringPlayback = false; // Reset the flag when playback stops
    }

    // Update previous play state to track state changes
    previousPlayState = playState;
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
        // If no file path is provided, determine the default name
        if (!currentDisplayedFileName.empty() && currentDisplayedFileName != "No ReaMOD session loaded.") {
            // If a .ReaMOD file is already loaded, use its name
            finalFilePath = currentDisplayedFileName + ".ReaMOD";
        } else {
            // Otherwise, use the currently open Reaper project name
            std::string reaperProjectName = GetCurrentReaperProjectName();
            finalFilePath = reaperProjectName + ".ReaMOD";
        }
    } else {
        // Use the provided file path
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
    outFile << "move_cursor_after_insert=" << (moveCursorAfterInsert ? 1 : 0) << "\n"; // Save the checkbox state
    outFile << "num_frames_for_item=" << numFramesForItem << "\n"; // Save the number of frames for the item

    outFile << "<bank_files>\n";
    for (size_t i = 0; i < bank_files.size(); ++i) {
        outFile << "bank_file=" << bank_files[i] << "\n";
        outFile << "load_state=" << (bank_load_states[i] ? 1 : 0) << "\n";
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

void LoadStateFromFile(const std::string& filePath) {
    if (!IsFMODInitialized()) {
        DebugMsg("FMOD system is not initialized. Cannot load state from file.\n");
        return;
    }

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
        if (line.rfind("fspro_file=", 0) == 0) {
            std::strncpy(selected_file_path, line.substr(11).c_str(), FILE_PATH_BUFFER_SIZE - 1);
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
        } else if (line.rfind("lookahead_time_ms=", 0) == 0) {
            lookAheadTimeMs = std::stoi(line.substr(18));
            DebugMsg("Loaded lookahead_time_ms: %d\n", lookAheadTimeMs);
        } else if (line.rfind("move_cursor_after_insert=", 0) == 0) {
            moveCursorAfterInsert = std::stoi(line.substr(25)) != 0;
            DebugMsg("Loaded move_cursor_after_insert: %d\n", moveCursorAfterInsert);
        } else if (line.rfind("num_frames_for_item=", 0) == 0) {
            numFramesForItem = std::stoi(line.substr(20));
            DebugMsg("Loaded num_frames_for_item: %d\n", numFramesForItem);
        } else if (line == "<bank_files>") {
            while (std::getline(inFile, line) && line != "</bank_files>") {
                if (line.rfind("bank_file=", 0) == 0) {
                    std::string bank_file = line.substr(10);
                    if (bank_file.find("Master.bank") != std::string::npos) {
                        master_bank_file = bank_file;
                    } else {
                        other_bank_files.push_back(bank_file);
                    }
                } else if (line.rfind("load_state=", 0) == 0) {
                    bool load_state = std::stoi(line.substr(11)) != 0;
                    if (!master_bank_file.empty() && other_bank_files.size() == other_bank_load_states.size()) {
                        master_bank_loaded = load_state;
                    } else {
                        other_bank_load_states.push_back(load_state);
                    }
                    DebugMsg("Loaded bank_file: %s, load_state: %d\n",
                             (other_bank_files.size() > 0 ? other_bank_files.back().c_str() : master_bank_file.c_str()),
                             load_state);
                }
            }
        }
    }

    inFile.close();

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

    if (masterStringsBankLoaded) {
        for (size_t i = 0; i < bank_files.size(); ++i) {
            if (bank_load_states[i] && bank_files[i].find("Master.strings.bank") == std::string::npos) {
                LoadBank(bank_files[i]);
            }
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

void RenderGUI() {
    ImGui::SetNextWindowSize(reaMOD_ImGui_Context, 700, 400, ImGui::Cond_FirstUseEver);

    bool open = true;  // Open flag for the window
    if (ImGui::Begin(reaMOD_ImGui_Context, "ReaMOD Window", &open)) {
        // Display the formatted ReaMOD session text
        ImGui::Text(reaMOD_ImGui_Context,"ReaMOD Session: ");
        ImGui::SameLine(reaMOD_ImGui_Context);
        ImGui::Text(reaMOD_ImGui_Context, currentDisplayedFileName.c_str());

        // Display the formatted last save timestamp if available
        if (!formattedLastSaveTimestamp.empty()) {
            ImGui::Text(reaMOD_ImGui_Context, formattedLastSaveTimestamp.c_str());
        }

        // Add Save and Load State buttons
        if (ImGui::Button(reaMOD_ImGui_Context, "Save")) {
            SaveStateDialog();
        }
        ImGui::SameLine(reaMOD_ImGui_Context);
        if (ImGui::Button(reaMOD_ImGui_Context, "Load")) {
            LoadStateDialog();
        }

        ImGui::Separator(reaMOD_ImGui_Context);
        ImGui::Text(reaMOD_ImGui_Context, "FMOD Project:");

        // Move the "Select" button to the left of the selected .fspro file
        if (ImGui::Button(reaMOD_ImGui_Context, "Select")) {
            OpenFileDialog();
        }

        ImGui::SameLine(reaMOD_ImGui_Context);  // Put the file name on the same line as the button
        ImGui::Text(reaMOD_ImGui_Context, selected_file_name);

        // List the .bank files found in the "Build/Desktop/" directory
        if (!bank_files.empty()) {
            ImGui::Separator(reaMOD_ImGui_Context);  // Add a separator line
            ImGui::Text(reaMOD_ImGui_Context, "FMOD Bank Files:");

            for (size_t i = 0; i < bank_files.size(); ++i) {
                std::string bank_file_name = RemoveBankExtension(fs::path(bank_files[i]).filename().string());
                std::string button_label = bank_load_states[i] ? "Unload##" + std::to_string(i) : "Load##" + std::to_string(i);

                // Render the toggle button for loading/unloading the bank on the left
                if (ImGui::Button(reaMOD_ImGui_Context, button_label.c_str())) {
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

                ImGui::SameLine(reaMOD_ImGui_Context);  // Place the text on the same line as the button

                // Display the bank file name as a collapsible tree node
                if (ImGui::TreeNode(reaMOD_ImGui_Context, bank_file_name.c_str())) {
                    // If the bank is loaded, display its events grouped by "Events" and "Snapshots"
                    if (bank_load_states[i]) {
                        if (bank_events.find(bank_files[i]) != bank_events.end()) {
                            const std::vector<std::string>& events = bank_events[bank_files[i]];
                            auto grouped_folders = GroupEventsAndSnapshotsByPath(events);

                            // Iterate over "Events" and "Snapshots"
                            for (const auto& folder_type : grouped_folders) {
                                if (ImGui::TreeNode(reaMOD_ImGui_Context, folder_type.first.c_str())) {  // "Events" or "Snapshots"
                                    // First, display top-level events/snapshots (with an empty folder name)
                                    auto top_level_folder = folder_type.second.find("");
                                    if (top_level_folder != folder_type.second.end()) {
                                        for (const auto& event_pair : top_level_folder->second) {
                                            std::string event_label = "Play##" + event_pair.second;
                                            RenderPlayButton(reaMOD_ImGui_Context, event_label, event_pair.second);
                                            ImGui::SameLine(reaMOD_ImGui_Context);
                                            ImGui::Text(reaMOD_ImGui_Context, event_pair.first.c_str());
                                        }
                                    }

                                    // Then display events/snapshots grouped by their folders
                                    for (const auto& folder : folder_type.second) {
                                        if (folder.first.empty()) {
                                            continue; // Skip top-level, already handled
                                        }
                                        if (ImGui::TreeNode(reaMOD_ImGui_Context, folder.first.c_str())) {
                                            for (const auto& event_pair : folder.second) {
                                                std::string event_label = "Play##" + event_pair.second;
                                                RenderPlayButton(reaMOD_ImGui_Context, event_label, event_pair.second);
                                                ImGui::SameLine(reaMOD_ImGui_Context);
                                                ImGui::Text(reaMOD_ImGui_Context, event_pair.first.c_str());
                                            }
                                            ImGui::TreePop(reaMOD_ImGui_Context);
                                        }
                                    }
                                    ImGui::TreePop(reaMOD_ImGui_Context);
                                }
                            }
                        }
                    }
                    ImGui::TreePop(reaMOD_ImGui_Context);  // End the bank file tree node
                }
            }
        }

        ImGui::Separator(reaMOD_ImGui_Context);
        ImGui::Text(reaMOD_ImGui_Context, "Settings:");

        // Add the InputInt control for Look Ahead Time and keep the text on the same line
        ImGui::SetNextItemWidth(reaMOD_ImGui_Context, 90);
        ImGui::InputInt(reaMOD_ImGui_Context, "##look_ahead_time_ms", &lookAheadTimeMs);
        ImGui::SameLine(reaMOD_ImGui_Context);
        ImGui::Text(reaMOD_ImGui_Context, "Event detection look ahead time (ms)");

        // Add the InputInt control for number of frames
        ImGui::SetNextItemWidth(reaMOD_ImGui_Context, 90);
        ImGui::InputInt(reaMOD_ImGui_Context, "##num_frames_for_item", &numFramesForItem);
        ImGui::SameLine(reaMOD_ImGui_Context);
        ImGui::Text(reaMOD_ImGui_Context, "Number of frames for inserted item");

        // Add the checkbox for moving the cursor after inserting an item
        ImGui::Checkbox(reaMOD_ImGui_Context, "Move Edit Cursor After Insert", &moveCursorAfterInsert);

        ImGui::Separator(reaMOD_ImGui_Context);
        ImGui::Text(reaMOD_ImGui_Context,"ReaMOD v0.1");
        ImGui::Text(reaMOD_ImGui_Context,"Created by Daniel Dehaan");
        ImGui::Text(reaMOD_ImGui_Context, "www.danielrdehaan.com");

        ImGui::End(reaMOD_ImGui_Context);
    }

    // If the window is closed, unregister the timer to stop rendering
    if (!open) {
        reaMOD_ImGui_Context = nullptr;
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
    if (!reaMOD_ImGui_Context) {
        // First-time setup: initialize ReaImGui and FMOD, and start rendering
        ImGui::init(plugin_getapi);
        reaMOD_ImGui_Context = ImGui::CreateContext("ReaMOD Window");

        // Initialize FMOD only if it's not already initialized
        if (!IsFMODInitialized()) {
            InitializeFMOD();  // Initialize the FMOD system
            playbackTaskId = AddTask(MonitorPlayback);  // Add playback monitoring
        }

        // Add the GUI rendering task and store its ID
        guiTaskId = AddTask(RenderGUI);

        // Attempt to auto-load a .ReaMOD file if present
        AutoLoadReaMODFile();

    } else {
        // Remove the GUI rendering task if the window is closed
        if (guiTaskId != -1) {  // Ensure the task ID is valid
            RemoveTask(guiTaskId);
            guiTaskId = -1;  // Invalidate the task ID after removal
        }

        reaMOD_ImGui_Context = nullptr;
    }
}

// Command hook function for Reaper custom action
static bool commandHook(KbdSectionInfo *sec, const int command, const int val, const int valhw, const int relmode, HWND hwnd) {
    // Check if the action ID matches the registered actions
    if (command == actionIdOpenCloseReaMODWindow) {
        toggleReaMODWindow();
        return true;
    }
    if (command == actionIdAddMarkerWithLastFMODEvent) {
        AddMarkerWithLastFMODEvent();
        return true;
    }
    if (command == actionIdAddItemWithLastFMODEvent) {
        AddItemWithLastFMODEvent();
        return true;
    }

    return false;
}

void RegisterActions() {
    plugin_register("hookcommand2", reinterpret_cast<void*>(&commandHook));  // Hook the action
    
    // Register the existing custom action for toggling the ReaMOD window
    static custom_action_register_t actionOpenReaMODWindowReg = { 0, "ReaMOD_OpenCloseReaMODWindow", "ReaMOD: Open/Close Window" };
    actionIdOpenCloseReaMODWindow = plugin_register("custom_action", &actionOpenReaMODWindowReg);  // Assign the action ID to actionIdOpenCloseReaMODWindow

    // Register the new custom action for adding a marker with the last FMOD event
    static custom_action_register_t actionAddMarkerWithLastFMODEventReg = { 0, "ReaMOD_AddMarkerWithLastFMODEvent", "ReaMOD: Add Marker with Last FMOD Event" };
    actionIdAddMarkerWithLastFMODEvent = plugin_register("custom_action", &actionAddMarkerWithLastFMODEventReg);

    // Register the new custom action for adding a marker with the last FMOD event
    static custom_action_register_t actionAddItemWithLastFMODEvent = { 0, "ReaMOD_AddItemWithLastFMODEvent", "ReaMOD: Add Empty Item with Last FMOD Event" };
    actionIdAddItemWithLastFMODEvent = plugin_register("custom_action", &actionAddItemWithLastFMODEvent);
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
