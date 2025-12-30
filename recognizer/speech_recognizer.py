# Real-time speech recognition and audio modulation system for a cyberpunk HUD
import os
import queue
import vosk
import sys
import json
import threading
import time
import numpy as np

import pyaudio

# ANSI color codes for cyberpunk theme
CLR_RESET = "\033[0m"
CLR_PINK = "\033[38;2;255;20;147m"
CLR_PURPLE = "\033[38;2;128;0;128m"
CLR_GREEN = "\033[38;2;0;255;0m"
CLR_CYAN = "\033[38;2;0;255;255m"
CLR_YELLOW = "\033[38;2;255;255;0m"

# Globals
audio_queue = queue.Queue(maxsize=8)  # bounded to avoid unbounded lag if processing stalls
trigger_words = set()
recognized_word = None
subtitle_pipe_path = "/tmp/visor_subtitles"
pipe_path = "/tmp/visor_pipe"
spectrum_pipe_path = "/tmp/visor_spectrum"

# Store open file descriptors for inter-process communication pipes
pipe_handles = {
    "pipe": None,
    "subtitle": None,
    "spectrum": None,
}

# Loads trigger keywords from animation directory names
def load_trigger_words(directory="animations"):
    global trigger_words
    try:
        trigger_words = {entry.name for entry in os.scandir(directory) if entry.is_dir()}
        print(f"{CLR_CYAN}[PY] :: [TR1GG3RZ L0ADED] >> {sorted(trigger_words)}{CLR_RESET}")
    except Exception as e:
        print(f"[Python] Failed to load trigger words: {e}", file=sys.stderr)

# Sends detected trigger word to the animation control pipe
def send_to_pipe(word):
    try:
        if pipe_handles["pipe"] is None:
            fd = os.open(pipe_path, os.O_WRONLY | os.O_NONBLOCK)
            pipe_handles["pipe"] = os.fdopen(fd, "w")
        pipe_handles["pipe"].write(word + "\n")
        pipe_handles["pipe"].flush()
    except Exception as e:
        pipe_handles["pipe"] = None
        print(f"{CLR_YELLOW}[PY] :: [PIPE WRI7E ERR] >> {e}{CLR_RESET}", file=sys.stderr)

# Sends recognized speech text to the subtitle pipe
def send_subtitle(text):
    try:
        if pipe_handles["subtitle"] is None:
            fd = os.open(subtitle_pipe_path, os.O_WRONLY | os.O_NONBLOCK)
            pipe_handles["subtitle"] = os.fdopen(fd, "w")
        pipe_handles["subtitle"].write(text + "\n")
        pipe_handles["subtitle"].flush()
    except Exception as e:
        pipe_handles["subtitle"] = None
        print(f"{CLR_YELLOW}[PY] :: [SUBTITLE PIPE ERR] >> {e}{CLR_RESET}", file=sys.stderr)

# Sends real-time FFT spectrum data to the spectrum pipe
def send_spectrum(freq_data):
    try:
        if pipe_handles["spectrum"] is None:
            fd = os.open(spectrum_pipe_path, os.O_WRONLY | os.O_NONBLOCK)
            pipe_handles["spectrum"] = os.fdopen(fd, "w")
        pipe_handles["spectrum"].write(",".join(f"{x:.4f}" for x in freq_data) + "\n")
        pipe_handles["spectrum"].flush()
    except Exception as e:
        pipe_handles["spectrum"] = None
        print(f"{CLR_YELLOW}[PY] :: [SPECTRUM PIPE ERR] >> {e}{CLR_RESET}", file=sys.stderr)

# Run recognition + spectrum in a worker to keep the audio callback lightweight
def processing_worker(recognizer):
    while True:
        try:
            in_data = audio_queue.get(timeout=1)
        except queue.Empty:
            continue

        # Spectrum
        samples = np.frombuffer(in_data, dtype=np.int16).astype(np.float32) / 32768.0
        fft = np.abs(np.fft.rfft(samples, n=128))[:64]
        fft = np.clip(fft / np.max(fft), 0, 1) if np.max(fft) != 0 else fft
        send_spectrum(fft)

        # Speech recognition section
        if recognizer.AcceptWaveform(in_data):
            result = json.loads(recognizer.Result())
            text = result.get("text", "")
            send_subtitle(text)
            for word in text.split():
                if word in trigger_words:
                    print(f"{CLR_GREEN}[PY] :: [K3YWORD DETECT3D] >> {word}{CLR_RESET}")
                    send_to_pipe(word)
                    break
        else:
            partial = json.loads(recognizer.PartialResult())
            partial_text = partial.get("partial", "")
            if partial_text:
                send_subtitle(partial_text)


# Initializes audio input/output stream with lightweight callback for low latency
def start_streaming_audio(recognizer):
    pa = pyaudio.PyAudio()
    input_rate = 16000
    modulation_freq = 70
    buffer_size = 1024
    phase = 0

    def callback(in_data, frame_count, time_info, status):
        nonlocal phase
        # Keep queue bounded to avoid runaway latency
        try:
            while audio_queue.full():
                audio_queue.get_nowait()
            audio_queue.put_nowait(in_data)
        except queue.Full:
            pass

        # Lightweight ring modulation passthrough for monitoring
        audio = np.frombuffer(in_data, dtype=np.int16).astype(np.float32)
        t = (np.arange(frame_count) + phase) / input_rate
        modulator = 0.5 * (1.0 + np.sin(2 * np.pi * modulation_freq * t))
        audio *= modulator

        phase = (phase + frame_count) % input_rate
        audio = np.clip(audio, -32768, 32767).astype(np.int16)
        return (audio.tobytes(), pyaudio.paContinue)

    stream = pa.open(format=pyaudio.paInt16,
                     channels=1,
                     rate=input_rate,
                     input=True,
                     output=True,
                     frames_per_buffer=buffer_size,
                     stream_callback=callback)
    stream.start_stream()

    # Spawn worker after stream starts so recognizer is ready
    threading.Thread(target=processing_worker, args=(recognizer,), daemon=True).start()
    return stream

# Main loop that sets up pipes, loads model, and starts the audio processing loop
def main():
    if not os.path.exists(pipe_path):
        os.mkfifo(pipe_path)
    if not os.path.exists(spectrum_pipe_path):
        os.mkfifo(spectrum_pipe_path)

    load_trigger_words("animations")

    model = vosk.Model("model")
    recognizer = vosk.KaldiRecognizer(model, 16000)
    stream = start_streaming_audio(recognizer)
    print(f"{CLR_PURPLE}[PY] :: [L1ST3N1NG W1TH R0B0-V0C0D3R]{CLR_RESET}")
    try:
        while True:
            time.sleep(0.1)
    except KeyboardInterrupt:
        print(f"\n{CLR_PINK}[PY] :: [D3T4CH1NG . . . G00DBY3]{CLR_RESET}")
        sys.exit(0)

if __name__ == "__main__":
    main()
