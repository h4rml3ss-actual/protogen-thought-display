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
    recognize_audio.last_partial = ""
    last_partial_time = time.time()

    while True:
        try:
            data = audio_queue.get(timeout=0.1)
            samples = np.frombuffer(data, dtype=np.int16) / 32768.0
            fft = np.abs(np.fft.rfft(samples, n=128))[:64]
            fft = np.clip(fft / np.max(fft), 0, 1) if np.max(fft) != 0 else fft
            send_spectrum(fft)
        except queue.Empty:
            # Send a soft idle spectrum when no new audio is detected
            idle_fft = np.full(64, 0.02)
            send_spectrum(idle_fft)

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
            partial = json.loads(recognizer.PartialResult())
            partial_text = partial.get("partial", "")
            now = time.time()
            if partial_text and partial_text != recognize_audio.last_partial and now - last_partial_time > 0.15:
                recognize_audio.last_partial = partial_text
                last_partial_time = now
                send_subtitle(partial_text)

def main():
    if not os.path.exists(pipe_path):
        os.mkfifo(pipe_path)
    if not os.path.exists(spectrum_pipe_path):
        os.mkfifo(spectrum_pipe_path)

    load_trigger_words("animations")

    # Find a valid input device automatically
    device_index = None
    for idx, dev in enumerate(sd.query_devices()):
        if dev['max_input_channels'] > 0:
            device_index = idx
            break

    if device_index is None:
        print(f"{CLR_YELLOW}[PY] :: [ERR] No input device found!{CLR_RESET}")
        sys.exit(1)

    device_info = sd.query_devices(device_index)
    channels = device_info['max_input_channels']

    with sd.RawInputStream(
        samplerate=44100,
        blocksize=4000,
        dtype="int16",
        channels=channels,
        callback=audio_callback,
        device=device_index
    ) as stream:
        try:
            thread = threading.Thread(target=recognize_audio, daemon=True)
            thread.start()
            print(f"{CLR_PURPLE}[PY] :: [L1ST3N1NG F0R S33CH...] Ctrl+C = D3TACH{CLR_RESET}")
            thread.join()
        finally:
            stream.stop()
            stream.close()
            print(f"{CLR_PINK}[PY] :: [AUDI0 STR34M CL0S3D]{CLR_RESET}")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n{CLR_PINK}[PY] :: [D3T4CH1NG . . . G00DBY3]{CLR_RESET}")
        sys.exit(0)
