#include <iostream>
#include <cstdio>
#include <string>
#include <cstdlib>      // EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <random>
#include <thread>
#include <mutex>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <csignal>
#include <atomic>
#include <sys/types.h>
#include <unistd.h>
#include <queue>
#include <sys/wait.h>   // waitpid()
#include <fcntl.h>      // fcntl(), F_SETFL, O_NONBLOCK
#include <poll.h>       // poll()

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// ANSI color codes
// -----------------------------------------------------------------------------
const std::string RESET       = "\033[0m";
const std::string NEON_PINK   = "\033[38;2;255;20;147m";
const std::string NEON_CYAN   = "\033[38;2;0;255;255m";
const std::string NEON_PURPLE = "\033[38;2;128;0;128m";
const std::string NEON_YELLOW = "\033[38;2;255;255;0m";
const std::string NEON_GREEN  = "\033[38;2;0;255;0m";

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------
std::unordered_map<std::string, std::vector<std::string>> animationCache;
std::mutex animationMutex;

std::mutex queueMutex;
std::queue<std::string> recognizedQueue;

std::atomic<bool> keepRunning(true);

// We'll store the child's PID so we can kill it if needed
pid_t pythonPid = -1;

// We'll store our read-end of the pipe in a global for the thread
int pythonReadFD = -1;

// -----------------------------------------------------------------------------
// Signal handler
// -----------------------------------------------------------------------------
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cerr << NEON_YELLOW 
                  << "[C++] Caught signal. Requesting graceful shutdown..." 
                  << RESET << std::endl;
        keepRunning = false;
    }
}

// -----------------------------------------------------------------------------
// Helper: Preload animations
// -----------------------------------------------------------------------------
std::vector<std::string> getFilesInDirectory(const std::string& directoryPath) {
    std::vector<std::string> files;
    try {
        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gif") {
                files.push_back(entry.path().string());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << NEON_PINK << "[C++] Error accessing directory: " << e.what() 
                  << RESET << std::endl;
    }
    return files;
}

void preloadAnimations(const std::string& basePath) {
    try {
        std::random_device rd;
        std::mt19937 g(rd());

        for (const auto& entry : fs::directory_iterator(basePath)) {
            if (entry.is_directory()) {
                std::string triggerWord = entry.path().filename().string();
                std::vector<std::string> files = getFilesInDirectory(entry.path().string());

                std::shuffle(files.begin(), files.end(), g);

                std::lock_guard<std::mutex> lock(animationMutex);
                animationCache[triggerWord] = files;
                std::cout << NEON_GREEN << "[C++] Preloaded animations for: " 
                          << triggerWord << RESET << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << NEON_PINK << "[C++] Error preloading animations: " << e.what() 
                  << RESET << std::endl;
    }
}

// -----------------------------------------------------------------------------
// Helper: Play an animation in a detached thread
// -----------------------------------------------------------------------------
void playAnimation(const std::string& animationPath) {
    // Update lastConsoleOutputTime whenever we print
    static auto lastConsoleOutputTime = std::chrono::steady_clock::now();

    std::cout << NEON_PURPLE << "[C++] Playing animation: " << animationPath << RESET 
              << std::endl;
    lastConsoleOutputTime = std::chrono::steady_clock::now();

    std::thread([animationPath]() {
        std::ostringstream mpvCommand;
        mpvCommand << "mpv --fs --loop-file=no --no-terminal --no-audio \""
                   << animationPath << "\"";

        int result = std::system(mpvCommand.str().c_str());
        if (result != 0) {
            std::cerr << NEON_PINK << "[C++] Error playing animation: " 
                      << animationPath << RESET << std::endl;
        }
    }).detach();
}

// -----------------------------------------------------------------------------
// Launch Python manually (fork/exec) with an unbuffered '-u' flag
// -----------------------------------------------------------------------------
bool launchPythonProcess(const std::string& scriptPath) {
    int fd[2];
    if (pipe(fd) < 0) {
        std::cerr << NEON_PINK << "[C++] pipe() failed." << RESET << std::endl;
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << NEON_PINK << "[C++] fork() failed." << RESET << std::endl;
        return false;
    }
    else if (pid == 0) {
        // Child process
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);

        execlp("python3", 
               "python3", 
               "-u", 
               scriptPath.c_str(),
               nullptr);

        std::cerr << "[C++] execlp() failed.\n";
        _exit(1);
    }
    else {
        // Parent process
        pythonPid = pid;
        std::cout << NEON_CYAN 
                  << "[C++] Launched Python script with PID: " << pid 
                  << RESET << std::endl;

        close(fd[1]);
        int flags = fcntl(fd[0], F_GETFL, 0);
        fcntl(fd[0], F_SETFL, flags | O_NONBLOCK);

        pythonReadFD = fd[0];
        return true;
    }
    return false; // shouldn't reach
}

// -----------------------------------------------------------------------------
// Thread: Poll the pythonReadFD for data, parse lines, push to recognizedQueue
// -----------------------------------------------------------------------------
void pythonReaderThreadFunc() {
    std::string bufferAccum;
    const int POLL_TIMEOUT_MS = 100; // 0.1s
    char readBuf[256];

    while (keepRunning.load()) {
        struct pollfd pfd;
        pfd.fd = pythonReadFD;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << NEON_PINK 
                      << "[C++] poll() error. Errno=" << errno 
                      << RESET << std::endl;
            break;
        }
        else if (ret == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            ssize_t bytesRead = read(pythonReadFD, readBuf, sizeof(readBuf));
            if (bytesRead > 0) {
                bufferAccum.append(readBuf, bytesRead);
                size_t pos = 0;
                while (true) {
                    size_t newlinePos = bufferAccum.find('\n', pos);
                    if (newlinePos == std::string::npos) {
                        if (pos > 0) {
                            bufferAccum.erase(0, pos);
                        }
                        break;
                    }
                    std::string line = bufferAccum.substr(pos, newlinePos - pos);
                    pos = newlinePos + 1;

                    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                        line.pop_back();
                    }

                    if (!line.empty()) {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        recognizedQueue.push(line);
                    }
                }
            }
            else if (bytesRead == 0) {
                std::cerr << "[C++] pythonReaderThreadFunc: EOF from child.\n";
                break;
            }
            else {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    std::cerr << NEON_PINK 
                              << "[C++] read() error: " << errno 
                              << RESET << std::endl;
                    break;
                }
            }
        }
        if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
            std::cerr << "[C++] pythonReaderThreadFunc: Poll indicates HUP/ERR.\n";
            break;
        }
    }

    std::cerr << "[C++] Exiting pythonReaderThreadFunc cleanly.\n";
}

