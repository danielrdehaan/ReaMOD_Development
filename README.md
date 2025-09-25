# ReaMOD Plugin for REAPER

ReaMOD is a plugin for [REAPER](https://www.reaper.fm/), a digital audio workstation, that integrates FMOD Studio projects into REAPER. It allows users to trigger FMOD events and snapshots directly within REAPER, providing a seamless workflow for game audio development and other interactive audio applications.

## Features

- **FMOD Project Integration**: Load and manage FMOD Studio projects and banks directly within REAPER.
- **Event Browsing and Playback**: Browse through FMOD events and snapshots, play them back, and integrate them into your REAPER projects.
- **Parameter Control**: Adjust FMOD event parameters using GUI sliders, and have these changes reflected in real-time.
- **Item Insertion**: Insert items associated with FMOD events into the REAPER timeline, allowing for precise synchronization.
- **Playback Monitoring**: Monitor REAPER's playback and trigger FMOD events based with items.
- **Session Management**: Save and load plugin states (`.ReaMOD` files) for consistent sessions across projects.
- **Custom Actions**: Provides custom actions that can be assigned to keyboard shortcuts or toolbar buttons for quick access.

## Installation

1. Download and install the latest version of the SWS/S&M Extension for Reaper: https://www.sws-extension.org/

  Warning: Mac users after copy/pasting the sws extension file in to the Reaper's User Plugins folder you must approve the .dylib file by first right clicking on the .dylib file and selecting Open. An alert willl appear informing the user that this operation is not safe. Acknowledge the alert then go to System Settings > Privacy & Security and scroll toward the bottom. You should see a button labeled "Open Anyway" next to a message saying that "reaper_sws-x86_64.dylib was blocked to protect your Mac." Click the "Open Anyway" button. On the resulting pop-up alert click "Open Anayway" and enter your user password if prompted. Then proceed to the next step of these installation instructions.

2. Relaunch Reaper and confirm that the SWS Extension has been succesfully installed by checking that an "Extension" dropdown menu has appeared in Reaper's Menu Bar.

3. Download and install ReaPack from https://reapack.com.

4. Restart REAPER after installing ReaPack.
  
  Warning: Users of macOS Catalina or newer may need to click on "Allow Anyway" in System Preferences > Security & Privacy after launching REAPER once for ReaPack to load when installed for the first time. Restart REAPER after approving.

5. Open ReaPack from the menu bar Extensions > ReaPack > Browse Packages... and install the "ReaImGui: ReaScript binding for Dear ImGui" extensions:

6. Restart Reaper

7. Download the latest build for your operating system

    - Mac
    - Windows

8. Unzip the downloaded file and copy/paste the .dll (Windows) or .dylib (Mac) file into the Reaper Resources Path/UserPlugins.
    
    Warning: Mac users must approve the .dylib file by first right clicking on the .dylib file and selecting Open. An alert willl appear informing the user that this operation is not safe. Acknowledge the alert then go to System Settings > Privacy & Security and scroll toward the bottom. You should see a button labeled "Open Anyway" next to a message saying that "reaper_ReaMOD_Plugin.dylib was blocked to protect your Mac." Click the "Open Anyway" button. On the resulting pop-up alert click "Open Anayway" and enter your user password if prompted. Then proceed to the next step of these installation instructions.

9. If Reaper is running restart Reaper.

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

### Inserting Items

- Use the provided custom actions to insert items associated with the selected FMOD event:

  - **Add Item with selected event at edit cursor**: Inserts an item at the edit cursor on the selected track with the selected event.
  - **Add Item with selected event within current time selection**: Inserts an item spanning the time selection with the selected event.

- These actions can be assigned to keyboard shortcuts or added to toolbars.

### Playback Integration

- When you play back your REAPER project, the plugin will monitor playback and trigger FMOD events based on items.
- Events will start and stop in sync with REAPER's timeline.

### Saving and Loading Plugin State

- **Save State**: Use the `Save` button to save the current plugin state to a `.ReaMOD` file.
- **Load State**: Use the `Load` button to load a previously saved `.ReaMOD` state.
- The plugin attempts to auto-load a `.ReaMOD` file matching the current REAPER project on startup.

### Custom Actions

The plugin provides several custom actions for enhanced workflow:

- **ReaMOD: Open/Close Window**
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