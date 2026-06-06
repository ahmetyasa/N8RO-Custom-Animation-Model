// Arkheon Simulation Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Simulation Technologies. All rights reserved.

#include "SimCharAnimCustomModelPlugin.h"

#include <model/AnimationModel.h>
#include <model/IModel.h>
#include <model/ModelFactoryRegistry.h>
#include <plugin/IModelPluginService.h>
#include <plugin/PluginContext.h>
#include <plugin/IPluginServices.h>

#include <cmath>
#include <memory>
#include <string>
#include <unordered_set>

namespace arkheon::sample::simcharanimcustommodel {
    namespace {

        class CustomAnimationModel final : public arkheon::astsim::IModel, public arkheon::astsim::IAnimationModel {
        public:
            [[nodiscard]] std::string getTypeName() const override {
                return "animationModelCustom";
            }

            [[nodiscard]] std::unique_ptr<arkheon::astsim::IModel> clone() const override {
                return std::make_unique<CustomAnimationModel>(*this);
            }

            [[nodiscard]] bool evaluate(
                const arkheon::astsim::AnimationModelInput& input,
                arkheon::astsim::AnimationModelOutput& output) override {

                std::unordered_set<std::string> availableJointIds;
                availableJointIds.reserve(input.entity.joints.size());

                for (const auto& joint : input.entity.joints) {
                    availableJointIds.insert(joint.jointId);
                }

                const double t = input.simulationTimeSeconds;
                const double phase = std::fmod(t, 16.0);
                const double s = std::sin(t * 3.0);

                output.clearExistingJointOverrides = true;
                output.jointOverrides.clear();

                auto addJoint = [&](const char* jointId, double x, double y, double z) {
                    if (hasJoint(availableJointIds, jointId)) {
                        output.jointOverrides.push_back({ jointId, x, y, z });
                    }
                    };

                // 0-4 sec: Custom Idle / arms slightly moving
                if (phase < 4.0) {
                    addJoint("leftAnkle", 0.0, 0.0, 0.0);
                    addJoint("rightAnkle", 0.0, 0.0, 0.0);

                    addJoint("leftKnee", 0.15, 0.0, 0.0);
                    addJoint("rightKnee", 0.15, 0.0, 0.0);

                    addJoint("leftHip", 0.10 * s, 0.0, 0.0);
                    addJoint("rightHip", -0.10 * s, 0.0, 0.0);

                    addJoint("leftShoulder", 0.60, 0.0, -0.80 + (0.25 * s));
                    addJoint("rightShoulder", 0.60, 0.0, 0.80 - (0.25 * s));

                    addJoint("leftElbow", 0.70, 0.0, 0.0);
                    addJoint("rightElbow", 0.70, 0.0, 0.0);
                }

                // 4-8 sec: Custom Walk / legs and arms swing clearly
                else if (phase < 8.0) {
                    addJoint("leftAnkle", -0.45 * s, 0.0, 0.0);
                    addJoint("rightAnkle", 0.45 * s, 0.0, 0.0);

                    addJoint("leftKnee", 0.65 + (0.45 * std::sin(t * 3.0)), 0.0, 0.0);
                    addJoint("rightKnee", 0.65 - (0.45 * std::sin(t * 3.0)), 0.0, 0.0);

                    addJoint("leftHip", 0.85 * s, 0.0, 0.0);
                    addJoint("rightHip", -0.85 * s, 0.0, 0.0);

                    addJoint("leftShoulder", 0.45, 0.0, -0.90 * s);
                    addJoint("rightShoulder", 0.45, 0.0, 0.90 * s);

                    addJoint("leftElbow", 0.90, 0.0, 0.0);
                    addJoint("rightElbow", 0.90, 0.0, 0.0);
                }

                // 8-12 sec: Custom Wave / left hand waves strongly
                else if (phase < 12.0) {
                    addJoint("leftAnkle", 0.0, 0.0, 0.0);
                    addJoint("rightAnkle", 0.0, 0.0, 0.0);

                    addJoint("leftKnee", 0.10, 0.0, 0.0);
                    addJoint("rightKnee", 0.10, 0.0, 0.0);

                    addJoint("leftHip", 0.0, 0.0, 0.0);
                    addJoint("rightHip", 0.0, 0.0, 0.0);

                    addJoint("leftShoulder", 1.80, 0.0, -1.45);
                    addJoint("rightShoulder", 0.40, 0.0, 0.50);

                    addJoint("leftElbow", 1.60 + (0.70 * s), 0.0, -0.60);
                    addJoint("rightElbow", 0.50, 0.0, 0.0);
                }

                // 12-16 sec: Custom Arms Up / both arms clearly raised
                else {
                    addJoint("leftAnkle", 0.0, 0.0, 0.0);
                    addJoint("rightAnkle", 0.0, 0.0, 0.0);

                    addJoint("leftKnee", 0.15, 0.0, 0.0);
                    addJoint("rightKnee", 0.15, 0.0, 0.0);

                    addJoint("leftHip", 0.0, 0.0, 0.0);
                    addJoint("rightHip", 0.0, 0.0, 0.0);

                    addJoint("leftShoulder", 2.00, 0.0, -1.60);
                    addJoint("rightShoulder", 2.00, 0.0, 1.60);

                    addJoint("leftElbow", 0.35, 0.0, 0.0);
                    addJoint("rightElbow", 0.35, 0.0, 0.0);
                }

                return !output.jointOverrides.empty();
            }

