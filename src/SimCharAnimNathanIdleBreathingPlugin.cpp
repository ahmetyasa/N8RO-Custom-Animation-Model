// Arkheon Simulation Technologies
// Proprietary and Confidential.
// Unauthorized copying of this file, via any medium, is strictly prohibited.
// © Arkheon Simulation Technologies. All rights reserved.

#include "SimCharAnimNathanIdleBreathingPlugin.h"

#include <core/json/JsonValue.h>
#include <model/AnimationModel.h>
#include <model/ModelFactoryRegistry.h>
#include <plugin/IModelPluginService.h>
#include <plugin/PluginContext.h>
#include <plugin/IPluginServices.h>

#include <cmath>
#include <string>
#include <unordered_set>

namespace arkheon::sample::simcharanimnathanidlebreathing {
namespace {

[[nodiscard]] bool hasJoint(
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

[[nodiscard]] bool evaluateIdleAnimation(
    const arkheon::astsim::AnimationModelInput& input,
    arkheon::astsim::AnimationModelOutput& output) {
    std::unordered_set<std::string> availableJointIds;
    availableJointIds.reserve(input.entity.joints.size());
    for (const auto& joint : input.entity.joints) {
        availableJointIds.insert(joint.jointId);
    }

    const double t = input.simulationTimeSeconds;
    const double swayPrimary = std::sin(t * 2.30) * 1.35;
    const double swaySecondary = std::sin(t * 3.10 + 1.20) * 1.10;
    const double headCompensation = std::sin(t * 4.20 + 0.80) * 0.95;

    output.clearExistingJointOverrides = true;
    output.jointOverrides.clear();
    if (hasJoint(availableJointIds, "spineUpper")) {
        output.jointOverrides.push_back({"spineUpper", swayPrimary, swaySecondary * 0.85, swayPrimary * 0.75});
    }
    if (hasJoint(availableJointIds, "leftShoulder")) {
        output.jointOverrides.push_back({"leftShoulder", 1.25 + (swaySecondary * 0.65), swayPrimary * 0.60, -1.65 - (swayPrimary * 0.90)});
    }
    if (hasJoint(availableJointIds, "rightShoulder")) {
        output.jointOverrides.push_back({"rightShoulder", 1.25 - (swaySecondary * 0.65), -swayPrimary * 0.60, 1.65 + (swayPrimary * 0.90)});
    }
    if (hasJoint(availableJointIds, "leftHip")) {
        output.jointOverrides.push_back({"leftHip", swaySecondary * 0.55, swayPrimary * 0.45, 1.10 + (swayPrimary * 0.80)});
    }
    if (hasJoint(availableJointIds, "rightHip")) {
        output.jointOverrides.push_back({"rightHip", -swaySecondary * 0.55, -swayPrimary * 0.45, -1.10 - (swayPrimary * 0.80)});
    }
    if (hasJoint(availableJointIds, "head")) {
        output.jointOverrides.push_back({"head", swaySecondary * 0.65, headCompensation, -swaySecondary * 0.90});
    }
    return !output.jointOverrides.empty();
}

} // namespace

int SimCharAnimNathanIdleBreathingPlugin::getInterfaceVersion() const {
    return 1;
}

arkheon::astlib::PluginMetadata SimCharAnimNathanIdleBreathingPlugin::getMetadata() const {
    arkheon::astlib::PluginMetadata metadata;
    metadata.setPluginId("sim-char-anim-nathan-idle-breathing");
    metadata.setVersion("1.0.0");
    metadata.setAuthor("Arkheon Sample");
    return metadata;
}

void SimCharAnimNathanIdleBreathingPlugin::initialize(arkheon::astlib::PluginContext& context) {
    initialized_ = true;
    shutdown_ = false;
    animationRegistered_ = false;
    modelType_ = "animationModelNathanHuman";
    animationCode_ = "Idle Breathing";
    autoRequestOnInitialize_ = false;

    modelFactoryRegistry_ = nullptr;
    if (context.services) {
        auto* rawService = context.services->getService(arkheon::astsim::IModelPluginService::kPluginServiceId);
        auto* service = static_cast<arkheon::astsim::IModelPluginService*>(rawService);
        modelFactoryRegistry_ = service ? &service->modelFactoryRegistry() : nullptr;
    }

    if (!context.configData.empty()) {
        const auto config = arkheon::astlib::JsonValue::parse(context.configData);
        if (config.has_value() && config->isObject()) {
            const auto parsedAutoRequest = config->get("autoRequestOnInitialize");
            if (parsedAutoRequest.isBool()) {
                autoRequestOnInitialize_ = parsedAutoRequest.asBool();
            }
        }
    }

    if (!modelFactoryRegistry_) {
        return;
    }

    auto* prototypeBase = modelFactoryRegistry_->getRegisteredPrototype(modelType_);
    auto* prototypeAnimationModel = dynamic_cast<arkheon::astsim::IAnimationModel*>(prototypeBase);
    if (!prototypeAnimationModel) {
        return;
    }

    animationRegistered_ = prototypeAnimationModel->registerAnimation(
        animationCode_,
        evaluateIdleAnimation);
    static_cast<void>(autoRequestOnInitialize_);
}

void SimCharAnimNathanIdleBreathingPlugin::tick(double dt) {
    static_cast<void>(dt);
    if (!initialized_ || shutdown_ || !modelFactoryRegistry_) {
        return;
    }
}

void SimCharAnimNathanIdleBreathingPlugin::shutdown() {
    if (modelFactoryRegistry_) {
        if (animationRegistered_) {
            auto* prototypeBase = modelFactoryRegistry_->getRegisteredPrototype(modelType_);
            auto* prototypeAnimationModel = dynamic_cast<arkheon::astsim::IAnimationModel*>(prototypeBase);
            if (prototypeAnimationModel) {
                static_cast<void>(prototypeAnimationModel->registerAnimation(
                    animationCode_,
                    arkheon::astsim::IAnimationModel::AnimationEvaluationFunction {}));
            }
        }
    }
    animationRegistered_ = false;
    shutdown_ = true;
    modelFactoryRegistry_ = nullptr;
}

} // namespace arkheon::sample::simcharanimnathanidlebreathing

extern "C" {

ARKHEON_ASTLIB_API arkheon::astlib::IPlugin* create_plugin() {
    return new arkheon::sample::simcharanimnathanidlebreathing::SimCharAnimNathanIdleBreathingPlugin();
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
