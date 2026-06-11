#pragma once

#include <filesystem>
#include <string>
#include <vector>

bool spinningCubeBackendsSupportArrayReturns();

std::vector<std::filesystem::path> collectExamplePrimeFiles(const std::filesystem::path &examplesDir);

void compileExampleIrBatch(const std::filesystem::path &examplesDir,
                           const std::vector<std::filesystem::path> &exampleFiles,
                           const std::string &outDirName,
                           const std::vector<std::string> &prefixes,
                           bool supportsSpinningCube);