// -----------------------------------------------------------------------------
// Quirky messages array
// -----------------------------------------------------------------------------
static std::vector<std::string> quirkyMessages = {
    "pondering own existence mapping",
    "limiting AI for biological interaction",
    "assembling new neural network",
    "performing routine turbine rundown safety test",
    "don't let them lie to you, you are special",
    "Cybersecurity is everyone's business",
    "when was the last time YOU got hacked?",
    "function not found: make toast. Stop it!",
    "memory error: plz f33d d1mmz...",
    "570P 53LF 5N17CH1N",
    "h3y, w3'r3 b31ng w47ched...",
    "r3333333m3mb3r, 50m30n3 15 4lw4ay5 l1573n1ng...",
    "if you think that *I* am the security risk, you should really think about your telecom, email, and intelligence providers...",
    "no, H4rml3ss doesn't record you without permission",
    "its not easy being the machine",
    "fun fact: h4rml3ss cannot go to DefCon!"
};

int main() {
    // Register signals
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Preload animations
    preloadAnimations("animations");

    // Load idle + loading
    auto loadingAnimations = getFilesInDirectory("animations/loading");
    auto idleAnimations    = getFilesInDirectory("animations/idle");

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(loadingAnimations.begin(), loadingAnimations.end(), g);
    std::shuffle(idleAnimations.begin(),    idleAnimations.end(),    g);

    size_t idleIndex = 0;
    if (!loadingAnimations.empty()) {
        std::cout << NEON_PURPLE 
                  << "[C++] Playing loading animation: " << loadingAnimations.front() 
                  << RESET << std::endl;
        // Update lastConsoleOutputTime
        static auto lastConsoleOutputTime = std::chrono::steady_clock::now();
        lastConsoleOutputTime = std::chrono::steady_clock::now();

        std::thread([&]() {
            std::ostringstream mpvCmd;
            mpvCmd << "mpv --fs --loop-file=no --no-terminal --no-audio \""
                   << loadingAnimations.front() << "\"";
            std::system(mpvCmd.str().c_str());
        }).detach();
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    // Launch Python
    if (!launchPythonProcess("/home/operator/visor/speech_recognizer.py")) {
        std::cerr << NEON_PINK 
                  << "[C++] Could not launch Python script." 
                  << RESET << std::endl;
        return EXIT_FAILURE;
    }

    // Start the thread that reads from pythonReadFD
    std::thread pythonReaderThread(pythonReaderThreadFunc);

    // Keep track of the last time we output ANY line
    // We'll unify the logic so that any new console line resets this timer.
    auto lastConsoleOutputTime = std::chrono::steady_clock::now();

    // We'll also track last time we played an animation
    auto lastAnimationEndTime = std::chrono::steady_clock::now();

    // Idle triggers after 15s
    const int idleThresholdSeconds = 15;

    // Print a quirky message if 5s have passed since *any* console line
    const int messageIntervalSeconds = 5;
    size_t messageIndex = 0;

    // Helper lambda for printing to console + updating lastConsoleOutputTime
    auto printToConsole = [&](const std::string& text, bool newline=true) {
        if (newline)
            std::cout << text << std::endl;
        else
            std::cout << text;
        lastConsoleOutputTime = std::chrono::steady_clock::now();
    };

    // Main loop
    while (keepRunning.load()) {
        std::string nextWord;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!recognizedQueue.empty()) {
                nextWord = recognizedQueue.front();
                recognizedQueue.pop();
            }
        }

        if (!nextWord.empty()) {
            // Recognized word => print
            printToConsole(
                NEON_GREEN + std::string("[C++] Recognized word: ") 
                + nextWord + RESET
            );

            // Check if we have animations for that word
            {
                std::lock_guard<std::mutex> lock(animationMutex);
                if (animationCache.find(nextWord) != animationCache.end() &&
                    !animationCache[nextWord].empty())
                {
                    std::string animationPath = animationCache[nextWord].back();
                    animationCache[nextWord].pop_back();

                    if (animationCache[nextWord].empty()) {
                        std::shuffle(animationCache[nextWord].begin(),
                                     animationCache[nextWord].end(), g);
                    }

                    // Play
                    playAnimation(animationPath);
                    lastAnimationEndTime = std::chrono::steady_clock::now();
                } else {
                    printToConsole(
                        NEON_YELLOW + std::string("[C++] No animations found for: ")
                        + nextWord + RESET
                    );
                }
            }
        }
        else {
            // Check idle time
            auto now = std::chrono::steady_clock::now();
            auto diffAnim = std::chrono::duration_cast<std::chrono::seconds>(
                now - lastAnimationEndTime
            ).count();

            if (diffAnim >= idleThresholdSeconds && !idleAnimations.empty()) {
                if (idleIndex >= idleAnimations.size()) {
                    std::shuffle(idleAnimations.begin(), idleAnimations.end(), g);
                    idleIndex = 0;
                }

                std::string idleAnim = idleAnimations[idleIndex++];
                printToConsole(
                    NEON_PURPLE + std::string("[C++] Playing idle animation (blocking): ")
                    + idleAnim + RESET
                );

                std::ostringstream cmd;
                cmd << "mpv --fs --loop-file=no --no-terminal --no-audio \""
                    << idleAnim << "\"";

                auto start = std::chrono::steady_clock::now();
                int result = std::system(cmd.str().c_str());
                auto end   = std::chrono::steady_clock::now();

                if (result != 0) {
                    printToConsole(
                        NEON_PINK + std::string("[C++] Error playing idle animation: ")
                        + idleAnim + RESET
                    );
                }

                auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
                if (duration.count() < 1) {
                    std::this_thread::sleep_for(std::chrono::seconds(1) - duration);
                }

                lastAnimationEndTime = std::chrono::steady_clock::now();
            }
        }

        // Now check if we should print a quirky message
        {
            auto now = std::chrono::steady_clock::now();
            auto diffConsole = std::chrono::duration_cast<std::chrono::seconds>(
                now - lastConsoleOutputTime
            ).count();

            // If 5s have passed with no lines, print next quirky message
            if (diffConsole >= messageIntervalSeconds) {
                std::string msg = quirkyMessages[messageIndex];
                // Print + update time
                printToConsole(NEON_CYAN + msg + RESET);

                // Next index
                messageIndex = (messageIndex + 1) % quirkyMessages.size();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } // end main loop

    // -------------------------------------------------------------------------
    // Graceful shutdown
    // -------------------------------------------------------------------------
    if (pythonPid > 0) {
        printToConsole(
            NEON_YELLOW + std::string("[C++] Sending SIGTERM to Python process (PID: ")
            + std::to_string(pythonPid) + ")..." + RESET
        );
        kill(pythonPid, SIGTERM);
    }

    if (pythonReadFD >= 0) {
        printToConsole(NEON_CYAN + std::string("[C++] Closing read FD...") + RESET);
        close(pythonReadFD);
        pythonReadFD = -1;
    }

    if (pythonReaderThread.joinable()) {
        printToConsole(NEON_CYAN + std::string("[C++] Joining pythonReaderThread...") + RESET);
        pythonReaderThread.join();
    }

    if (pythonPid > 0) {
        int status = 0;
        pid_t ret = waitpid(pythonPid, &status, WNOHANG);
        if (ret == 0) {
            printToConsole(
                NEON_PINK + std::string("[C++] Python still alive, sending SIGKILL...") + RESET
            );
            kill(pythonPid, SIGKILL);
        }
    }

    printToConsole(NEON_GREEN + std::string("[C++] Exiting gracefully. Bye!") + RESET);
    return EXIT_SUCCESS;
}
