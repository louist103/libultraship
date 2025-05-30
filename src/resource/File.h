#pragma once

#include <string>
#include <variant>
#include <vector>
#include <memory>
#include <stdint.h>
#include "resource/ResourceType.h"
#include "utils/binarytools/BinaryReader.h"

namespace tinyxml2 {
class XMLDocument;
class XMLElement;
} // namespace tinyxml2

namespace Ship {
class Archive;

#define RESOURCE_FORMAT_BINARY 0
#define RESOURCE_FORMAT_XML 1

struct ResourceInitData {
    std::shared_ptr<Archive> Parent;
    std::string Path;
    Endianness ByteOrder;
    uint32_t Type;
    int32_t ResourceVersion;
    uint64_t Id;
    bool IsCustom;
    uint32_t Format;
};

struct File {
    std::shared_ptr<std::vector<char>> Buffer;
    std::variant<std::shared_ptr<tinyxml2::XMLDocument>, std::shared_ptr<BinaryReader>> Reader;
    bool IsLoaded = false;
};
} // namespace Ship