        private:
            [[nodiscard]] static bool hasJoint(
                const std::unordered_set<std::string>& availableJointIds,
                const char* jointId) {
                if (!jointId || *jointId == '\0') {
                    return false;
                }
                if (availableJointIds.empty()) {
                    return true;
                }
                return availableJointIds.find(jointId) != availableJointIds.end();
            }
        };

    } // namespace

    int SimCharAnimCustomModelPlugin::getInterfaceVersion() const {
        return 1;
    }

    arkheon::astlib::PluginMetadata SimCharAnimCustomModelPlugin::getMetadata() const {
        arkheon::astlib::PluginMetadata metadata;
        metadata.setPluginId("sim-char-anim-custom-model");
        metadata.setVersion("1.0.0");
        metadata.setAuthor("Ahmet Yasa");
        return metadata;
    }

    void SimCharAnimCustomModelPlugin::initialize(arkheon::astlib::PluginContext& context) {
        initialized_ = true;
        shutdown_ = false;
        modelRegistered_ = false;
        modelType_ = "animationModelCustom";

        modelFactoryRegistry_ = nullptr;
        if (context.services) {
            auto* rawService = context.services->getService(arkheon::astsim::IModelPluginService::kPluginServiceId);
            auto* service = static_cast<arkheon::astsim::IModelPluginService*>(rawService);
            modelFactoryRegistry_ = service ? &service->modelFactoryRegistry() : nullptr;
        }

        if (!modelFactoryRegistry_) {
            return;
        }

        modelRegistered_ = modelFactoryRegistry_->registerFactory(
            modelType_,
            std::make_unique<CustomAnimationModel>());
    }

    void SimCharAnimCustomModelPlugin::tick(double dt) {
        static_cast<void>(dt);
        if (!initialized_ || shutdown_) {
            return;
        }
    }

    void SimCharAnimCustomModelPlugin::shutdown() {
        if (modelFactoryRegistry_) {
            if (modelRegistered_) {
                static_cast<void>(modelFactoryRegistry_->unregisterFactory(modelType_));
            }
        }

        modelRegistered_ = false;
        shutdown_ = true;
        modelFactoryRegistry_ = nullptr;
    }

} // namespace arkheon::sample::simcharanimcustommodel

extern "C" {

    ARKHEON_ASTLIB_API arkheon::astlib::IPlugin* create_plugin() {
        return new arkheon::sample::simcharanimcustommodel::SimCharAnimCustomModelPlugin();
    }

    ARKHEON_ASTLIB_API void destroy_plugin(arkheon::astlib::IPlugin* plugin) {
        if (plugin) {
            delete plugin;
        }
    }

    ARKHEON_ASTLIB_API const char* get_plugin_signature() {
        return "ARKHEON_PLUGIN_V1";
    }

} // extern "C"