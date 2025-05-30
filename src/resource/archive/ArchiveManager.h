#pragma once

#include <string>
#include <memory>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <stdint.h>
#include "resource/File.h"

namespace Ship {
struct File;
class Archive;

class ArchiveManager {
  public:
    ArchiveManager();
    void Init(const std::vector<std::string>& archivePaths);
    void Init(const std::vector<std::string>& archivePaths, const std::unordered_set<uint32_t>& validGameVersions);
    ~ArchiveManager();

    std::shared_ptr<Archive> AddArchive(const std::string& archivePath);
    std::shared_ptr<Archive> AddArchive(std::shared_ptr<Archive> archive);
    std::shared_ptr<std::vector<std::shared_ptr<Archive>>> GetArchives();
    void SetArchives(std::shared_ptr<std::vector<std::shared_ptr<Archive>>> archives);
    size_t RemoveArchive(std::shared_ptr<Archive> archive);
    size_t RemoveArchive(const std::string& path);

    bool IsLoaded();
    std::shared_ptr<File> LoadFile(const std::string& filePath);
    std::shared_ptr<File> LoadFile(uint64_t hash);
    bool WriteFile(std::shared_ptr<Archive> archive, const std::string& filename, const std::vector<uint8_t>& data);
    bool HasFile(const std::string& filePath);
    bool HasFile(uint64_t hash);
    std::shared_ptr<Archive>
    GetArchiveFromFile(const std::string& filePath); // Retrieves a ptr to the archive that the asset is inside of
    std::shared_ptr<std::vector<std::string>> ListFiles(const std::string& searchMask = "");
    std::shared_ptr<std::vector<std::string>> ListFiles(const std::list<std::string>& includes,
                                                        const std::list<std::string>& excludes);
    std::shared_ptr<std::vector<std::string>> ListDirectories(const std::string& searchMask = "");
    std::vector<uint32_t> GetGameVersions();
    const std::string* HashToString(uint64_t hash) const;
    bool IsGameVersionValid(uint32_t gameVersion);

  protected:
    static std::vector<std::string> GetArchiveListInPaths(const std::vector<std::string>& archivePaths);
    void AddGameVersion(uint32_t newGameVersion);
    void ResetVirtualFileSystem();

  private:
    std::vector<std::shared_ptr<Archive>> mArchives;
    std::vector<uint32_t> mGameVersions;
    std::unordered_set<uint32_t> mValidGameVersions;
    std::unordered_map<uint64_t, std::string> mHashes;
    std::unordered_set<std::string> mDirectories;
    std::unordered_map<uint64_t, std::shared_ptr<Archive>> mFileToArchive;
};
} // namespace Ship
