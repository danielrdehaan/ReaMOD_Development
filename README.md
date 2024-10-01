# ReaperFMODPlugin Overview

The **ReaperFMODPlugin.dylib** allows seamless integration between Reaper and FMOD Studio, providing users with control over FMOD events, banks, and audio playback directly within Reaper. This plugin offers several key features, including the ability to load, play, and manage FMOD events and banks, and control FMOD’s internal state during Reaper playback. Below is a breakdown of the plugin's main functionality:

## Installation and Usage
1. Copy the compiled [`ReaperFMODPlugin.dylib`](https://github.com/danielrdehaan/ReaperFMODPlugin/blob/main/build/reaper_ReaperFMODPlugin.dylib) file to Reaper's `UserPlugins` directory.
2. Launch Reaper, and the plugin will automatically register the custom actions for loading, listing, stopping, and refreshing FMOD banks.
3. Use Reaper's action list to map shortcuts or buttons to the available FMOD actions.

## Key Features

### 1. **Loading FMOD Banks**
   - **Action**: `FMOD: Load Bank`
   - Opens a file dialog in Reaper, allowing the user to load an FMOD `.bank` file. Both **Master.bank** and **Master.strings.bank** are loaded automatically when a new bank is selected.
   - All previously loaded banks and their sample data are unloaded before loading new banks.
   - Banks and their events are automatically available for playback after being loaded.

### 2. **Listing Events in a Bank**
   - **Action**: `FMOD: List Current Bank Events`
   - Lists all available FMOD events from the most recently loaded `.bank` file in the ReaScript console.
   - Ensures that event descriptions and paths are retrieved correctly from the bank.

### 3. **Playing FMOD Events**
   - **Action**: (Not registered by default)
   - Allows the playback of any FMOD event triggered from media items in Reaper based on notes attached to the media item (e.g., `event:/path/to/event`). 
   - The playback state is checked to ensure the event is playing, and appropriate debug messages are provided.
   - Handles event instances, allowing multiple events to play simultaneously.

### 4. **Stopping All FMOD Events**
   - **Action**: `FMOD: Stop All Currently Playing Events`
   - Stops all FMOD events routed through the Master bus with fade-out support (`FMOD_STUDIO_STOP_ALLOWFADEOUT`).
   - Ensures that FMOD processes the stop command immediately through `fmodSystem->update()`.

### 5. **Refreshing Loaded Banks**
   - **Action**: `FMOD: Refresh Last Loaded Bank`
   - Unloads and reloads the **Master.bank**, **Master.strings.bank**, and the last loaded `.bank` file. This is useful for reloading FMOD banks after making changes in FMOD Studio.
   - Sample data for the banks is automatically reloaded and flushed to ensure that the changes are reflected within Reaper.

### 6. **Monitoring Play Cursor and Triggering FMOD Events**
   - The plugin monitors Reaper’s playback state and triggers FMOD events based on the playhead’s position relative to media items in Reaper.
   - Triggered events are based on item notes (e.g., an item note that starts with `event:` is used to trigger FMOD events).
   - Events are only triggered once per playthrough unless the playhead is moved backward.

## Debugging and Console Output
   - The plugin provides extensive debug output, ensuring that any issues encountered (e.g., loading, unloading, or playback failures) are clearly logged in the ReaScript console.
   - Custom debug messages are printed when actions are executed or when events and banks are loaded or unloaded.

## Known Issues
- **Multiple Bank Handling**: The plugin currently unloads all previously loaded banks before loading new ones, meaning multiple banks cannot be handled simultaneously without unloading.
