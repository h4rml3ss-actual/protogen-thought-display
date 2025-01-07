import sys
import queue
import json
import threading
import sounddevice as sd
import vosk
import os
import time  # For sleep intervals
import subprocess

# Define logging levels
LOG_LEVELS = {"debug": 3, "normal": 2, "silent": 1}
current_log_level = LOG_LEVELS["normal"]  # Default log level

# ANSI color codes for vaporwave aesthetic
RESET = "\033[0m"
NEON_PINK = "\033[38;2;255;20;147m"
NEON_CYAN = "\033[38;2;0;255;255m"
NEON_PURPLE = "\033[38;2;128;0;128m"
NEON_GREEN = "\033[38;2;0;255;0m"
NEON_YELLOW = "\033[38;2;255;255;0m"

# Logging helper function with color
def log(message, level="debug", color=RESET):
    if LOG_LEVELS[level] <= current_log_level:
        print(f"{color}{message}{RESET}", file=sys.stderr)

# Define the queue for audio processing and initialize variables
audio_queue = queue.Queue()
trigger_words = set()  # This will be dynamically populated
recognized_word = None
word_lock = threading.Lock()

# Function to dynamically load trigger words based on folders
def load_trigger_words(directory="animations"):
    global trigger_words
    try:
        trigger_words = {entry.name for entry in os.scandir(directory) if entry.is_dir()}
        log(f"Loaded trigger words: {trigger_words}", level="normal", color=NEON_CYAN)
    except Exception as e:
        log(f"Error loading trigger words: {e}", level="debug", color=NEON_PINK)

# Audio callback function
def audio_callback(indata, frames, time, status):
    if status:
        log(f"# Audio Status: {status}", level="debug", color=NEON_YELLOW)
    else:
        log(f"# Captured {frames} frames.", level="debug", color=NEON_GREEN)
    audio_queue.put(bytes(indata))

# Function to process audio and recognize words
def process_audio():
    global recognized_word
    try:
        # Load the Vosk model
        model = vosk.Model("/home/operator/visor/model")

        recognizer = vosk.KaldiRecognizer(model, 16000)

        # Notify C++ application that the recognizer is ready
        print("ready", flush=True)
        log("Recognizer is ready. Sent 'ready' signal.", level="normal", color=NEON_GREEN)

        while True:
            try:
                data = audio_queue.get(timeout=0.1)  # Timeout to avoid infinite blocking
            except queue.Empty:
                time.sleep(0.01)  # Sleep briefly to reduce CPU usage
                continue

            if recognizer.AcceptWaveform(data):
                result_json = recognizer.Result()
                result_dict = json.loads(result_json)
                text = result_dict.get("text", "")

                log(f"[DEBUG] Recognized text: {text}", level="debug", color=NEON_PURPLE)

                # Tokenize the recognized text and check for trigger words
                words = text.split()
                matching_words = [word for word in trigger_words if word in words]
                if matching_words:
                    with word_lock:
                        recognized_word = matching_words[0]
                        log(f"[DEBUG] Trigger word detected: {recognized_word}", level="normal", color=NEON_PINK)
            else:
                log("[DEBUG] Partial result ignored.", level="debug", color=NEON_YELLOW)
    except Exception as e:
        log(f"Error in process_audio: {e}", level="normal", color=NEON_PINK)

# Function to send the recognized word to the C++ application
def send_recognized_word():
    global recognized_word

    while True:
        with word_lock:
            if recognized_word:
                print(recognized_word, flush=True)
                recognized_word = None  # Clear the recognized word
        time.sleep(0.01)  # Allow other threads to run

# Main function to manage threads
def main():
    global current_log_level
    try:
        # Allow setting logging level via an environment variable or user input
        log_level = os.getenv("LOG_LEVEL", "normal").lower()
        if log_level in LOG_LEVELS:
            current_log_level = LOG_LEVELS[log_level]
        else:
            log("Invalid LOG_LEVEL specified. Falling back to 'normal'.", level="normal", color=NEON_YELLOW)

        log(f"Starting application with log level: {log_level}", level="normal", color=NEON_CYAN)

        # Dynamically load trigger words based on folders in the animations directory
        load_trigger_words("animations")

        # Start the audio stream
        with sd.RawInputStream(
            samplerate=16000,
            blocksize=4000,  # Keep block size balanced for performance
            dtype="int16",
            channels=1,
            callback=audio_callback
        ):
            # Create and start threads for audio processing and word sending
            audio_thread = threading.Thread(target=process_audio, daemon=True)
            word_thread = threading.Thread(target=send_recognized_word, daemon=True)

            audio_thread.start()
            word_thread.start()

            # Keep the main thread alive
            audio_thread.join()
            word_thread.join()
    except KeyboardInterrupt:
        log("Exiting on user interrupt.", level="normal", color=NEON_GREEN)
        sys.exit(0)
    except Exception as e:
        log(f"Unexpected error: {e}", level="normal", color=NEON_PINK)
        sys.exit(1)

if __name__ == "__main__":
    main()
