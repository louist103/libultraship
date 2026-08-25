#include "fast/resource/factory/TextureFactory.h"
#include "fast/resource/type/Texture.h"
#include "ship/Context.h"
#include "ship/resource/ResourceManager.h"
#include "spdlog/spdlog.h"

namespace Fast {

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryTextureV0::ReadResource(std::shared_ptr<Ship::File> file,
                                             std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto texture = std::make_shared<Texture>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    texture->Type = (TextureType)reader->ReadUInt32();
    texture->Width = reader->ReadUInt32();
    texture->Height = reader->ReadUInt32();
    texture->ImageDataSize = reader->ReadUInt32();
    texture->mImageBuffer = file->Buffer;
    texture->ImageData = reinterpret_cast<uint8_t*>(file->Buffer->data() + reader->GetBaseAddress());

    return texture;
}

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryTextureV1::ReadResource(std::shared_ptr<Ship::File> file,
                                             std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto texture = std::make_shared<Texture>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    texture->Type = (TextureType)reader->ReadUInt32();
    texture->Width = reader->ReadUInt32();
    texture->Height = reader->ReadUInt32();
    texture->Flags = reader->ReadUInt32();
    texture->HByteScale = reader->ReadFloat();
    texture->VPixelScale = reader->ReadFloat();
    texture->ImageDataSize = reader->ReadUInt32();
    texture->mImageBuffer = file->Buffer;
    texture->ImageData = reinterpret_cast<uint8_t*>(file->Buffer->data() + reader->GetBaseAddress());

    return texture;
}

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryTextureV2::ReadResource(std::shared_ptr<Ship::File> file,
                                                 std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto texture = std::make_shared<Texture>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    texture->Type = (TextureType)reader->ReadUInt32();
    texture->Width = reader->ReadUInt32();
    texture->Height = reader->ReadUInt32();
    texture->atlasX = reader->ReadUInt32();
    texture->atlasY = reader->ReadUInt32();
    //const int32_t parentLen = reader->ReadInt32();
    //auto parentName = std::make_unique<char[]>(parentLen);
    //reader->Read(parentName.get(), parentLen);
    const auto parentName = reader->ReadString();
    texture->mParentAtlas = std::static_pointer_cast<Texture>(Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(parentName));
    texture->mImageBuffer = file->Buffer;
    // These are just proxys to the actual atlas data. No texture data is in this resource.
    texture->ImageData = nullptr;//reinterpret_cast<uint8_t*>(file->Buffer->data() + reader->GetBaseAddress());

    return texture;
}

} // namespace Fast
