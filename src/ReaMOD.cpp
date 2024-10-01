#include <cstdarg>
#include <string>
#include <memory>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <map>        // Use for storing hierarchical paths
#include <set>        // Use for sorted folder paths
#include <filesystem> // C++17 file system operations
#include "fmod_studio.hpp"
#include "fmod.hpp"
#include "fmod_errors.h"
#include "reaper_plugin.h"

#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"

#define REAIMGUIAPI_IMPLEMENT
#include "reaper_imgui_functions.h"

#define FILE_PATH_BUFFER_SIZE 1024

namespace fs = std::filesystem;  // Alias for easier use of filesystem operations

// Declare the global variable to store the custom action ID
static int g_actionId = 0;

// ImGui context
ImGui_Context* g_imgui_ctx = nullptr;
char selected_file_path[FILE_PATH_BUFFER_SIZE] = "";  // Full path of selected .fspro file
char selected_file_name[FILE_PATH_BUFFER_SIZE] = "No project selected.";  // Initial text in the input box

// Store the list of found .bank files and their toggle states
std::vector<std::string> bank_files;
std::vector<bool> bank_load_states;
std::unordered_map<std::string, FMOD::Studio::Bank*> loaded_banks;  // Map of loaded banks
std::unordered_map<std::string, std::vector<std::string>> bank_events;  // Map of events in each bank

// FMOD system pointers
FMOD::Studio::System* fmod_system = nullptr;

// Load Reaper API functions
void LoadReaperAPIFunctions(reaper_plugin_info_t* rec) {
    if (rec && rec->GetFunc) {
        GetUserFileNameForRead = (bool (*)(char*, const char*, const char*))rec->GetFunc("GetUserFileNameForRead");
    }
}

// Initialize FMOD system
void InitializeFMOD() {
    FMOD::Studio::System::create(&fmod_system);
    fmod_system->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, 0);
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

// Function to split an event path into folder structure
std::map<std::string, std::vector<std::string>> GroupEventsByPath(const std::vector<std::string>& events) {
    std::map<std::string, std::vector<std::string>> grouped_events;
    for (const auto& event : events) {
        size_t last_slash_pos = event.find_last_of('/');
        std::string folder = event.substr(0, last_slash_pos);
        std::string event_name = event.substr(last_slash_pos + 1);
        grouped_events[folder].push_back(event_name);
    }
    return grouped_events;
}

// GUI rendering function
void RenderGUI() {
    ImGui::SetNextWindowSize(g_imgui_ctx, 700, 400, ImGui::Cond_FirstUseEver);

    bool open = true;
    if (ImGui::Begin(g_imgui_ctx, "FMOD Project Selector", &open)) {
        ImGui::Text(g_imgui_ctx, "FMOD Project:");

        // Move the "Select" button to the left of the selected .fspro file
        if (ImGui::Button(g_imgui_ctx, "Select")) {
            OpenFileDialog();
        }
        ImGui::SameLine(g_imgui_ctx);  // Put the file name on the same line as the button
        ImGui::Text(g_imgui_ctx, selected_file_name);

        // List the .bank files found in the "Build/Desktop/" directory
        if (!bank_files.empty()) {
            ImGui::Separator(g_imgui_ctx);  // Add a separator line
            ImGui::Text(g_imgui_ctx, "FMOD Bank Files:");
            
            for (size_t i = 0; i < bank_files.size(); ++i) {
                std::string bank_file_name = fs::path(bank_files[i]).filename().string();
                // Create a unique label for each button by appending the index
                std::string button_label = bank_load_states[i] ? "Unload##" + std::to_string(i) : "Load##" + std::to_string(i);

                // Render the toggle button for loading/unloading the bank on the left
                if (ImGui::Button(g_imgui_ctx, button_label.c_str())) {
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

                ImGui::SameLine(g_imgui_ctx);  // Place the text on the same line as the button

                // Display the bank file name as a collapsible tree node
                if (ImGui::TreeNode(g_imgui_ctx, bank_file_name.c_str())) {
                    // If the bank is loaded, display its events grouped by folder structure
                    if (bank_load_states[i]) {
                        if (bank_events.find(bank_files[i]) != bank_events.end()) {
                            const std::vector<std::string>& events = bank_events[bank_files[i]];
                            auto grouped_events = GroupEventsByPath(events);

                            for (const auto& folder : grouped_events) {
                                // Display the folder as a tree node
                                if (ImGui::TreeNode(g_imgui_ctx, folder.first.c_str())) {
                                    // Display events inside the folder
                                    for (size_t j = 0; j < folder.second.size(); ++j) {
                                        std::string event_label = "Play##" + std::to_string(i) + "_" + std::to_string(j);
                                        if (ImGui::Button(g_imgui_ctx, event_label.c_str())) {
                                            PlayEvent(folder.first + "/" + folder.second[j]);
                                        }
                                        ImGui::SameLine(g_imgui_ctx);
                                        ImGui::Text(g_imgui_ctx, folder.second[j].c_str());
                                    }
                                    ImGui::TreePop(g_imgui_ctx);  // End the folder node
                                }
                            }
                        }
                    }
                    ImGui::TreePop(g_imgui_ctx);  // End the tree node
                }
            }
        }

        ImGui::End(g_imgui_ctx);
    }

    // If the window is closed, unregister the timer to stop rendering
    if (!open) {
        plugin_register("-timer", reinterpret_cast<void*>(&RenderGUI));
        g_imgui_ctx = nullptr;
    }
}

// Command hook function for Reaper custom action
static bool commandHook(KbdSectionInfo *sec, const int command,
  const int val, const int valhw, const int relmode, HWND hwnd)
{
    // Check if the action ID matches
    if (command != g_actionId) return false;

    // Initialize ReaImGui context and set up rendering loop
    if (!g_imgui_ctx) {
        ImGui::init(plugin_getapi);
        g_imgui_ctx = ImGui::CreateContext("ReaMOD Settings & Control");

        InitializeFMOD();  // Initialize the FMOD system

        plugin_register("timer", reinterpret_cast<void*>(&RenderGUI));  // Hook up the render loop
    } else {
        ImGui::SetNextWindowFocus(g_imgui_ctx);  // Focus on the existing window if it's already open
    }

    return true;
}

// Entry point function for the Reaper plugin
extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(
  REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t *rec)
{
    if (!rec) return 0;  // If rec is null, clean up

    LoadReaperAPIFunctions(rec);  // Load API functions

    if (rec->caller_version != REAPER_PLUGIN_VERSION) return 0;  // Check for compatibility

    // Fetch Reaper API functions
    plugin_getapi   = reinterpret_cast<decltype(plugin_getapi)>(rec->GetFunc("plugin_getapi"));
    plugin_register = reinterpret_cast<decltype(plugin_register)>(rec->GetFunc("plugin_register"));
    ShowMessageBox  = reinterpret_cast<decltype(ShowMessageBox)>(rec->GetFunc("ShowMessageBox"));

    // Check if GetUserFileNameForRead is available (already loaded in LoadReaperAPIFunctions)
    if (!GetUserFileNameForRead) {
        ShowMessageBox("GetUserFileNameForRead not available", "Error", 0);
        return 0;
    }

    // Register custom action and command hook
    custom_action_register_t action { 0, "LINK_FMOD_PROJECT", "ReaMOD: Settings & Control" };
    g_actionId = plugin_register("custom_action", &action);  // Assign the action ID to g_actionId

    plugin_register("hookcommand2", reinterpret_cast<void*>(&commandHook));  // Hook the action

    return 1;  // Success
}
