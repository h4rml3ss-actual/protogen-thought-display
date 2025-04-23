import os
import queue
import sounddevice as sd
import vosk
import sys
import json
import threading
import time
import numpy as np

# ANSI color codes for cyberpunk theme
CLR_RESET = "\033[0m"
CLR_PINK = "\033[38;2;255;20;147m"
CLR_PURPLE = "\033[38;2;128;0;128m"
CLR_GREEN = "\033[38;2;0;255;0m"
CLR_CYAN = "\033[38;2;0;255;255m"
CLR_YELLOW = "\033[38;2;255;255;0m"

# Globals
audio_queue = queue.Queue()
trigger_words = set()
recognized_word = None
pipe_path = "/tmp/visor_pipe"
spectrum_pipe_path = "/tmp/visor_spectrum"

# Load trigger words from animation folders
def load_trigger_words(directory="animations"):
    global trigger_words
    try:
        trigger_words = {entry.name for entry in os.scandir(directory) if entry.is_dir()}
        print(f"{CLR_CYAN}[PY] :: [TR1GG3RZ L0ADED] >> {sorted(trigger_words)}{CLR_RESET}")
    except Exception as e:
        print(f"[Python] Failed to load trigger words: {e}", file=sys.stderr)

# Audio callback to queue microphone data
def audio_callback(indata, frames, time_info, status):
    if status:
        print(f"{CLR_YELLOW}[PY] :: [AUDI0 ERR] >> {status}{CLR_RESET}", file=sys.stderr)
    audio_queue.put(bytes(indata))

# Write matched word to pipe
def send_to_pipe(word):
    try:
        with open(pipe_path, "w") as pipe:
            pipe.write(word + "\n")
    except Exception as e:
        print(f"{CLR_YELLOW}[PY] :: [PIPE WRI7E ERR] >> {e}{CLR_RESET}", file=sys.stderr)

def send_subtitle(text):
    try:
        with open("/tmp/visor_subtitles", "w") as pipe:
            pipe.write(text + "\n")
    except Exception as e:
        print(f"{CLR_YELLOW}[PY] :: [SUBTITLE PIPE ERR] >> {e}{CLR_RESET}", file=sys.stderr)

def send_spectrum(freq_data):
    try:
        with open(spectrum_pipe_path, "w") as pipe:
            pipe.write(",".join(f"{x:.4f}" for x in freq_data) + "\n")
    except Exception as e:
        print(f"{CLR_YELLOW}[PY] :: [SPECTRUM PIPE ERR] >> {e}{CLR_RESET}", file=sys.stderr)

# Recognizer loop
def recognize_audio():
    model = vosk.Model("model")
    recognizer = vosk.KaldiRecognizer(model, 44100)

    while True:
        try:
            data = audio_queue.get(timeout=0.1)
            samples = np.frombuffer(data, dtype=np.int16) / 32768.0
            fft = np.abs(np.fft.rfft(samples, n=128))[:64]
            fft = np.clip(fft / np.max(fft), 0, 1) if np.max(fft) != 0 else fft
            send_spectrum(fft)
        except queue.Empty:
            continue

        if recognizer.AcceptWaveform(data):
            result = json.loads(recognizer.Result())
            text = result.get("text", "")
            send_subtitle(text)
            words = text.split()

            for word in words:
                if word in trigger_words:
                    print(f"{CLR_GREEN}[PY] :: [K3YWORD DETECT3D] >> {word}{CLR_RESET}")
                    send_to_pipe(word)
                    break
        else:
            # Optional: print partial result
            pass

def main():
    if not os.path.exists(pipe_path):
        os.mkfifo(pipe_path)
    if not os.path.exists(spectrum_pipe_path):
        os.mkfifo(spectrum_pipe_path)

    load_trigger_words("animations")

    with sd.RawInputStream(
        samplerate=44100,
        blocksize=4000,
        dtype="int16",
        channels=1,
        callback=audio_callback,
        device=0  # <-- use USB Audio Device
    ):
        thread = threading.Thread(target=recognize_audio, daemon=True)
        thread.start()
        print(f"{CLR_PURPLE}[PY] :: [L1ST3N1NG F0R S33CH...] Ctrl+C = D3TACH{CLR_RESET}")
        thread.join()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n{CLR_PINK}[PY] :: [D3T4CH1NG . . . G00DBY3]{CLR_RESET}")
        sys.exit(0)
