# ReaMOD Plugin for REAPER

ReaMOD is a plugin for [REAPER](https://www.reaper.fm/), a digital audio workstation, that integrates FMOD Studio projects into REAPER. It allows users to trigger FMOD events and snapshots directly within REAPER, providing a seamless workflow for game audio development and other interactive audio applications.

## Features

- **FMOD Project Integration**: Load and manage FMOD Studio projects and banks directly within REAPER.
- **Event Browsing and Playback**: Browse through FMOD events and snapshots, play them back, and integrate them into your REAPER projects.
- **Parameter Control**: Adjust FMOD event parameters using GUI sliders, and have these changes reflected in real-time.
- **Marker and Item Insertion**: Insert markers and items associated with FMOD events into the REAPER timeline, allowing for precise synchronization.
- **Playback Monitoring**: Monitor REAPER's playback and trigger FMOD events based on markers or items.
- **Session Management**: Save and load plugin states (`.ReaMOD` files) for consistent sessions across projects.
- **Custom Actions**: Provides custom actions that can be assigned to keyboard shortcuts or toolbar buttons for quick access.

## Installation

### Prerequisites

- **REAPER**: Ensure you have the latest version of REAPER installed.
- **FMOD Studio API**: You need the FMOD Studio API libraries and headers to build and run the plugin. Download from [FMOD's website](https://www.fmod.com/download).
- **ReaImGui**: The plugin uses ReaImGui for the GUI. Ensure that ReaImGui is available in your REAPER installation.
- **TinyFileDialogs**: Used for file dialog operations.
- **C++17 Compiler**: The code uses C++17 features, so you need a compiler that supports C++17.

### Building the Plugin

1. **Clone the Repository**

   ```bash
   git clone https://github.com/yourusername/ReaMOD.git
   ```

2. **Set Up Build Environment**

   - Ensure that you have the FMOD Studio API downloaded and accessible.
   - Set up the include paths and library paths for FMOD in your build configuration.
   - Ensure that the `reaper_plugin.h`, `reaper_plugin_functions.h`, and `reaper_imgui_functions.h` headers are accessible.

3. **Compile the Plugin**

   - Use your preferred C++ compiler to build the plugin.
   - **On Windows (using Visual Studio):**
     - Open the solution file if provided, or create a new DLL project.
     - Add `ReaMOD.cpp` to the project.
     - Set the include directories for FMOD and REAPER SDK.
     - Link against `fmodstudio.lib` and `fmod.lib`.
     - Compile the project to produce `reamod.dll`.

   - **On macOS (using Xcode):**
     - Create a new Dynamic Library project.
     - Add `ReaMOD.cpp` to the project.
     - Set the include directories for FMOD and REAPER SDK.
     - Link against `libfmodstudio.dylib` and `libfmod.dylib`.
     - Compile the project to produce `reamod.dylib`.

   - **On Linux:**
     - Use a command similar to:

       ```bash
       g++ -std=c++17 -shared -fPIC -o reamod.so ReaMOD.cpp -I/path/to/fmod/api/core/inc -L/path/to/fmod/api/core/lib -lfmod -lfmodstudio
       ```

     - Replace `/path/to/fmod/api/core/inc` and `/path/to/fmod/api/core/lib` with the actual paths.

4. **Place the Plugin in REAPER's Plugin Directory**

   - Copy the compiled plugin (`reamod.dll` on Windows, `reamod.so` on Linux, `reamod.dylib` on macOS) into REAPER's `Plugins` directory.

     - **Windows:**

       ```
       C:\Program Files\REAPER (x64)\Plugins\
       ```

     - **macOS:**

       ```
       /Applications/REAPER.app/Contents/Plugins/
       ```

     - **Linux:**

       The location may vary; you can place it in `~/.config/REAPER/UserPlugins/`

5. **Restart REAPER**

   - Restart REAPER to load the new plugin.

## Usage

### Opening the ReaMOD Window

- After installing the plugin, you can open the ReaMOD window by running the custom action:

  ```
  ReaMOD: Open/Close Window
  ```

- You can assign this action to a keyboard shortcut or add it to a toolbar for quick access.

### Loading an FMOD Project

1. **Select FMOD Project**

   - In the ReaMOD window, click the `Select` button to choose an `.fspro` FMOD Studio project file.

2. **Load Banks**

   - After selecting the project, the plugin will find and list the available `.bank` files in the project's `Build/Desktop` directory.
   - Use the `Load` buttons next to each bank to load them into the plugin.

### Browsing and Playing Events

- The loaded banks will display the available FMOD events and snapshots.
- Click the play button next to an event to audition it.
- Click on an event's name to select it. The selected event's parameters will be displayed in the `Selected Event` section.

### Adjusting Parameters

- In the `Selected Event` section, use the sliders to adjust the event's parameters.
- If the event is currently playing, changes to the sliders will affect the event in real-time.

### Inserting Markers and Items

- Use the provided custom actions to insert markers or items associated with the selected FMOD event:

  - **Add Marker with Last FMOD Event**: Inserts a marker at the edit cursor with the selected event.
  - **Add Item with selected event at edit cursor**: Inserts an item at the edit cursor on the selected track with the selected event.
  - **Add Item with selected event within current time selection**: Inserts an item spanning the time selection with the selected event.

- These actions can be assigned to keyboard shortcuts or added to toolbars.

### Playback Integration

- When you play back your REAPER project, the plugin will monitor playback and trigger FMOD events based on markers or items.
- Events will start and stop in sync with REAPER's timeline.

### Saving and Loading Plugin State

- **Save State**: Use the `Save` button to save the current plugin state to a `.ReaMOD` file.
- **Load State**: Use the `Load` button to load a previously saved `.ReaMOD` state.
- The plugin attempts to auto-load a `.ReaMOD` file matching the current REAPER project on startup.

### Custom Actions

The plugin provides several custom actions for enhanced workflow:

- **ReaMOD: Open/Close Window**
- **ReaMOD: Add Marker with Last FMOD Event**
- **ReaMOD: Add Item with selected event at edit cursor**
- **ReaMOD: Add Item with selected event within current time selection**
- **ReaMOD: Update number of frames for item insertion from current time selection**
- **ReaMOD: Stop/Release All FMOD Event Instances**
- **ReaMOD: Insert default parameter update item for selected event item on selected track at edit cursor**
- **ReaMOD: Insert parameter automation items for selected event item over time selection**

Assign these actions to keyboard shortcuts or add them to toolbars for quick access.

### Settings

- **Look Ahead Time**: Adjust the look-ahead time (in milliseconds) for event detection.
- **Number of Frames for Item**: Set the number of frames to use when inserting items.
- **Move Edit Cursor After Insert**: Toggle whether the edit cursor moves to the end of the inserted item.
- **Update Item Length from Time Selection**: When inserting items within the time selection, update the item length based on the time selection.

## Notes

- **Track Naming**: The plugin monitors tracks named "FMOD" (case-insensitive) for triggering events based on items.
- **Parameter Automation**: You can automate FMOD event parameters using items and take names starting with `param:`.

## Dependencies

- **FMOD Studio API**: The FMOD Studio API libraries are required to build and run the plugin.
- **ReaImGui**: Required for the GUI elements. Available at [ReaImGui GitHub](https://github.com/cfillion/reaimgui).
- **TinyFileDialogs**: Used for file dialog operations. Available at [Tiny File Dialogs](https://sourceforge.net/projects/tinyfiledialogs/).

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues on the GitHub repository.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- **Daniel Dehaan**: Original author of the plugin.
- **Cockos Incorporated**: For REAPER and the REAPER SDK.
- **FMOD**: For the FMOD Studio API.
- **cfillion**: For ReaImGui.

## Contact

For any questions or suggestions, please contact [Daniel Dehaan](http://www.danielrdehaan.com).

---

**Disclaimer**: This plugin is provided as-is without any warranty. Use at your own risk.