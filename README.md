
# 🦾 Protogen Thought Display System

A modular Raspberry Pi-based animation and subtitle system built for live display via visor, helmet, or glasses-mounted HUD. Uses offline voice recognition to trigger animations and display real-time subtitles in a stylized cyberpunk aesthetic.

## 🚀 Features

- 🎙️ Offline voice recognition using [Vosk](https://alphacephei.com/vosk/)
- 🎞️ Keyword-triggered animation playback via `mpv`
- 🧠 Real-time subtitles displayed on screen (with cyberpunk styling)
- 🔄 Idle animations after periods of silence
- 🧠 Quirky personality messages when quiet
- 🎛️ System launches all components automatically (C++ HUD + Python recognizer)
- 🎨 Cyberpunk aesthetic with colored terminal feedback

## 🛠️ Components

- `main.cpp`: Central HUD control loop
- `animation_manager.*`: Loads and plays categorized animations
- `speech_recognizer.py`: Vosk-powered recognizer that sends triggers/subtitles
- `SBOM`: System design and implementation plan

## 🧩 File Structure

```
visor/
├── animations/            # GIFs and WebPs organized by trigger word
│   ├── hello/
│   ├── idle/
│   └── loading/
|__ model/
├── recognizer/            # Python Vosk recognizer
│   └── speech_recognizer.py
├── src/                   # C++ components
│   ├── main.cpp
│   ├── animation_manager.cpp
│   ├── animation_manager.h
│   └── ...
├── messages.txt           # Optional quirky messages (one per line)
├── SBOM                   # Design document
└── README.md              # This file
```

## 🔧 Setup Instructions

1. Clone the repo:
   ```bash
   git clone https://github.com/h4rml3ss-actual/protogen-thought-display.git
   ```

2. Build the C++ HUD:
   ```bash
   cd visor
   g++ -std=c++17 -Wall -pthread \
       src/main.cpp src/animation_manager.cpp \
       -o build/visor
   ```

3. Make sure your `animations/` folder is populated with GIF or WebP files.

4. Start the HUD:
   ```bash
   ./build/visor
   ```

> ⚠️ Make sure `model` folder exists for Vosk recognizer (`vosk-model-small-en-us-0.15` or similar)

## 💬 Subtitles & Trigger Logic

- Subtitles are streamed from the Python process over a named pipe and rendered as HUD overlays.
- Words matching folder names in `animations/` will trigger playback.

## 📦 Dependencies

- `OpenCV`
- `mpv`
- `Python 3` with `vosk`, `sounddevice`

## 🔮 Future Ideas

- Add gesture control input
- System metrics (CPU temp, net usage) overlay
- Real-time audio visualizer sync
