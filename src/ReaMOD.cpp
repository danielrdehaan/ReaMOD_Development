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

// ImGui context
ImGui_Context* reaMOD_ImGui_Context = nullptr;
char selected_file_path[FILE_PATH_BUFFER_SIZE] = "";  // Full path of selected .fspro file
char selected_file_name[FILE_PATH_BUFFER_SIZE] = "No project selected.";  // Initial text in the input box
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

// FMOD system pointers
FMOD::Studio::System* fmod_system = nullptr;

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

    // Temporary vectors for holding other bank files
    std::vector<std::string> other_bank_files;

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

        // Second pass: Load Master.bank
        for (const auto& entry : fs::directory_iterator(bank_directory)) {
            std::string bank_file = entry.path().string();
            std::string bank_file_name = entry.path().filename().string();

            if (bank_file_name == "Master.bank") {
                LoadBank(bank_file);  // Load Master.bank with sample data
                bank_files.push_back(bank_file);  // Show in the list
                bank_load_states.push_back(true);  // Mark as loaded by default
                DebugMsg("Loading Master.bank file.\n");
            }
        }

        // Third pass: Add remaining bank files
        for (const auto& entry : fs::directory_iterator(bank_directory)) {
            std::string bank_file = entry.path().string();
            std::string bank_file_name = entry.path().filename().string();

            // Skip Master.strings.bank and Master.bank as they are already processed
            if (bank_file_name == "Master.strings.bank" || bank_file_name == "Master.bank") {
                continue;
            }

            // Add other .bank files to the list
            if (entry.path().extension() == ".bank") {
                DebugMsg("Adding %s to bank list.\n", bank_file.c_str());
                bank_files.push_back(bank_file);
                bank_load_states.push_back(false);  // False means not loaded
            }
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
        std::string folder = stripped_event.substr(0, last_slash_pos);  // Folder path
        std::string event_name = stripped_event.substr(last_slash_pos + 1);  // Event name

        // Strip prefix from subfolders (if any) in the path
        folder = StripPathPrefix(folder);

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

    // Use ImGui::MouseButton_Right for the right mouse button check
    if (ImGui::IsItemHovered(ctx) && ImGui::IsMouseReleased(ctx, ImGui::MouseButton_Right)) {
        DebugMsg("Right-click detected on event: %s\n", event_path.c_str());
        CopyToClipboard(event_path);  // Copy the full path to the clipboard
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
std::vector<std::string> SplitString(const std::string& str, const std::string& delimiter) {
    size_t start = 0;
    size_t end = str.find(delimiter);
    std::vector<std::string> tokens;

    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }

    tokens.push_back(str.substr(start));
    return tokens;
}

// Function to check items on tracks named "FMOD" or "fmod" for event or snapshot item notes
void CheckItems(double playPosition) {
    int trackCount = CountTracks(nullptr); // Get total number of tracks in the project
    DebugMsg("Checking items. Total tracks: %d\n", trackCount);

    double tolerance = 0.04; // Tolerance for play position checking

    // Iterate over all tracks
    for (int i = 0; i < trackCount; ++i) {
        MediaTrack* track = GetTrack(nullptr, i); // Get track by index
        const char* trackName = (const char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
        
        // Log track name
        DebugMsg("Track %d name: %s\n", i, trackName ? trackName : "(unnamed)");

        // Check if the track name is "FMOD" or "fmod" (case-insensitive comparison)
        if (trackName && (strcasecmp(trackName, "FMOD") == 0 || strcasecmp(trackName, "fmod") == 0)) {
            DebugMsg("Track %d is named 'FMOD'. Checking items...\n", i);

            int itemCount = CountTrackMediaItems(track); // Get number of items on the track
            DebugMsg("Track %d has %d items.\n", i, itemCount);

            // Iterate over all items on the track
            for (int j = 0; j < itemCount; ++j) {
                MediaItem* item = GetTrackMediaItem(track, j);
                
                // Skip if this item has already been triggered
                if (triggeredItems[item]) {
                    DebugMsg("Item %d on track %d has already been fully triggered. Skipping.\n", j, i);
                    continue;
                }
                
                // Retrieve the item note using GetSetMediaItemInfo_String
                char itemNotes[4096];
                bool hasNotes = GetSetMediaItemInfo_String(item, "P_NOTES", itemNotes, false);
                
                if (!hasNotes || strlen(itemNotes) == 0) {
                    DebugMsg("Item %d on track %d has no notes. Skipping.\n", j, i);
                    continue; // Skip if no notes
                }

                // Split item notes into individual lines
                std::vector<std::string> noteLines = SplitString(itemNotes, "\n");
                double itemPosition = *(double*)GetSetMediaItemInfo(item, "D_POSITION", nullptr);
                double itemLength = *(double*)GetSetMediaItemInfo(item, "D_LENGTH", nullptr);
                double itemEndPosition = itemPosition + itemLength;

                DebugMsg("Item %d on track %d: Position=%.2f, Length=%.2f, End=%.2f\n", j, i, itemPosition, itemLength, itemEndPosition);

                // Check if the play position is within the item range
                if (playPosition >= itemPosition - tolerance && playPosition <= itemEndPosition + tolerance) {
                    // Trigger all events or snapshots listed in the item's notes
                    for (const std::string& line : noteLines) {
                        // Trim whitespace from the line
                        std::string trimmedLine = line;
                        trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t\n\r\f\v"));
                        trimmedLine.erase(trimmedLine.find_last_not_of(" \t\n\r\f\v") + 1);

                        // Check if the line starts with "event:" or "snapshot:"
                        if (trimmedLine.rfind("event:", 0) == 0 || trimmedLine.rfind("snapshot:", 0) == 0) {
                            DebugMsg("Triggering event for item note: %s at position %.2f\n", trimmedLine.c_str(), itemPosition);
                            PlayEvent(trimmedLine); // Pass the event or snapshot path
                        }
                    }

                    // Mark the item as triggered after processing all events
                    DebugMsg("Marking item %d on track %d as triggered.\n", j, i);
                    triggeredItems[item] = true;
                }
            }
        }
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

    int playState = GetPlayState();  // Get current playback state
    double playPosition = GetPlayPosition();  // Get current play position

    // Check if REAPER is playing or recording
    if (playState & 1) {  // REAPER is playing
        DebugMsg("Playback running. Current position: %.2f\n", playPosition);

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
    }

    // Update previous play state to track state changes
    previousPlayState = playState;
}

// Function to save the extension's current state to a file
void SaveStateToFile(const std::string& filePath) {
    std::ofstream outFile(filePath);
    if (!outFile) {
        DebugMsg("Failed to open file for saving: %s\n", filePath.c_str());
        return;
    }

    // Save relevant state information
    outFile << "fspro_file=" << selected_file_path << "\n";
    outFile << "lookahead_time_ms=" << lookAheadTimeMs << "\n";

    outFile << "<bank_files>\n";
    for (size_t i = 0; i < bank_files.size(); ++i) {
        outFile << "bank_file=" << bank_files[i] << "\n";
        outFile << "load_state=" << (bank_load_states[i] ? 1 : 0) << "\n";
    }
    outFile << "</bank_files>\n";

    outFile.close();
    DebugMsg("State saved successfully to: %s\n", filePath.c_str());
}

// Function to load the extension state from a .txt file
void LoadStateFromFile(const std::string& filePath) {
    // Check if the FMOD system is initialized
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

    while (std::getline(inFile, line)) {
        if (line.rfind("fspro_file=", 0) == 0) {
            // Load .fspro file path
            std::strncpy(selected_file_path, line.substr(11).c_str(), FILE_PATH_BUFFER_SIZE - 1);
            selected_file_path[FILE_PATH_BUFFER_SIZE - 1] = '\0';
            DebugMsg("Loaded fspro_file: %s\n", selected_file_path);

            // Extract the .fspro directory
            std::string file_path(selected_file_path);
            size_t last_slash_pos = file_path.find_last_of("/\\");
            std::string file_name = file_path.substr(last_slash_pos + 1);
            std::strncpy(selected_file_name, file_name.c_str(), FILE_PATH_BUFFER_SIZE - 1);
            fsproDirectory = file_path.substr(0, last_slash_pos);

            // Attempt to load the Master.strings.bank file immediately
            std::string masterStringsBankPath = fsproDirectory + "/Build/Desktop/Master.strings.bank";
            if (fs::exists(masterStringsBankPath)) {
                LoadBank(masterStringsBankPath, false); // Load without sample data
                masterStringsBankLoaded = true;
                DebugMsg("Master.strings.bank loaded from: %s\n", masterStringsBankPath.c_str());
            } else {
                DebugMsg("Master.strings.bank not found in: %s\n", masterStringsBankPath.c_str());
            }
        } else if (line.rfind("lookahead_time_ms=", 0) == 0) {
            lookAheadTimeMs = std::stoi(line.substr(18));
            DebugMsg("Loaded lookahead_time_ms: %d\n", lookAheadTimeMs);
        } else if (line == "<bank_files>") {
            while (std::getline(inFile, line) && line != "</bank_files>") {
                if (line.rfind("bank_file=", 0) == 0) {
                    bank_files.push_back(line.substr(10));
                } else if (line.rfind("load_state=", 0) == 0) {
                    bank_load_states.push_back(std::stoi(line.substr(11)) != 0);
                    DebugMsg("Loaded bank_file: %s, load_state: %d\n", bank_files.back().c_str(), bank_load_states.back());
                }
            }
        }
    }

    inFile.close();

    // Load other bank files if the Master.strings.bank was successfully loaded
    if (masterStringsBankLoaded) {
        for (size_t i = 0; i < bank_files.size(); ++i) {
            if (bank_load_states[i] && bank_files[i].find("Master.strings.bank") == std::string::npos) {
                LoadBank(bank_files[i]);
            }
        }
    }

    DebugMsg("State loaded from file: %s\n", filePath.c_str());
}



void SaveStateDialog() {
    const char* filterPatterns[2] = { "*.ReaMOD", "*.*" };
    const char* savePath = tinyfd_saveFileDialog(
        "Save State As",  // Dialog title
        "state.ReaMOD",   // Default filename with .ReaMOD extension
        2,                // Number of filter patterns
        filterPatterns,   // Filter patterns array
        "ReaMOD files (*.ReaMOD)" // Filter description
    );

    if (savePath) {
        // If the file name doesn't end with .ReaMOD, add the extension
        std::string savePathStr(savePath);
        if (savePathStr.find(".ReaMOD") == std::string::npos) {
            savePathStr += ".ReaMOD";
        }
        SaveStateToFile(savePathStr);
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
        DebugMsg("State loaded successfully from: %s\n", loadPath);
    } else {
        DebugMsg("Load operation canceled or invalid file name.\n");
    }
}

// Update the RenderGUI function to include the right-click clipboard feature
void RenderGUI() {
    ImGui::SetNextWindowSize(reaMOD_ImGui_Context, 700, 400, ImGui::Cond_FirstUseEver);

    bool open = true;  // Open flag for the window
    if (ImGui::Begin(reaMOD_ImGui_Context, "ReaMOD Window", &open)) {
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
                std::string bank_file_name = fs::path(bank_files[i]).filename().string();
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
                                    // Iterate over subfolders
                                    for (const auto& folder : folder_type.second) {
                                        if (ImGui::TreeNode(reaMOD_ImGui_Context, folder.first.c_str())) {
                                            // Display events inside the folder
                                            for (size_t j = 0; j < folder.second.size(); ++j) {
                                                std::string event_label = "Play##" + std::to_string(i) + "_" + std::to_string(j);
                                                const std::string& display_name = folder.second[j].first; // Stripped name for display
                                                const std::string& full_path = folder.second[j].second;   // Full path for playback

                                                // Render the play button and pass the full event path to PlayEvent
                                                RenderPlayButton(reaMOD_ImGui_Context, event_label, full_path);

                                                ImGui::SameLine(reaMOD_ImGui_Context);
                                                ImGui::Text(reaMOD_ImGui_Context, display_name.c_str());
                                            }
                                            ImGui::TreePop(reaMOD_ImGui_Context);  // End the folder node
                                        }
                                    }
                                    ImGui::TreePop(reaMOD_ImGui_Context);  // End the "Events"/"Snapshots" node
                                }
                            }
                        }
                    }
                    ImGui::TreePop(reaMOD_ImGui_Context);  // End the bank file tree node
                }
            }
        }

        ImGui::Separator(reaMOD_ImGui_Context);
        ImGui::Text(reaMOD_ImGui_Context, "Look Ahead Time (ms):");
        ImGui::SetNextItemWidth(reaMOD_ImGui_Context, 90);
        ImGui::InputInt(reaMOD_ImGui_Context, "##look_ahead_time_ms", &lookAheadTimeMs);
        // lookAheadTimeMs = std::max(0, lookAheadTimeMs);  // Prevent negative values

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

// Open/Close ReaMODWindow
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
    } else if (command == actionIdAddMarkerWithLastFMODEvent) {
        AddMarkerWithLastFMODEvent();
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
    RemoveTask(playbackTaskId);
    plugin_register("-timer", reinterpret_cast<void*>(&OnTimer));
}
