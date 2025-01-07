# Protogen Visor Display Application

## Overview
The Protogen Visor Display is an innovative application designed for wearable technology. It integrates **voice recognition** with a **dynamic animation system** to create a reactive and engaging user experience. The visor:
- Listens for voice input using offline speech recognition.
- Displays animations triggered by specific words or phrases.
- Features idle animations and visual effects for continuous engagement.

This repository includes three core components:
1. **Speech Recognizer** (`speech_recognizer.py`) - Handles real-time voice recognition.
2. **Animation Controller** (`protogen_visor.cpp`) - Displays animations on the visor based on recognized words.
3. **Cleanup Utility** (`cleanup_animations.py`) - Maintains the animation directory structure by removing invalid files.

---

## Features
### Speech Recognition
- Real-time audio capture and processing using the **Vosk library**.
- Offline speech recognition for trigger words.
- Sends recognized words to the animation controller for immediate display.

### Animation Control
- Plays animations stored in specific folders for trigger words.
- Implements **idle state animations** for periods of silence.
- Ensures smooth transitions between animation states.
- Prevents animation repetition within the same context.

### Cleanup Utility
- Scans the animation directory for unnecessary or corrupt files.
- Removes files with invalid extensions or hidden metadata.
- Keeps the animation library organized and efficient.

---

## Prerequisites

### Hardware
- Raspberry Pi 5 8gb
- HDMI compatible display
- Microphone for audio input.


### Software
#### Python Components
- Python 3.7+
- Dependencies (install using `pip`):
  - `vosk`
  - `numpy`
  - `opencv-python`
  - `sounddevice`
  - `psutil`
  - `json`

#### C++ Components
- GCC 7.0+ (for C++17 support):
  ```bash
  sudo apt install build-essential
  ```
- `mpv` for animation playback:
  ```bash
  sudo apt install mpv
  ```
- POSIX libraries (already included in most Linux distributions):
  - `<poll.h>`, `<unistd.h>`, `<sys/wait.h>`.

---

## Installation
1. Clone this repository:
   ```bash
   git clone https://github.com/yourusername/protogen-visor.git
   cd protogen-visor
   ```

2. Install Python dependencies:
   ```bash
   chmod +x pregame.sh
   ./pregame.sh
   ```

3. Compile the animation controller:
   ```bash
   g++ protogen_visor.cpp -o protogen_visor -lstdc++fs -pthread
   ```

4. Organize the `animations/` directory with subfolders for each trigger word (e.g., `hello`, `loading`).

---

## Usage

### Running the Application
1. Launch the animation controller (this spawns the speech recognizer automatically):
   ```bash
   ./protogen_visor
   ```

2. Maintain the animation directory periodically with the cleanup utility:
   ```bash
   python3 cleanup_animations.py
   ```

---

## User Experience

1. **Startup**:
   - A "loading" animation is displayed while the system initializes.

2. **Listening**:
   - The visor enters a ready state and listens for predefined words.

3. **Trigger Words**:
   - When a trigger word is detected, the visor displays a corresponding animation.

4. **Idle State**:
   - If animation is detected for 3 seconds, the visor prints an idle statement to the command line.  After 10 seconds, it plays an animation from the 'idle' folder.

5. **Error Handling**:
   - If a trigger word folder is missing or empty, the system logs the issue and moves on.

---

## Technical Details

### Speech Recognizer (`speech_recognizer.py`)
- Captures real-time audio and processes it using the **Vosk library**.
- Recognizes predefined words and sends them via inter-process communication (IPC) to the animation controller.
- Automatically started by the animation controller during runtime.

### Animation Controller (`protogen_visor.cpp`)
- Displays animations stored in trigger word folders.
- Implements:
  - **State transitions** (e.g., idle to triggered animation).
- Utilizes efficient threading and file system checks for real-time performance.
- Uses `mpv` to handle animation playback in a seamless and lightweight manner.

### Cleanup Utility (`cleanup_animations.py`)
- Traverses the `animations/` directory.
- Removes invalid files (e.g., hidden metadata, incorrect formats).
- Logs cleanup actions for transparency.

---

## Future Enhancements
- Lowering latency is a top priority
- Add triggers based on hotkeys/sensors/etc
- Allow to be run at startup using systemd or cron.
- Lots of room for general improvements
---

## Contributing
Contributions are welcome! Please fork the repository and submit a pull request with your improvements or suggestions.

---

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.

