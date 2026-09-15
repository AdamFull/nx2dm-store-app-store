#include "store_app_store/store_app_store_gamecenter_leaderboards.h"
#include "store_app_store/store_app_store_gamecenter_platform.h"
#include "store_app_store/store_app_store_gamecenter_services.h"
#include "store_app_store/store_app_store_platform.h"
#include "store_app_store/store_app_store_scripting.h"
#include "store_app_store/store_app_store_services.h"

#include "store/store_service.h"

#include "app/engine.h"
#include "app/module_system/module.h"
#include "app/module_system/module_context.h"

#include "core/foundation/diagnostics/log.h"

namespace nxm::store_app_store {
namespace {

const nx::log::Category log_store_app_store = nx::log::category("store_app_store");

// store.presence stays absent - Game Center's own friends list has been
// access-gated behind explicit per-player authorization since iOS 14, a
// real scope reduction (see store_app_store_gamecenter_services.h). Its
// achievements/cloud-saves are now covered by Game Center, a second,
// independent framework from StoreKit (see StoreAppStoreModule's members
// below).
constexpr nxe::ModuleService PROVIDED_SERVICES[] = {
    {.id = store::kCoreService, .version = {1, 0, 0}},
    {.id = store::kIapService, .version = {1, 0, 0}},
    {.id = store::kAchievementsService, .version = {1, 0, 0}},
    {.id = store::kCloudSavesService, .version = {1, 0, 0}},
};

class StoreAppStoreModule final : public nxe::Module {
public:
  StoreAppStoreModule()
      : m_core(m_platform), m_iap(m_platform),
        m_gc_achievements(m_gamecenter_platform),
        m_gc_cloud_saves(m_gamecenter_platform),
        m_gc_leaderboards(m_gamecenter_platform) {}

  [[nodiscard]] nxe::ModuleDescriptor descriptor() const noexcept override {
    nxe::ModuleDescriptor out{};
    out.id = "store_app_store";
    out.version = {1, 0, 0};
    out.provided_services = PROVIDED_SERVICES;
    out.platforms = nxe::ModulePlatform::IOS;
    return out;
  }

  bool on_register(nxe::ModuleContext &ctx) override {
    nxe::ServiceRegistrar registrar = ctx.service_registrar();
    store::StoreCore &core = m_core;
    store::StoreIap &iap = m_iap;
    store::StoreAchievements &achievements = m_gc_achievements;
    store::StoreCloudSaves &cloud_saves = m_gc_cloud_saves;
    return registrar.provide(store::kCoreService, PROVIDED_SERVICES[0].version, core) &&
           registrar.provide(store::kIapService, PROVIDED_SERVICES[1].version, iap) &&
           registrar.provide(store::kAchievementsService,
                              PROVIDED_SERVICES[2].version, achievements) &&
           registrar.provide(store::kCloudSavesService,
                              PROVIDED_SERVICES[3].version, cloud_saves);
  }

  bool on_attach(nxe::ModuleContext &) override {
    // No pump system registered here, unlike the desktop backends - every
    // StoreKit call resolves through the transaction observer/delegate
    // callbacks Apple's runtime dispatches on its own (see
    // store_app_store_platform.h), not from anything this module needs to
    // poll each frame. There's also no per-project config file: StoreKit
    // takes no developer-supplied credentials at runtime at all - it
    // resolves everything from the process's own bundle identity and the
    // signed-in Apple ID, matching store_google_play's own shape.
    m_platform.initialize();
    if (m_platform.ready())
      nx::logi(log_store_app_store, "attached, StoreKit payment queue ready");
    else
      nx::logi(log_store_app_store,
                "attached, but SKPaymentQueue.canMakePayments is false");

    // Game Center is a second, independent framework - its own readiness
    // has nothing to do with StoreKit's above (see
    // store_app_store_gamecenter_platform.h for why there's no shutdown
    // counterpart to this call).
    m_gamecenter_platform.initialize();
    return true;
  }

  void on_expose_scripts(nxe::script::Host &host, nxe::ModuleContext &) override {
    expose_store_app_store_extras(host, m_gc_leaderboards);
  }

  void on_detach(nxe::ModuleContext &) override { m_platform.shutdown(); }

private:
  AppStorePlatform m_platform;
  AppStoreCore m_core;
  AppStoreIap m_iap;

  // Game Center - a second, independent Apple framework from StoreKit
  // above, filling in store.achievements/store.cloud_saves (real
  // functionality this backend was missing) plus a leaderboards extra
  // (never part of store_service.h's neutral interface, never registered
  // through ServiceRegistry - see store_app_store_scripting.h).
  GameCenterPlatform m_gamecenter_platform;
  GameCenterAchievements m_gc_achievements;
  GameCenterCloudSaves m_gc_cloud_saves;
  GameCenterLeaderboards m_gc_leaderboards;
};

} // namespace
} // namespace nxm::store_app_store

NX_DECLARE_MODULE(store_app_store, nxm::store_app_store::StoreAppStoreModule)
