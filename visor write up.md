# 🎛️ Protogen Visor: Animation Trigger System

## 📜 System Flow

1. **Startup Sequence**
   - Launch the application.
   - Play one random animation from `/animations/loading`.
   - Scan `/animations` for folder names → build **word bank** from folder names.
   - Pre-cache animations into memory for faster playback.
   - Display in terminal:
     ```
     DEPLOYED AND OPERATIONAL
     ```

2. **Keyword Detection Loop**
   - Begin microphone input via USB audio adapter.
   - Use **Vosk** for offline speech-to-text transcription.
   - Listen for user speech continuously.
   - When a keyword (from the word bank) is detected:
     - Pause listening.
     - Play one animation from the matching folder:
       - Play once, or loop for **2 seconds**, whichever is longer.
     - Resume listening after animation playback.

3. **Idle Behavior**
   - If **no speech detected for 5 seconds**:
     - Print a random message from a user-defined list (each message on its own line in a file).
   - If **no speech detected for 30 seconds**:
     - Play one random animation from `/animations/idle`.

4. **Graceful Exit**
   - Pressing `Q` will gracefully terminate the application.

---

## 🧠 Design Considerations

- **Platform**: Raspberry Pi 5 (8 GB RAM, M.2 SSD).
- **Audio Interface**: USB Audio Adapter (input/output).
- **Resource Usage**: Maximize use of **CPU, RAM, and GPU** where beneficial.
- **Latency**: Minimize the time between keyword recognition and animation playback. Aim for **near real-time**.
- **Offline Use**: Must work without an internet connection.
- **Extensibility**: We can rebuild from scratch using better libraries or languages **as long as** they are:
  - Compatible with Raspberry Pi OS
  - Proven to work on Raspberry Pi 5

---

## 📁 Folder Structure

```bash
/animations/
│
├── loading/     # Shown once at startup
├── idle/        # Random idle loops when silent too long
├── hello/       # Keyword-specific folders (user-defined)
├── ...          # Add folders named after trigger words
```