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

    // Glitch font is now always FONT_HERSHEY_DUPLEX
    std::vector<cv::Scalar> neonColors = {
        cv::Scalar(255, 20, 147),  // pink
        cv::Scalar(128, 0, 128),   // purple
        cv::Scalar(0, 255, 0),     // green
        cv::Scalar(255, 255, 0),   // yellow
        cv::Scalar(0, 255, 255)    // cyan
    };

    size_t messageIndex = 0;
    auto lastMessageTime = Clock::now();
    std::mt19937 rng(std::random_device{}());
    // Progressive halving message interval system for quirky messages
    std::chrono::milliseconds baseMessageInterval(10000); // 10s
    std::chrono::milliseconds currentMessageInterval = baseMessageInterval;
    const std::chrono::milliseconds minMessageInterval(1000); // 1s minimum

    // Create a persistent fullscreen window and black frame
    static cv::Mat frame(720, 1280, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::namedWindow("SubtitleOverlay", cv::WINDOW_NORMAL);
    cv::setWindowProperty("SubtitleOverlay", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);

    // Glitch management variables
    std::vector<std::chrono::milliseconds> glitchIntervals = {
        std::chrono::milliseconds(10000),
        std::chrono::milliseconds(5000),
        std::chrono::milliseconds(2500),
        std::chrono::milliseconds(1250),
        std::chrono::milliseconds(750)
    };
    size_t currentGlitchStage = 0;
    std::chrono::milliseconds glitchInterval = glitchIntervals[currentGlitchStage];
    auto lastGlitchSpawn = Clock::now();
    struct GlitchMessage {
        std::string text;
        int font;
        double fontScale;
        int thickness;
        cv::Scalar color;
        cv::Point basePos;
        Clock::time_point spawnTime;
        float lifetimeSec;
    };
    std::vector<GlitchMessage> activeGlitches;
    // For robust startup delay on random glitches
    auto glitchStartupTime = Clock::now();
    bool glitchStartupDelayPassed = false;

    static size_t currentLine = 0; // Tracks current subtitle line being rendered

    // Main event loop
    while (keepRunning.load()) {
        ssize_t bytesRead = read(pipeFd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';  // null-terminate
            std::string keyword(buffer);
            keyword.erase(keyword.find_last_not_of(" \n\r\t") + 1);  // trim trailing whitespace

            std::cout << CLR_GREEN << "[Main] :: [K3YWORD ACQUIRED] >> " << keyword << CLR_RESET << std::endl;
            animationManager.playAnimation(keyword);
            lastAnimationTime = Clock::now();
            // Reset glitch timing if user activity detected
            currentMessageInterval = baseMessageInterval;
        }

        // Read subtitle pipe
        char subtitleBuf[512];
        ssize_t subRead = read(subtitleFd, subtitleBuf, sizeof(subtitleBuf) - 1);
        if (subRead > 0) {
            subtitleBuf[subRead] = '\0';
            std::string newText = std::string(subtitleBuf);
            newText.erase(newText.find_last_not_of(" \n\r\t") + 1);
            static std::string lastSubtitleText;
            if (newText != subtitleText) {
                subtitleText = newText;
                lastSubtitleText = newText;
                lastSubtitleTime = Clock::now();
                currentLine = 0; // Reset line animation when subtitle changes
                // Reset glitch timer and interval progression to initial state when subtitle arrives
                currentGlitchStage = 0;
                glitchInterval = glitchIntervals[currentGlitchStage];
                lastGlitchSpawn = Clock::now();
            }
        }

        // --- Draw order: clear, spectrum, then subtitles, then glitches ---
        // 1. Clear the frame to black each frame before drawing
        frame.setTo(cv::Scalar(0, 0, 0));

        // 2. Prepare audio spectrum buffer for FFT integration
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
        } else if (spectrumRead <= 0) {
            // If no new spectrum data, decay the spectrum slowly
            for (int i = 0; i < 64; ++i) {
                spectrum[i] *= 0.9f;
            }
        }

        // 3. Draw the audio visualizer spectrum
        const int numBars = 64;
        const float radius = 200.0f;
        const float maxBarLength = 100.0f;
        const cv::Point center(frame.cols / 2, frame.rows / 2);
        float angleStep = 2 * CV_PI / numBars;

        for (int i = 0; i < numBars; ++i) {
            float angle = i * angleStep;
            float amplitude = spectrum[i];
            float len = amplitude * maxBarLength;
            cv::Point p1(center.x + std::cos(angle) * radius,
                         center.y + std::sin(angle) * radius);
            cv::Point p2(center.x + std::cos(angle) * (radius + len),
                         center.y + std::sin(angle) * (radius + len));
            cv::line(frame, p1, p2, cv::Scalar(0, 255 - i * 4, 0), 2, cv::LINE_AA);
        }

        // 4. Subtitle line drawing (after spectrum)
        if (!subtitleText.empty() && Clock::now() - lastSubtitleTime < subtitleDisplayTime) {
            std::vector<std::string> lines;
            std::istringstream iss(subtitleText);
            std::string word;
            std::string line;
            int wordsPerLine = 3;
            int count = 0;

            while (iss >> word) {
                if (!line.empty()) line += " ";
                line += word;
                count++;
                if (count >= wordsPerLine) {
                    lines.push_back(line);
                    line.clear();
                    count = 0;
                }
            }
            if (!line.empty()) lines.push_back(line);

            static auto lastLineUpdate = Clock::now();
            const std::chrono::milliseconds lineDelay(100);

            if (currentLine < lines.size() && Clock::now() - lastLineUpdate >= lineDelay) {
                currentLine++;
                lastLineUpdate = Clock::now();
            }

            int totalHeight = lines.size() * 80;
            int y = (frame.rows - totalHeight) / 2 + 60;
            for (size_t i = 0; i < currentLine && i < lines.size(); ++i) {
                const auto& ln = lines[i];
                int baseline = 0;
                cv::Size textSize = cv::getTextSize(ln, cv::FONT_HERSHEY_DUPLEX, 2.5, 5, &baseline);
                int x = (frame.cols - textSize.width) / 2;

                cv::putText(frame, ln, cv::Point(x, y), cv::FONT_HERSHEY_DUPLEX, 2.5, cv::Scalar(0, 64, 0), 10);
                cv::putText(frame, ln, cv::Point(x, y), cv::FONT_HERSHEY_DUPLEX, 2.5, cv::Scalar(0, 128, 0), 6);
                cv::putText(frame, ln, cv::Point(x, y), cv::FONT_HERSHEY_DUPLEX, 2.5, cv::Scalar(0, 255, 0), 3);
                y += 80;
            }

            if (Clock::now() - lastSubtitleTime > subtitleDisplayTime) {
                currentLine = 0;
            }
        }

        // --- Glitch drawing and spawning (draw after subtitles) ---
        auto now = Clock::now();
        // Remove expired glitches
        activeGlitches.erase(
            std::remove_if(activeGlitches.begin(), activeGlitches.end(), [&](const GlitchMessage& g) {
                return std::chrono::duration<float>(now - g.spawnTime).count() > g.lifetimeSec;
            }),
            activeGlitches.end()
        );
        // Draw glitches with fading and jitter/trails (limit trails to 2 for perf)
        for (const auto& g : activeGlitches) {
            float age = std::chrono::duration<float>(now - g.spawnTime).count();
            float alpha = std::max(0.0f, 1.0f - age / g.lifetimeSec);
            // Enhanced flicker: keep minimum brightness higher
            float flicker = 0.8f + 0.2f * std::sin(age * 80.0f);
            float finalAlpha = alpha * flicker;
            float jitterX = std::sin(age * 20.0f) * 4.0f + (rng() % 3 - 1);
            float jitterY = std::cos(age * 25.0f) * 4.0f + (rng() % 3 - 1);
            for (int trail = 0; trail < 2; ++trail) {
                float trailAlpha = finalAlpha * (1.0f - trail * 0.3f);
                float trailJitterX = jitterX + (rng() % 7 - 3); // jitter range -3 to +3 px
                float trailJitterY = jitterY + (rng() % 7 - 3);
                // Boost trail color brightness for vivid effect
                cv::Scalar boostedColor = g.color * (trailAlpha * 1.2f);
                boostedColor[0] = std::min(boostedColor[0], 255.0);
                boostedColor[1] = std::min(boostedColor[1], 255.0);
                boostedColor[2] = std::min(boostedColor[2], 255.0);
                cv::Point trailPos(g.basePos.x + trailJitterX, g.basePos.y + trailJitterY);
                // Guard final draw position
                if (trailPos.x >= 0 && trailPos.x < frame.cols - 50 &&
                    trailPos.y >= 0 && trailPos.y < frame.rows - 50) {
                    cv::putText(frame, g.text, trailPos, g.font, g.fontScale, boostedColor, g.thickness, cv::LINE_AA);
                }
            }
        }
        // Glitch startup delay (5s after program start)
        if (!glitchStartupDelayPassed) {
            if (now - glitchStartupTime >= std::chrono::seconds(5)) {
                glitchStartupDelayPassed = true;
                lastGlitchSpawn = now;
            }
        }
        // --- Robust glitch spawning: independent from TRACE messages ---
        // Only spawn a random glitch if:
        //  - at least 5s have passed since startup
        //  - no subtitle is visible
        //  - glitchInterval timer elapsed
        if (glitchStartupDelayPassed &&
            (subtitleText.empty() || now - lastSubtitleTime > subtitleDisplayTime)) {
            if (now - lastGlitchSpawn >= glitchInterval) {
                lastGlitchSpawn = now;
                // Progress glitch interval stage if possible
                if (currentGlitchStage < glitchIntervals.size() - 1)
                    currentGlitchStage++;
                glitchInterval = glitchIntervals[currentGlitchStage];
                // Clear old glitches before spawning
                activeGlitches.clear();
                // Spawn new random glitch message
                std::string glitchText = quirkyMessages[rng() % quirkyMessages.size()];
                int font = cv::FONT_HERSHEY_DUPLEX;
                double fontScale = 1.0 + (rng() % 200) / 100.0; // range 1.0 - 3.0 max
                int thickness = 1 + (rng() % 4);
                cv::Scalar color = neonColors[rng() % neonColors.size()];
                int x = 50 + (rng() % (frame.cols - 100)); // 50px margin
                int y = 50 + (rng() % (frame.rows - 100));
                GlitchMessage glitch = {
                    glitchText,
                    font,
                    fontScale,
                    thickness,
                    color,
                    cv::Point(x, y),
                    now,
                    3.0f
                };
                activeGlitches.push_back(glitch);
            }
        }
        // If a subtitle appears, force-reset glitch timing for next random glitch
        if (!subtitleText.empty() && now - lastSubtitleTime < subtitleDisplayTime) {
            currentGlitchStage = 0;
            glitchInterval = glitchIntervals[currentGlitchStage];
            lastGlitchSpawn = now;
            // Also clear glitches if desired (for clean state)
            // activeGlitches.clear();
        }

        // Show frame and handle key input after all drawing
        cv::imshow("SubtitleOverlay", frame);
        int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q') {
            std::cout << CLR_PINK << "[Main] :: [Q detected via window -- disengaging interface]" << CLR_RESET << std::endl;
            keepRunning = false;
        }

        // Idle animation and quirky messages (unchanged)
        if (Clock::now() - lastAnimationTime >= idleThreshold) {
            std::cout << CLR_YELLOW << "[Main] :: [SYS.IDLE > 30s] -- TR1GGERING 1DL3 ANIM" << CLR_RESET << std::endl;
            animationManager.playAnimation("idle");
            lastAnimationTime = Clock::now();
        }

        // --- TRACE quirky message logic (completely independent from glitch spawning) ---
        if (Clock::now() - lastSubtitleTime >= currentMessageInterval && Clock::now() - lastMessageTime >= currentMessageInterval) {
            std::cout << CLR_PINK << "[TRACE] " << quirkyMessages[messageIndex] << CLR_RESET << std::endl;
            // Also spawn visually (with glitch effect, clear previous)
            std::string glitchText = quirkyMessages[messageIndex];
            int font = cv::FONT_HERSHEY_DUPLEX;
            double fontScale = 1.0 + (rng() % 200) / 100.0; // range 1.0 - 3.0 max
            int thickness = 1 + (rng() % 4);
            cv::Scalar color = neonColors[rng() % neonColors.size()];
            int x = 50 + (rng() % (frame.cols - 100)); // 50px margin
            int y = 50 + (rng() % (frame.rows - 100));
            GlitchMessage glitch = {
                glitchText,
                font,
                fontScale,
                thickness,
                color,
                cv::Point(x, y),
                Clock::now(),
                3.0f
            };
            // Clear old glitch immediately for message-triggered glitches too
            activeGlitches.clear();
            activeGlitches.push_back(glitch);
            messageIndex = (messageIndex + 1) % quirkyMessages.size();
            lastMessageTime = Clock::now();
            currentMessageInterval = std::max(minMessageInterval, currentMessageInterval / 2);
            // Reset random glitch spawn timing and interval progression cleanly to avoid race with TRACE
            currentGlitchStage = 0;
            glitchInterval = glitchIntervals[currentGlitchStage];
            lastGlitchSpawn = Clock::now();
        }

        // Sleep at end of loop to cap FPS (~30 FPS = 33ms per frame)
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
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