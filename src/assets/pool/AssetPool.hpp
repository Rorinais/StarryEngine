#pragma once
#include<iostream>
#include<vector>
#include<unordered_map>
#include"../AssetType.hpp"

namespace StarryEngine::Assets {
	class AssetPool {
	public:
		AssetPool(std::shared_ptr<RHI::ResourceManager> resMgr):mResMgr(resMgr){}
		~AssetPool() {
			for (auto handle: mTexturePools){
				if (handle.isValid()) {
					mResMgr->destroy(handle);
					handle.reset();
				}
			}
			for (auto handle : mShaderPools) {
				if (handle.isValid()) {
					mResMgr->destroy(handle);
					handle.reset();
				}
			}
		}



	private:
		std::shared_ptr<RHI::ResourceManager> mResMgr;

		std::vector<RHI::TextureHandle> mTexturePools;
		std::unordered_map<std::string, uint32_t> mTextureNameToIndex;

		std::vector<RHI::ShaderHandle> mShaderPools;
		std::unordered_map<std::string, uint32_t> mShaderNameToIndex;
	};

}
