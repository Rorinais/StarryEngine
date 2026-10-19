#pragma once
// 内容层：角色材质策略（与模型资产分离，shader 可替换）
#include <content/scene/SceneBuilder.hpp>
#include <assets/loader/ModelDesc.hpp>

namespace StarryEngine::Content {
    StarryEngine::Assets::MaterialSetDesc makeCharacterMaterialSet(const SceneResources& res);
}
