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
#include "fmod_studio.hpp"
#include "fmod.hpp"
#include "fmod_errors.h"
#include "reaper_plugin.h"

#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"

#define REAIMGUIAPI_IMPLEMENT
#include "reaper_imgui_functions.h"

#define FILE_PATH_BUFFER_SIZE 1024

#define DEBUG false

namespace fs = std::filesystem;  // Alias for easier use of filesystem operations

// Declare the global variable to store the custom action ID
static int actionIdOpenCloseReaMODWindow = 0;

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


// Global variables to track playback
double previousPlayPosition = 0.0;
double lastCallTime = 0.0;
double desiredInterval = 1.0;  // 10ms interval for monitoring playback
int previousPlayState = 0;

// Task management
std::unordered_map<int, std::function<void()>> taskMap;
int nextTaskId = 0;
int guiTaskId = -1;
int playbackTaskId = -1;

// FMOD system pointers
FMOD::Studio::System* fmod_system = nullptr;

// Load Reaper API functions
void LoadReaperAPIFunctions(reaper_plugin_info_t* rec) {
    if (rec && rec->GetFunc) {
        GetUserFileNameForRead = (bool (*)(char*, const char*, const char*))rec->GetFunc("GetUserFileNameForRead");
        plugin_getapi   = reinterpret_cast<decltype(plugin_getapi)>(rec->GetFunc("plugin_getapi"));
        plugin_register = reinterpret_cast<decltype(plugin_register)>(rec->GetFunc("plugin_register"));
        ShowMessageBox  = reinterpret_cast<decltype(ShowMessageBox)>(rec->GetFunc("ShowMessageBox"));
        ShowConsoleMsg   = reinterpret_cast<decltype(ShowConsoleMsg)>(rec->GetFunc("ShowConsoleMsg"));
        GetPlayState = reinterpret_cast<decltype(GetPlayState)>(rec->GetFunc("GetPlayState"));
        GetPlayPosition = reinterpret_cast<decltype(GetPlayPosition)>(rec->GetFunc("GetPlayPosition"));
        EnumProjectMarkers = reinterpret_cast<decltype(EnumProjectMarkers)>(rec->GetFunc("EnumProjectMarkers"));
        CountProjectMarkers = reinterpret_cast<decltype(CountProjectMarkers)>(rec->GetFunc("CountProjectMarkers"));
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
}

// Function to check if FMOD System is Initialzed
bool IsFMODInitialized() {
    if (fmod_system) {
        FMOD::System* coreSystem = nullptr;
        FMOD_RESULT result = fmod_system->getCoreSystem(&coreSystem);  // Get the core system
        
        if (result == FMOD_OK && coreSystem) {
            return true;  // FMOD is initialized
        }
    }
    return false;  // FMOD is not initialized
}

// Load a bank and retrieve its events
void LoadBank(const std::string& bank_path, bool load_sample_data = true) {
    if (loaded_banks.find(bank_path) == loaded_banks.end()) {
        FMOD::Studio::Bank* bank = nullptr;
        FMOD_RESULT result = fmod_system->loadBankFile(bank_path.c_str(), FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);
        if (result == FMOD_OK) {
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

    // Check if the bank directory exists
    if (fs::exists(bank_directory) && fs::is_directory(bank_directory)) {
        for (const auto& entry : fs::directory_iterator(bank_directory)) {
            std::string bank_file = entry.path().string();
            std::string bank_file_name = entry.path().filename().string();

            // Automatically load Master.strings.bank but do not display it
            if (bank_file_name == "Master.strings.bank") {
                LoadBank(bank_file, false);  // No need to load sample data for strings bank
            }
            // Automatically load Master.bank but show it in the list
            else if (bank_file_name == "Master.bank") {
                LoadBank(bank_file);  // Load Master.bank with sample data
                bank_files.push_back(bank_file);  // Show in the list
                bank_load_states.push_back(true);  // Mark as loaded by default
            }
            // Add other .bank files to the list
            else if (entry.path().extension() == ".bank") {
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

// Function to play an event
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

// Function to render an arrow play button and trigger the FMOD event
void RenderPlayButton(ImGui_Context* ctx, const std::string& button_id, const std::string& event_path) {
    if (ImGui::ArrowButton(ctx, ("##play_button_" + button_id).c_str(), ImGui::Dir_Right)) {
        DebugMsg("Play button clicked: Event Path - %s\n", event_path.c_str());  // Debugging event path

        // Trigger the FMOD event when the button is clicked
        PlayEvent(event_path);
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

    for (int i = 0; i < totalMarkersAndRegions; ++i) {
        bool isRegion = false;
        double markerPosition = 0.0, regionEnd = 0.0;
        const char* name = nullptr;
        int markerIndex = 0;  // Marker index from Reaper

        // Corrected order of arguments for EnumProjectMarkers
        if (EnumProjectMarkers(i, &isRegion, &markerPosition, &regionEnd, &name, &markerIndex)) {
            if (name == nullptr) continue;  // Skip invalid markers

            std::string markerName(name);

            // Only check markers that are within the 1-second window ahead of the play position
            if (markerPosition >= playPosition && markerPosition <= playPosition + checkAheadWindow) {
                DebugMsg("Checking marker %d: %s at position %.2f\n", markerIndex, markerName.c_str(), markerPosition);

                // Trigger the event when the playhead reaches or passes the marker's position (with tolerance)
                if (playPosition >= markerPosition - tolerance && playPosition <= markerPosition + tolerance) {
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
        }

        // Update previous play position
        previousPlayPosition = playPosition;

        // Check markers and trigger FMOD events based on marker positions
        CheckMarkers(playPosition);

    } else if (previousPlayState & 1) {  // REAPER was playing but now stopped
        DebugMsg("Playback stopped. Cleaning up any lingering state.\n");
        triggeredMarkers.clear();  // Clear all triggered markers when playback stops
    }

    // Update previous play state to track state changes
    previousPlayState = playState;
}



// GUI rendering function
void RenderGUI() {
    ImGui::SetNextWindowSize(reaMOD_ImGui_Context, 700, 400, ImGui::Cond_FirstUseEver);

    bool open = true;  // Open flag for the window
    if (ImGui::Begin(reaMOD_ImGui_Context, "ReaMOD Window", &open)) {
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
    // Check if the action ID matches
    if (command != actionIdOpenCloseReaMODWindow) return false;

    // Call the toggle function to open/close the window
    toggleReaMODWindow();

    return true;
}

// Register custom Reaper actions
void RegisterActions(){

    plugin_register("hookcommand2", reinterpret_cast<void*>(&commandHook));  // Hook the action
    
    // Register custom actions
    static custom_action_register_t actionOpenReaMODWindowReg = { 0, "ReaMOD_OpenCloseReaMODWindow", "ReaMOD: Open/Close Window" };
    actionIdOpenCloseReaMODWindow = plugin_register("custom_action", &actionOpenReaMODWindowReg);  // Assign the action ID to actionIdOpenCloseReaMODWindow
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
    // Unregister the timer when the plugin is unloaded
    RemoveTask(playbackTaskId);
    plugin_register("-timer", reinterpret_cast<void*>(&OnTimer));
}
