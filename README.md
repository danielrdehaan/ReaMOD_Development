# ReaMOD Extension for REAPER

ReaMOD is an extension for [REAPER](https://www.reaper.fm/), a digital audio workstation, that integrates FMOD Studio projects into REAPER. It allows users to trigger FMOD events and snapshots directly within REAPER, providing a seamless workflow for game audio development and other interactive audio applications.

I recorded a quick [video](https://www.youtube.com/live/mngoJSFgblk) showing the basic workflow/concepts of ReaMOD. For some reason, OBS captured the audio from Reaper twice so all the playback examples sound off. Sorry! Someday I'll produce a better quality demo video.


## Features

- **FMOD Project Integration**: Load and manage FMOD Studio projects and banks directly within REAPER.
- **Event Browsing and Playback**: Browse through FMOD events and snapshots, play them back, and integrate them into your REAPER projects.
- **Parameter Control**: Adjust FMOD event parameters using GUI sliders, and have these changes reflected in real-time.
- **Item Insertion**: Insert items associated with FMOD events into the REAPER timeline, allowing for precise synchronization.
- **FMOD-Aware Item Lengths**: Optionally size inserted media items from the FMOD event's reported timeline length for tighter timing against authored content.
- **Playback Monitoring**: Monitor REAPER's playback and trigger FMOD events based on markers or items.
- **Custom Actions**: Provides custom actions that can be assigned to keyboard shortcuts or toolbar buttons for quick access.

## Requirements:

This REAPER script requires:
- REAPER 7.20+ (could work with older versions but has not been tested)
- ReaPack
- SWS/S&M REAPER extension
- ReaImGUI

## FMOD

At this time, ReaMOD is intended to be used with FMOD version 2.03.09.

### FMOD Distribution Notice
This repository includes FMOD API library files (.dylib/.lib) for convenience in building and using the ReaMOD extension. This distribution has been authorized by Firelight Technologies.

**Important Licensing Information:**
The inclusion of FMOD library files in this repository does not constitute a license to use FMOD commercially. If you plan to use ReaMOD (or FMOD directly) in any commercial capacity, you must obtain your own license from FMOD.
For complete licensing information and to obtain a commercial license, please visit: [www.fmod.com/legal](www.fmod.com/legal)


## Installation Instructions

ReaMOD is a REAPER extension that integrates FMOD functionality. Due to FMOD licensing, you must supply your own FMOD API libraries.

### Install Dependencies

Before instllating ReaMOD be sure that you have already installed both the SWS/S&M and ReaImGui extension. They can be installed via the ReaPack Pack Manager for Reaper.

1. Download and follow the installation instruction for ReaPack: [https://reapack.com/](https://reapack.com/)

Note: Users of macOS Catalina or newer may need to click on "Allow Anyway" in System Preferences > Security & Privacy after launching REAPER once for ReaPack to load when installed for the first time. Restart REAPER after approving.

2. Once ReaPack is installed, open it from the menu bar Extensions > ReaPack > Browse Packages... and install the following extensions:
    - SWS/S&M extensions
    - ReaImGui: ReaScript binding for Dear ImGui

### Installing ReaMOD

1) Locate your REAPER resource folder
   In REAPER: Options → “Show REAPER resource path in explorer/finder”.
   This opens the root directory where REAPER expects extensions.

2) Install ReaMOD
   - Download the [latest release](https://github.com/danielrdehaan/ReaMOD/releases)
   - Copy the extension into the UserPlugins subfolder:

     - macOS   → REAPER/UserPlugins/reaper_ReaMOD_Plugin.dylib
     - Windows → REAPER/UserPlugins/reaper_ReaMOD_Plugin.dll

3) Provide the FMOD runtime libraries

    - Download the `FMOD Engine 2.03.09` API files for your operating system from https://www.fmod.com/download#fmodengine

   macOS
   -----
   - From the FMOD API download, copy **these two files** into the paths below:
       - `libfmod.dylib` → `<REAPER Resource Folder>/ReaMOD/fmod/core/lib/libfmod.dylib`
       - `libfmodstudio.dylib` → `<REAPER Resource Folder>/ReaMOD/fmod/studio/lib/libfmodstudio.dylib`

   Windows
   -------
   - From the FMOD Windows Desktop SDK (x64), end users need **only**:
       - `fmod.dll`
       - `fmodstudio.dll`
     (Do NOT copy the *.lib files; those are only for building.)
   - Place the DLLs in ONE of these locations so Windows can find them:
       • Same folder as REAPER.exe (recommended)
         e.g., `C:\Program Files\REAPER (x64)\ReaMOD\fmod\`
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

4) Verify (optional)

   *macOS:*
    Show rpaths recorded in the plugin
     `otool -l "REAPER/UserPlugins/reaper_ReaMOD_Plugin.dylib" | awk '/LC_RPATH/{flag=1;next}/Loadcommand/{flag=0}flag' | awk '/path /{print $2}'`

    (Optional) Show linked install names
     `otool -L "REAPER/UserPlugins/reaper_ReaMOD_Plugin.dylib"`

   *Windows:*
    Show dependent DLL names (path resolution happens at load time)
     `dumpbin /DEPENDENTS "REAPER\UserPlugins\reaper_ReaMOD_Plugin.dll"`
    Or use the "Dependencies" GUI tool to check resolution.

5) Add ReaMOD JSFX for automating FMOD Parameters
    - Download the [ReaMOD JSFX.zip](https://github.com/danielrdehaan/ReaMOD/raw/e284a8879326f9861c7ddf5f7e31e8613cd0a0b5/Effects/ReaMOD%20JSFX.zip) file.
    - Unzip the file and place this ReaMOD folder inside the "Effects" folder in Reaper's Resource directory.

5) Restart REAPER

   After copying everything into place, restart REAPER. The ReaMOD menu/actions should appear.

Notes
-----
- macOS Gatekeeper: if macOS blocks the dylib, open System Settings → Privacy & Security and “Allow Anyway”, then restart REAPER. If needed:
    `xattr -dr com.apple.quarantine "REAPER/UserPlugins/reaper_ReaMOD_Plugin.dylib"`
- Architecture: use 64-bit REAPER with 64-bit FMOD (Windows x64; macOS universal is supported).
- Debug FMOD libs (libfmodL.*, fmodL.*) are for development; end users should use the non-L variants.



## Usage

Note to Windows users: ASIO audio drivers have been reported to NOT work with ReaMOD since only one app can control the driver at a time and ReaMOD relies on audio to be able run in the background. 

### Opening the ReaMOD Window

- After installing ReaMOD, you can open the ReaMOD window by running the custom action:

  ```
  ReaMOD: Open/Close Window
  ```

- You can assign this action to a keyboard shortcut or add it to a toolbar for quick access.

### Loading an FMOD Project

1. **Select FMOD Project**

   - In the ReaMOD window, click the `Select` button to choose an `.fspro` FMOD Studio project file.

2. **Manage Bank Directories**

   - After selecting the project, by default ReaMOD will find and list the any available `.bank` files in the project's `Build/Desktop` directory and load the `Master.bank`.
   - The list of bank directories can be managed with the `Add directory...` or `Remove` directory buttons.

3. **Managing Banks**
   - Use the `Load`/`Unload` buttons next to each bank to load/unload them .
   - Press the `Rescan` button to update the `.bank` files after making any changes in FMOD Studio.

### Browsing and Playing Events

- The loaded banks will display the available FMOD events and snapshots.
- Click the play button next to an event to audition it.
- Click on an event's name to select it. The selected event's parameters will be displayed in the `Selected Event` section.

### Adjusting Parameters

- In the `Selected Event` section, use the sliders to adjust the event's parameters.
- If the event is currently playing, changes to the sliders will affect the event in real-time.

### Inserting FMOD Events

- Use the provided custom actions to insert media items to trigger specific FMOD event:

  - **Add Item with selected event at edit cursor**: Inserts an empty media item at the edit cursor on the selected track with the selected FMOD event and the current value of any associated FMOD parameters in the item's notes.
  - **Add Item with selected event within current time selection**: Inserts an empty media item spanning the time selection with the selected event and the current value of any associated FMOD parameters in the item's notes.
  - **Add item with selected event length at edit cursor**: Inserts an empty media item with the same length as the selected FMOD event and the current value of any associated FMOD parameters in the item's notes.

- These actions can be assigned to keyboard shortcuts or added to toolbars.

### Playback Integration

- The ReaMOD window must be open for FMOD events to be triggered.
- When you play back your REAPER project, ReaMOD will automatically check all tracks for FMOD-related media items. It looks for tracks whose names contain ‘FMOD’ or ‘fmod,’ as well as any tracks inside a parent folder whose name contains ‘FMOD’ or ‘fmod.’
- Adjust the `Event detection lookahead time (ms)` in ReaMOD's setting to accomadate any latency issues. Note that timing is a little "loose" in ReaMOD due to several factors that may not be possible to solve.

### Custom Actions

ReaMOD provides several custom actions for enhanced workflow:

- **ReaMOD: Open/Close Window**
    Open or close the ReaMOD window.
- **ReaMOD: Add Item with selected event at edit cursor**
    Adds the currently selected FMOD event in the ReaMOD window with the current parameter values (as item notes) to the selected track at the edit cursor position for the number of frames set in ReaMOD settings.
- **ReaMOD: Add item with selected event length at edit cursor**
    Adds the currently selected FMOD event at the edit cursor and automatically sizes the new item to match the FMOD event's reported length, falling back to the frame-count length when unavailable.
- **ReaMOD: Add Item with selected event within current time selection**
    Adds the currently selected FMOD event in the ReaMOD window with the current parameter values (as item notes) to the selected track at the edit cursor position within the current time selection.
- **ReaMOD: Update number of frames for item insertion from current time selection**
    Updates the "Number of frames for inserted item" value in ReaMOD's settings based upon the currrent time selection made in Reaper.
- **ReaMOD: Stop/Release All FMOD Event Instances**
    Stops and releases all active FMOD event instances.
- **ReaMOD: Insert default parameter update item for selected event item on selected track at edit cursor**
    Inserts an empty media item with the currently selected media item's GUID identifier in its item notes and the defualt item name `param:Name=Value` for use so the user can replace `Name` and `Value` with the desired parameter and value they want to changed for the targeted media item and it corisponding FMOD event.
- **ReaMOD: Insert parameter automation items for selected event item over time selection**
    Current solution for changing FMOD parameters over time. Requires a time selection and a media item to be selected before running the action. After running the action the user is prompted to enter the `parameter name` of the desired FMOD parameter, a `starting value`, and an `ending value`. The action then inserts one media item per frame and interpolates over every frame within the time selection between the starting and ending values provided.
- **ReaMOD: Insert position interpolation items for selected media item over time selection**
    Current solution for controlling FMOD events 3D location. Requires a time selection and a media item to be selected before running the action. The selected media item should be the one used to triggered the 3D FMOD event that the users wants to control. After running the action the user is prompted to enter the starting and ending location (`x`, `y`, and `z`). The action then inserts one media item per frame and interpolates over every frame within the time selection between the starting and ending locations provided.

Assign these actions to keyboard shortcuts or add them to toolbars for quick access.

### Settings

- **Look Ahead Time**: Adjust the look-ahead time (in milliseconds) for event detection.
- **Number of Frames for Item**: Set the number of frames to use when inserting items.
- **Move Edit Cursor After Insert**: Toggle whether the edit cursor moves to the end of the inserted item.
- **Update Item Length from Time Selection**: When inserting items within the time selection, update the item length based on the time selection.

## Recording/Exporting Audio with ReaMOD
All the audio triggered by ReaMOD does not actually flow through Reaper so there are only two options for capturing the audio stream triggered by ReaMOD:
1. Use the "Live Update" feature of FMOD Studio to connect to ReaMOD and then record a new session in FMOD Studio's Profiler window.
2. Use a screen recording software like OBS to record your system audio.  

## Notes

- **Track Naming**: ReaMOD monitors tracks named "FMOD" (case-insensitive) for triggering events based on items.
- **Parameter Automation**: You can automate FMOD event parameters using items and take names starting with `param:`.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues on the GitHub repository.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- **Cockos Incorporated**: For REAPER and the REAPER SDK.
- **FMOD**: For the FMOD Studio API.
- **cfillion**: For ReaImGui.

## Contact

For any questions or suggestions, please contact [Daniel Dehaan](http://www.danielrdehaan.com).

---

**Disclaimer**: ReaMOD is provided as-is without any warranty. Use at your own risk.
