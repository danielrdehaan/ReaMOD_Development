# Dev Log
- 2025-10-01: Finializing Mac OS Universal Github build workflow. Still having issues getting the workflow artifact to upload to the `releases` directory. Once that is all working correctly I'll move on to the Windows build workflow. Hopefully now that I have relative paths for FMOD api working for Mac the Windows build will be fairly simple.

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

## Installation Instructions

ReaMOD is a REAPER extension that integrates FMOD functionality. Due to FMOD licensing, you must supply your own FMOD API libraries.

1) Locate your REAPER resource folder
   In REAPER: Options → “Show REAPER resource path in explorer/finder”.
   This opens the root directory where REAPER expects extensions.

2) Install the ReaMOD plugin
   - Download the latest release: https://github.com/danielrdehaan/ReaMOD/releases
   - Copy the plugin into the UserPlugins subfolder:

     macOS   → REAPER/UserPlugins/reaper_ReaMOD_Plugin.dylib
     Windows → REAPER/UserPlugins/reaper_ReaMOD_Plugin.dll   ← (underscore)

3) Provide the FMOD runtime libraries

   macOS
   -----
   - From the FMOD API download, copy **these two files** into the paths below:
       `REAPER/ReaMOD/fmod/core/lib/libfmod.dylib`
       `REAPER/ReaMOD/fmod/studio/lib/libfmodstudio.dylib`
   - The plugin uses relative rpaths:
       `@loader_path`
       `@loader_path/../ReaMOD/fmod/core/lib`
       `@loader_path/../ReaMOD/fmod/studio/lib`

   Windows
   -------
   - From the FMOD Windows Desktop SDK (x64), end users need **only**:
       fmod.dll
       fmodstudio.dll
     (Do NOT copy the *.lib files; those are only for building.)
   - Place the DLLs in ONE of these locations so Windows can find them:
       • Same folder as REAPER.exe (recommended)
         e.g., C:\Program Files\REAPER (x64)\
       • Or any folder that is on the user/system PATH
         (You can add your chosen ReaMOD\...\lib folder to PATH if preferred.)

   Example macOS-oriented layout (Windows users typically put DLLs next to REAPER.exe, not here):
   ```
     REAPER/
     ├─ UserPlugins/
     │  └─ reaper_ReaMOD_Plugin.(dylib|dll)
     └─ ReaMOD/
        └─ fmod/
           ├─ core/
           │  └─ lib/
           │     └─ libfmod.dylib              (macOS)
           └─ studio/
              └─ lib/
                 └─ libfmodstudio.dylib        (macOS)
    ```

4) Verify

   *macOS:*
    Show rpaths recorded in the plugin
     `otool -l "REAPER/UserPlugins/reaper_ReaMOD_Plugin.dylib" | awk '/LC_RPATH/{flag=1;next}/Loadcommand/{flag=0}flag' | awk '/path /{print $2}'`

    (Optional) Show linked install names
     `otool -L "REAPER/UserPlugins/reaper_ReaMOD_Plugin.dylib"`

   *Windows:*
    Show dependent DLL names (path resolution happens at load time)
     `dumpbin /DEPENDENTS "REAPER\UserPlugins\reaper_ReaMOD_Plugin.dll"`
    Or use the "Dependencies" GUI tool to check resolution.

5) Restart REAPER

   After copying everything into place, restart REAPER. The ReaMOD menu/actions should appear.

Notes
-----
- macOS Gatekeeper: if macOS blocks the dylib, open System Settings → Privacy & Security and “Allow Anyway”, then restart REAPER. If needed:
    `xattr -dr com.apple.quarantine "REAPER/UserPlugins/reaper_ReaMOD_Plugin.dylib"`
- Architecture: use 64-bit REAPER with 64-bit FMOD (Windows x64; macOS universal is supported).
- Debug FMOD libs (libfmodL.*, fmodL.*) are for development; end users should use the non-L variants.



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