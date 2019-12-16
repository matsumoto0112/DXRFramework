/**
 * @file UnorderedAccessView.h
 * @brief アンオーダードアクセスビュー
 */

#pragma once
#include "DX/Descriptor/DescriptorInfo.h"
#include "DX/Resource/Buffer.h"

namespace Framework::DX {
    class DeviceResource;
    /**
     * @class UnorderedAccessView
     * @brief discription
     */
    class UnorderedAccessView {
    public:
        /**
         * @brief
         */
        UnorderedAccessView() {}
        /**
         * @brief
         */
        ~UnorderedAccessView() {}
        void initAsTexture2D(DeviceResource* device, const Buffer& buffer, bool isGlobal = true);
        const DescriptorInfo& getInfo() const {
            return mInfo;
        }

    private:
        DescriptorInfo mInfo;
    };
} // namespace Framework::DX
