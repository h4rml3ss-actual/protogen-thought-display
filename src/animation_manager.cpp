#include "animation_manager.h"
#include <filesystem>
#include <iostream>
#include <random>
#include <algorithm>
#include <thread>
#include <sstream>
#include <cstdlib>  // for std::system

namespace fs = std::filesystem;

AnimationManager::AnimationManager() {
    std::random_device rd;
    rng = std::mt19937(rd());
}

void AnimationManager::loadAnimations(const std::string& baseDir) {
    try {
        for (const auto& entry : fs::directory_iterator(baseDir)) {
            if (entry.is_directory()) {
                std::string keyword = entry.path().filename().string();
                std::vector<std::string> files;

                for (const auto& file : fs::directory_iterator(entry.path())) {
                    std::string ext = file.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".gif" || ext == ".webp") {
                        files.push_back(file.path().string());
                    }
                }

                if (!files.empty()) {
                    std::shuffle(files.begin(), files.end(), rng);
                    animationMap[keyword] = files;
                    std::cout << "[AnimationManager] Loaded " << files.size()
                              << " animations for keyword: " << keyword << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[AnimationManager] Error loading animations: " << e.what() << std::endl;
    }
}

void AnimationManager::playAnimation(const std::string& keyword) {
    std::string animationFile;

    {
        std::lock_guard<std::mutex> lock(animMutex);
        auto it = animationMap.find(keyword);
        if (it != animationMap.end() && !it->second.empty()) {
            animationFile = it->second.back();
            it->second.pop_back();

            // Reshuffle if exhausted
            if (it->second.empty()) {
                std::shuffle(animationMap[keyword].begin(), animationMap[keyword].end(), rng);
            }
        }
    }

    if (!animationFile.empty()) {
        std::cout << "[AnimationManager] Playing: " << animationFile << std::endl;
        std::thread([animationFile]() {
            std::ostringstream cmd;
            cmd << "mpv --fs --loop-file=no --no-terminal --no-audio \""
                << animationFile << "\"";
            std::system(cmd.str().c_str());
        }).detach();
    } else {
        std::cerr << "[AnimationManager] No animation found for: " << keyword << std::endl;
    }
}
