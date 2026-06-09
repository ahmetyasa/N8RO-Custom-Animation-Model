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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace arkheon::sample::simcharanimcustommodel {
    namespace {

        // Mission script — 44 sn döngü:
        //  0– 2 sn  giriş (idle → walk geçişi)
        //  2–12 sn  FAZ 1: YÜRÜME
        // 12–13 sn  geçiş
        // 13–23 sn  FAZ 2: SQUAT
        // 23–24 sn  geçiş
        // 24–34 sn  FAZ 3: EL SALLAMA
        // 34–35 sn  geçiş
        // 35–43 sn  FAZ 4: JUMPING JACK
        // 43–44 sn  kapanış

        // Smoothstep — başlangıç/bitişte türev sıfır
        static inline double smoothstep(double x) {
            if (x <= 0.0) return 0.0;
            if (x >= 1.0) return 1.0;
            return x * x * (3.0 - 2.0 * x);
        }
        static inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

        // Tüm kontrol edilen eklemler tek struct'ta
        struct Pose {
            double lHipX, lHipY, lHipZ;
            double rHipX, rHipY, rHipZ;
            double lKneeX, rKneeX;
            double lAnkleX, rAnkleX;
            double lShX, lShZ;
            double rShX, rShZ;
            double lElX, rElX;
            double spineX;
        };

        static Pose idlePose() {
            Pose p{};
            p.lHipX = 0.0;  p.lHipY = 0.0;  p.lHipZ = 0.0;
            p.rHipX = 0.0;  p.rHipY = 0.0;  p.rHipZ = 0.0;
            p.lKneeX = 0.05; p.rKneeX = 0.05;
            p.lAnkleX = 0.0; p.rAnkleX = 0.0;
            p.lShX = 0.30; p.lShZ = -0.30;
            p.rShX = 0.30; p.rShZ = 0.30;
            p.lElX = 0.20; p.rElX = 0.20;
            p.spineX = 0.0;
            return p;
        }

        static Pose blendPose(const Pose& a, const Pose& b, double t) {
            double s = smoothstep(t);
            Pose r{};
            r.lHipX = lerp(a.lHipX, b.lHipX, s); r.lHipY = lerp(a.lHipY, b.lHipY, s); r.lHipZ = lerp(a.lHipZ, b.lHipZ, s);
            r.rHipX = lerp(a.rHipX, b.rHipX, s); r.rHipY = lerp(a.rHipY, b.rHipY, s); r.rHipZ = lerp(a.rHipZ, b.rHipZ, s);
            r.lKneeX = lerp(a.lKneeX, b.lKneeX, s); r.rKneeX = lerp(a.rKneeX, b.rKneeX, s);
            r.lAnkleX = lerp(a.lAnkleX, b.lAnkleX, s); r.rAnkleX = lerp(a.rAnkleX, b.rAnkleX, s);
            r.lShX = lerp(a.lShX, b.lShX, s); r.lShZ = lerp(a.lShZ, b.lShZ, s);
            r.rShX = lerp(a.rShX, b.rShX, s); r.rShZ = lerp(a.rShZ, b.rShZ, s);
            r.lElX = lerp(a.lElX, b.lElX, s); r.rElX = lerp(a.rElX, b.rElX, s);
            r.spineX = lerp(a.spineX, b.spineX, s);
            return r;
        }

        // FAZ 1: YÜRÜME
        // Adım periyodu 1.1 sn — orta tempo normal yürüyüş hızı.
        // Sol/sağ bacak tam π faz farkıyla sallanır.
        // Kalça fleksiyonu ±28° (0.49 rad) — gerçek yürüyüş ölçümlerine yakın.
        // Diz: swing fazında ~60° büküm, stance'da neredeyse düz.
        // Ayak bileği: topuk vuruşunda dorsifleksif, itme sırasında plantarfleksif.
        // Omuzlar bacaklarla ters senkronda, dirsekler sabit ~75° bükümlü.
        // Kalça lateral kayması adım frekansının yarısında: tek destek anında karşı tarafa.
        static Pose walkPose(double lt) {
            const double period = 1.1;
            const double a = lt * (2.0 * M_PI / period);
            const double L = std::sin(a);          // sol bacak: +1 öne, -1 arkaya
            const double R = std::sin(a + M_PI);   // sağ bacak: ters faz

            // Kalça fleksiyon/ekstansiyon — ±0.49 rad (±28°)
            const double lHipX = L * 0.49;
            const double rHipX = R * 0.49;

            // Lateral ağırlık transferi: stance tarafına doğru.
            // Adım periyodunun iki katında (0.55 sn) bir sallanma — her adımda bir kez.
            const double lHipY = std::sin(a) * 0.05;
            const double rHipY = -std::sin(a) * 0.05;   // sağ kalça zıt yönde

            // Diz: yalnızca swing fazında bükülür (öne gelen bacak).
            // max(0, sin) ile stance fazında diz düz kalır (~0.05 rad).
            const double lKnee = 0.05 + std::max(0.0, L) * 1.05;  // max ~1.10 rad ≈ 63°
            const double rKnee = 0.05 + std::max(0.0, R) * 1.05;

            // Ayak bileği: topuk vuruşunda dorsifleksiyon (+), itme sırasında plantarfleksiyon (−)
            const double lAnkle = L * 0.28;
            const double rAnkle = R * 0.28;

            // Omuzlar — bacaklarla ters fazda, ±18° salınım
            const double lShX = 0.32 + R * 0.32;   // sol omuz sağ bacakla senkron
            const double rShX = 0.32 + L * 0.32;

            // Dirsek ~75° bükümlü sabit (güçlü yürüyüş pozu)
            const double elbow = 1.30;

            // Gövde lateral eğilimi — ağırlık taşıyan tarafa çok az yatar
            const double spine = std::sin(a) * 0.03;

            Pose p{};
            p.lHipX = lHipX; p.lHipY = lHipY; p.lHipZ = 0.0;
            p.rHipX = rHipX; p.rHipY = rHipY; p.rHipZ = 0.0;
            p.lKneeX = lKnee; p.rKneeX = rKnee;
            p.lAnkleX = lAnkle; p.rAnkleX = rAnkle;
            p.lShX = lShX; p.lShZ = -0.32;
            p.rShX = rShX; p.rShZ = 0.32;
            p.lElX = elbow; p.rElX = elbow;
            p.spineX = spine;
            return p;
        }

        // FAZ 2: SQUAT
        // Tekrar periyodu 2.0 sn — 10 sn içinde 5 tam tekrar.
        // (1−cos)/2 eğrisi ile ivmeli iniş/çıkış.
        // Tam squat derinliği: diz ~120° büküm (2.09 rad), kalça ~100° fleksiyon (1.75 rad).
        // Gövde öne eğilimi squat derinliğiyle orantılı — gerçek squat formunu yansıtır.
        // Kollar tam squat'ta yatay konuma çıkar (denge ağırlığı).
        static Pose squatPose(double lt) {
            const double a = lt * (2.0 * M_PI / 2.0);
            const double depth = (1.0 - std::cos(a)) * 0.5;  // 0→1→0

            // Kalça: dik=0.0, tam squat≈1.75 rad (≈100°)
            const double hip = depth * 1.75;
            // Diz: dik=0.05, tam squat≈2.09 rad (≈120°)
            const double knee = 0.05 + depth * 2.04;
            // Ayak bileği dorsifleksiyonu: ≈20° max
            const double ankle = depth * 0.35;
            // Gövde öne eğilimi — squat'ta kaçınılmaz
            const double spine = depth * 0.22;
            // Kollar: yatay konuma doğru (1.57 rad = 90°)
            const double arm = depth * 1.57;

            Pose p{};
            p.lHipX = hip; p.lHipY = 0.0; p.lHipZ = 0.0;
            p.rHipX = hip; p.rHipY = 0.0; p.rHipZ = 0.0;
            p.lKneeX = knee; p.rKneeX = knee;
            p.lAnkleX = ankle; p.rAnkleX = ankle;
            p.lShX = arm; p.lShZ = -0.18;
            p.rShX = arm; p.rShZ = 0.18;
            p.lElX = 0.05; p.rElX = 0.05;
            p.spineX = spine;
            return p;
        }

        // FAZ 3: EL SALLAMA
        // Sağ kol yukarıda, dirsek bükük — klasik karşılama dalgası.
        // Omuz ~123° yukarıda sabit; dirsek 0.8 sn periyotla 50°–130° arasında sallanır.
        // Omuzda küçük yukarı-aşağı titreme dirsekle faz kaymalı → hareket omuzdan akar.
        // Sol kol ve tüm bacaklar dinlenik.
        static Pose wavePose(double lt) {
            const double a = lt * (2.0 * M_PI / 0.8);

            // Sağ omuz: yukarıda sabit + küçük titreme
            const double rShX = 2.15 + std::sin(a) * 0.10;
            const double rShZ = 1.40 + std::sin(a) * 0.12;

            // Sağ dirsek: büyük sallanma, faz kaymalı
            const double rElX = 0.90 + std::sin(a + 0.55) * 0.65;

            Pose p{};
            p.lHipX = 0.0; p.lHipY = 0.0; p.lHipZ = 0.0;
            p.rHipX = 0.0; p.rHipY = 0.0; p.rHipZ = 0.0;
            p.lKneeX = 0.05; p.rKneeX = 0.05;
            p.lAnkleX = 0.0; p.rAnkleX = 0.0;
            p.lShX = 0.30; p.lShZ = -0.30;
            p.rShX = rShX; p.rShZ = rShZ;
            p.lElX = 0.20; p.rElX = rElX;
            p.spineX = 0.0;
            return p;
        }

        // FAZ 4: JUMPING JACK
        // Periyot 1.3 sn — 8 sn içinde ~6 tam tekrar.
        // Kollar: yan duruştan baş üstüne tam açılır (0.30 → 2.85 rad ≈ 163°).
        // Bacaklar: bitişikten ±38° abduction'a açılır.
        // Dirsek: açılma sırasında hafif gerilir, kapanırken bükülür.
        // Kol ve bacak hareketi tam eşzamanlı.
        static Pose jumpingJackPose(double lt) {
            const double a = lt * (2.0 * M_PI / 1.3);
            const double expand = (1.0 - std::cos(a)) * 0.5;

            // Kol: yan kapalı (0.30) → baş üstü açık (2.85)
            const double armFlx = 0.30 + expand * 2.55;
            // Omuz abduction (Z ekseni): kapalı (0.25) → açık (1.55)
            const double armAbduct = 0.25 + expand * 1.30;
            // Dirsek: kapalı=0.25, açık=0.05 (gerilmiş kol)
            const double elbow = 0.25 - expand * 0.20;
            // Bacak abduction: 0 → ±0.66 rad (±38°)
            const double legAbduct = expand * 0.66;

            Pose p{};
            p.lHipX = 0.0; p.lHipY = 0.0; p.lHipZ = -legAbduct;
            p.rHipX = 0.0; p.rHipY = 0.0; p.rHipZ = legAbduct;
            p.lKneeX = 0.05; p.rKneeX = 0.05;
            p.lAnkleX = 0.0; p.rAnkleX = 0.0;
            p.lShX = armFlx; p.lShZ = -(armAbduct);
            p.rShX = armFlx; p.rShZ = armAbduct;
            p.lElX = elbow; p.rElX = elbow;
            p.spineX = 0.0;
            return p;
        }

        class CustomAnimationModel final
            : public arkheon::astsim::IModel,
            public arkheon::astsim::IAnimationModel
        {
        public:
            [[nodiscard]] std::string getTypeName() const override { return "animationModelCustom"; }
            [[nodiscard]] std::unique_ptr<arkheon::astsim::IModel> clone() const override {
                return std::make_unique<CustomAnimationModel>(*this);
            }

            [[nodiscard]] bool evaluate(
                const arkheon::astsim::AnimationModelInput& input,
                arkheon::astsim::AnimationModelOutput& output) override
            {
                std::unordered_set<std::string> avail;
                avail.reserve(input.entity.joints.size());
                for (const auto& j : input.entity.joints) avail.insert(j.jointId);

                const double t = input.simulationTimeSeconds;
                const double tLoop = std::fmod(t, 44.0);

                Pose pose = idlePose();

                if (tLoop < 2.0) { pose = blendPose(idlePose(), walkPose(0.0), (tLoop) / 2.0); }
                else if (tLoop < 12.0) { pose = walkPose(tLoop - 2.0); }
                else if (tLoop < 13.0) { pose = blendPose(walkPose(10.0), idlePose(), (tLoop - 12.0) / 1.0); }
                else if (tLoop < 23.0) { pose = squatPose(tLoop - 13.0); }
                else if (tLoop < 24.0) { pose = blendPose(squatPose(10.0), idlePose(), (tLoop - 23.0) / 1.0); }
                else if (tLoop < 34.0) { pose = wavePose(tLoop - 24.0); }
                else if (tLoop < 35.0) { pose = blendPose(wavePose(10.0), idlePose(), (tLoop - 34.0) / 1.0); }
                else if (tLoop < 43.0) { pose = jumpingJackPose(tLoop - 35.0); }
                else { pose = blendPose(jumpingJackPose(8.0), idlePose(), (tLoop - 43.0) / 1.0); }

                output.clearExistingJointOverrides = true;
                output.jointOverrides.clear();

                auto add = [&](const char* id, double x, double y, double z) {
                    if (!id || *id == '\0') return;
                    if (!avail.empty() && avail.find(id) == avail.end()) return;
                    output.jointOverrides.push_back({ id, x, y, z });
                    };

                add("leftHip", pose.lHipX, pose.lHipY, pose.lHipZ);
                add("rightHip", pose.rHipX, pose.rHipY, pose.rHipZ);
                add("leftKnee", pose.lKneeX, 0.0, 0.0);
                add("rightKnee", pose.rKneeX, 0.0, 0.0);
                add("leftAnkle", pose.lAnkleX, 0.0, 0.0);
                add("rightAnkle", pose.rAnkleX, 0.0, 0.0);
                add("leftShoulder", pose.lShX, 0.0, pose.lShZ);
                add("rightShoulder", pose.rShX, 0.0, pose.rShZ);
                add("leftElbow", pose.lElX, 0.0, 0.0);
                add("rightElbow", pose.rElX, 0.0, 0.0);
                add("spine", pose.spineX, 0.0, 0.0);

                return !output.jointOverrides.empty();
            }
        };

    } // namespace

    int SimCharAnimCustomModelPlugin::getInterfaceVersion() const { return 1; }

    arkheon::astlib::PluginMetadata SimCharAnimCustomModelPlugin::getMetadata() const {
        arkheon::astlib::PluginMetadata metadata;
        metadata.setPluginId("sim-char-anim-custom-model");
        metadata.setVersion("3.0.0");
        metadata.setAuthor("Ahmet Yasa");
        return metadata;
    }

    void SimCharAnimCustomModelPlugin::initialize(arkheon::astlib::PluginContext& context) {
        initialized_ = true; shutdown_ = false; modelRegistered_ = false;
        modelType_ = "animationModelCustom";
        modelFactoryRegistry_ = nullptr;
        if (context.services) {
            auto* raw = context.services->getService(arkheon::astsim::IModelPluginService::kPluginServiceId);
            auto* svc = static_cast<arkheon::astsim::IModelPluginService*>(raw);
            modelFactoryRegistry_ = svc ? &svc->modelFactoryRegistry() : nullptr;
        }
        if (!modelFactoryRegistry_) return;
        modelRegistered_ = modelFactoryRegistry_->registerFactory(
            modelType_, std::make_unique<CustomAnimationModel>());
    }

    void SimCharAnimCustomModelPlugin::tick(double dt) {
        static_cast<void>(dt);
        if (!initialized_ || shutdown_) return;
    }

    void SimCharAnimCustomModelPlugin::shutdown() {
        if (modelFactoryRegistry_ && modelRegistered_)
            static_cast<void>(modelFactoryRegistry_->unregisterFactory(modelType_));
        modelRegistered_ = false; shutdown_ = true; modelFactoryRegistry_ = nullptr;
    }

} // namespace arkheon::sample::simcharanimcustommodel

extern "C" {
    ARKHEON_ASTLIB_API arkheon::astlib::IPlugin* create_plugin() {
        return new arkheon::sample::simcharanimcustommodel::SimCharAnimCustomModelPlugin();
    }
    ARKHEON_ASTLIB_API void destroy_plugin(arkheon::astlib::IPlugin* plugin) { if (plugin) delete plugin; }
    ARKHEON_ASTLIB_API const char* get_plugin_signature() { return "ARKHEON_PLUGIN_V1"; }
} // extern "C"