

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <random>

class AnimationManager {
public:
    AnimationManager();

    // Load animation paths from keyword folders under baseDir
    void loadAnimations(const std::string& baseDir);

    // Play one animation for a matched keyword (async)
    void playAnimation(const std::string& keyword);

private:
    std::unordered_map<std::string, std::vector<std::string>> animationMap;
    std::mutex animMutex;
    std::mt19937 rng;
};