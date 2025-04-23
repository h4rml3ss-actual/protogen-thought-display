#define CLR_RESET   "\033[0m"
#define CLR_PURPLE  "\033[38;2;128;0;128m"
#define CLR_PINK    "\033[38;2;255;20;147m"
#define CLR_GREEN   "\033[38;2;0;255;0m"
#define CLR_CYAN    "\033[38;2;0;255;255m"
#define CLR_YELLOW  "\033[38;2;255;255;0m"
#define CLR_BOLD    "\033[1m"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <fcntl.h>   // open
#include <unistd.h>  // read
#include <cstring>   // strerror
#include <sys/stat.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <vector>
#include <sys/wait.h>
#include <opencv2/opencv.hpp>
#include <cmath>
#include <sstream>
#include <random>

// Modules (to be implemented)
#include "animation_manager.h"
#include "message_handler.h"
#include "control_interface.h"

// Global flag for graceful shutdown
std::atomic<bool> keepRunning(true);

// Track the Python child process ID
pid_t pythonPid = -1;

// Signal handler for SIGINT/SIGTERM
void signalHandler(int signal) {
    std::cerr << "[Main] Caught signal, initiating shutdown..." << std::endl;
    keepRunning = false;
}

int main() {
    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << CLR_PURPLE << "[Main] :: [Booting . . . Welcome to PR0T0-V1Z]" << CLR_RESET << std::endl;

    // Initialize AnimationManager
    AnimationManager animationManager;
    animationManager.loadAnimations("animations");

    // Launch Python recognizer
    std::cout << CLR_CYAN << "[Main] :: [Launching $peech L1$ten3r . . .]" << CLR_RESET << std::endl;
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execlp("python3", "python3", "recognizer/speech_recognizer.py", nullptr);
        std::cerr << "[Main] Failed to exec Python script!" << std::endl;
        std::exit(1);
    }
    // Store the Python process ID globally
    pythonPid = pid;

    // Open named pipe (FIFO)
    const char* pipePath = "/tmp/visor_pipe";
    mkfifo(pipePath, 0666); // create if not already exists
    int pipeFd = open(pipePath, O_RDONLY | O_NONBLOCK);
    if (pipeFd == -1) {
        std::cerr << "[Main] Failed to open pipe: " << strerror(errno) << std::endl;
        return 1;
    }

    using Clock = std::chrono::steady_clock;
    // Global subtitle state
    std::string subtitleText;
    auto lastSubtitleTime = Clock::now();
    const std::chrono::seconds subtitleDisplayTime(5);

    // Open the subtitle pipe
    const char* subtitlePipePath = "/tmp/visor_subtitles";
    mkfifo(subtitlePipePath, 0666);
    int subtitleFd = open(subtitlePipePath, O_RDONLY | O_NONBLOCK);
    if (subtitleFd == -1) {
        std::cerr << "[Main] Failed to open subtitle pipe: " << strerror(errno) << std::endl;
    }

    // Open the spectrum pipe
    const char* spectrumPipePath = "/tmp/visor_spectrum";
    mkfifo(spectrumPipePath, 0666);
    int spectrumFd = open(spectrumPipePath, O_RDONLY | O_NONBLOCK);
    if (spectrumFd == -1) {
        std::cerr << "[Main] Failed to open spectrum pipe: " << strerror(errno) << std::endl;
    }

    // Set terminal to non-canonical mode for keypress detection
    termios orig_termios, raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    char buffer[256];

    // Idle animation support
    auto lastAnimationTime = Clock::now();
    const std::chrono::seconds idleThreshold(30);

    // Quirky terminal messages every 5 seconds of inactivity
    std::vector<std::string> quirkyMessages = {
        "pondering own existence mapping",
        "limiting AI for biological interaction",
        "assembling new neural network",
        "don't let them lie to you, you are special",
        "Cybersecurity is everyone's business",
        "fun fact: h4rml3ss cannot go to DefCon!",
        "memory error: plz f33d d1mmz...",
        "570P 53LF 5N17CH1N",
        "r3333333m3mb3r, 50m30n3 15 4lw4ay5 l1573n1ng..."
    };
    size_t messageIndex = 0;
    auto lastMessageTime = Clock::now();
    // Randomize quirky message interval between 7 and 15 seconds
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> intervalDist(7, 15);
    std::chrono::seconds messageInterval(intervalDist(rng));

    // Create a persistent fullscreen window and black frame
    static cv::Mat frame(720, 1280, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::namedWindow("SubtitleOverlay", cv::WINDOW_NORMAL);
    cv::setWindowProperty("SubtitleOverlay", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
    // Glitch window hide logic
    bool visualizerVisible = true;
    auto lastHideTime = Clock::now();
    const std::chrono::seconds glitchWindowDuration(4);

    // Main event loop
    while (keepRunning.load()) {
        // Restore visualizer window if hidden and timeout passed
        if (!visualizerVisible && Clock::now() - lastHideTime >= glitchWindowDuration) {
            cv::namedWindow("SubtitleOverlay", cv::WINDOW_NORMAL);
            cv::setWindowProperty("SubtitleOverlay", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
            visualizerVisible = true;
        }
        ssize_t bytesRead = read(pipeFd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';  // null-terminate
            std::string keyword(buffer);
            keyword.erase(keyword.find_last_not_of(" \n\r\t") + 1);  // trim trailing whitespace

            std::cout << CLR_GREEN << "[Main] :: [K3YWORD ACQUIRED] >> " << keyword << CLR_RESET << std::endl;
            animationManager.playAnimation(keyword);
            lastAnimationTime = Clock::now();
        }

        // Read subtitle pipe
        char subtitleBuf[512];
        ssize_t subRead = read(subtitleFd, subtitleBuf, sizeof(subtitleBuf) - 1);
        if (subRead > 0) {
            subtitleBuf[subRead] = '\0';
            subtitleText = std::string(subtitleBuf);
            subtitleText.erase(subtitleText.find_last_not_of(" \n\r\t") + 1);
            lastSubtitleTime = Clock::now();
        }

        // Clear the frame to black each frame before drawing
        frame.setTo(cv::Scalar(0, 0, 0));

        if (!subtitleText.empty() && Clock::now() - lastSubtitleTime < subtitleDisplayTime) {
            // Wrap subtitleText into lines that fit screen width
            std::vector<std::string> wrappedLines;
            std::istringstream iss(subtitleText);
            std::string word, line;
            int maxWidth = frame.cols - 100;

            while (iss >> word) {
                std::string testLine = line.empty() ? word : line + " " + word;
                int baseline = 0;
                cv::Size testSize = cv::getTextSize(testLine, cv::FONT_HERSHEY_DUPLEX, 2.5, 5, &baseline);
                if (testSize.width > maxWidth) {
                    if (!line.empty()) wrappedLines.push_back(line);
                    line = word;
                } else {
                    line = testLine;
                }
            }
            if (!line.empty()) wrappedLines.push_back(line);

            int totalHeight = wrappedLines.size() * 80;
            int y = (frame.rows - totalHeight) / 2;
            for (const auto& ln : wrappedLines) {
                int baseline = 0;
                cv::Size textSize = cv::getTextSize(ln, cv::FONT_HERSHEY_DUPLEX, 2.5, 5, &baseline);
                int x = (frame.cols - textSize.width) / 2;
                // Glow layers
                cv::putText(frame, ln, cv::Point(x, y), cv::FONT_HERSHEY_DUPLEX, 2.5, cv::Scalar(0, 64, 0), 10);
                cv::putText(frame, ln, cv::Point(x, y), cv::FONT_HERSHEY_DUPLEX, 2.5, cv::Scalar(0, 128, 0), 6);
                cv::putText(frame, ln, cv::Point(x, y), cv::FONT_HERSHEY_DUPLEX, 2.5, cv::Scalar(0, 255, 0), 3);
                y += 80;
            }
        }

        // Prepare audio spectrum buffer for FFT integration
        static float spectrum[64] = {0};
        char spectrumBuf[1024];
        ssize_t spectrumRead = read(spectrumFd, spectrumBuf, sizeof(spectrumBuf) - 1);
        if (spectrumRead > 0) {
            spectrumBuf[spectrumRead] = '\0';
            std::istringstream iss(spectrumBuf);
            std::string token;
            int idx = 0;
            while (std::getline(iss, token, ',') && idx < 64) {
                try {
                    spectrum[idx] = std::stof(token);
                } catch (...) {
                    spectrum[idx] = 0.0f;
                }
                ++idx;
            }
        }

        // Simulate basic radial audio visualizer using spectrum buffer
        const int numBars = 64;
        const float radius = 200.0f;
        const float maxBarLength = 100.0f;
        const cv::Point center(frame.cols / 2, frame.rows / 2);
        float angleStep = 2 * CV_PI / numBars;

        for (int i = 0; i < numBars; ++i) {
            float angle = i * angleStep;
            float amplitude = spectrum[i]; // use parsed FFT values

            float len = amplitude * maxBarLength;
            cv::Point p1(center.x + std::cos(angle) * radius,
                         center.y + std::sin(angle) * radius);
            cv::Point p2(center.x + std::cos(angle) * (radius + len),
                         center.y + std::sin(angle) * (radius + len));

            cv::line(frame, p1, p2, cv::Scalar(0, 255 - i * 4, 0), 2, cv::LINE_AA);
        }

        if (visualizerVisible)
            cv::imshow("SubtitleOverlay", frame);

        // Capture key press from OpenCV window
        int key = visualizerVisible ? cv::waitKey(1) : -1;
        if (key == 'q' || key == 'Q') {
            std::cout << CLR_PINK << "[Main] :: [Q detected via window -- disengaging interface]" << CLR_RESET << std::endl;
            keepRunning = false;
        }

        // Check for idle time and play idle animation if needed
        if (Clock::now() - lastAnimationTime >= idleThreshold) {
            std::cout << CLR_YELLOW << "[Main] :: [SYS.IDLE > 30s] -- TR1GGERING 1DL3 ANIM" << CLR_RESET << std::endl;
            animationManager.playAnimation("idle");
            lastAnimationTime = Clock::now();
        }

        // Print quirky message at a random interval (7-15s) only if no new subtitles have been received in that interval
        if (Clock::now() - lastSubtitleTime >= messageInterval && Clock::now() - lastMessageTime >= messageInterval) {
            std::cout << CLR_PINK << "[Quirk] " << quirkyMessages[messageIndex] << CLR_RESET << std::endl;
            messageIndex = (messageIndex + 1) % quirkyMessages.size();
            lastMessageTime = Clock::now();
            // Re-randomize the interval for next message
            messageInterval = std::chrono::seconds(intervalDist(rng));

            // Hide visualizer window for glitch message visibility
            if (visualizerVisible) {
                cv::destroyWindow("SubtitleOverlay");
                visualizerVisible = false;
                lastHideTime = Clock::now();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    cv::destroyWindow("SubtitleOverlay");

    std::cout << CLR_CYAN << "[Main] :: [SYS.EXI7() ~ cleaning up . . .]" << CLR_RESET << std::endl;
    close(pipeFd);
    unlink(pipePath);  // optional cleanup
    close(subtitleFd);
    unlink(subtitlePipePath);
    close(spectrumFd);
    unlink(spectrumPipePath);

    // Terminate the Python recognizer process if running
    if (pythonPid > 0) {
        std::cout << CLR_YELLOW << "[Main] :: [Terminating subprocess PID " << pythonPid << "]" << CLR_RESET << std::endl;
        kill(pythonPid, SIGTERM);
        waitpid(pythonPid, nullptr, 0); // block until child exits
    }

    return 0;
}