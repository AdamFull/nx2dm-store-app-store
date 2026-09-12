#include "store_app_store/store_app_store_platform.h"
#include "store_app_store/store_app_store_services.h"

#include "store/store_service.h"

#include "core/app/engine.h"
#include "core/app/module.h"
#include "core/app/module_context.h"

#include "core/foundation/diagnostics/log.h"

namespace nxm::store_app_store {
namespace {

const nx::log::Category log_store_app_store = nx::log::category("store_app_store");

// store.achievements/store.cloud_saves/store.presence are deliberately
// absent - StoreKit has none of those subsystems (Game Center is a
// separate, unrelated product for that), the same "simply doesn't provide
// it" shape the other three mobile backends already established.
constexpr nxe::ModuleService PROVIDED_SERVICES[] = {
    {.id = store::kCoreService, .version = {1, 0, 0}},
    {.id = store::kIapService, .version = {1, 0, 0}},
};

class StoreAppStoreModule final : public nxe::Module {
public:
  StoreAppStoreModule() : m_core(m_platform), m_iap(m_platform) {}

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
    return registrar.provide(store::kCoreService, PROVIDED_SERVICES[0].version, core) &&
           registrar.provide(store::kIapService, PROVIDED_SERVICES[1].version, iap);
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
    return true;
  }

  void on_detach(nxe::ModuleContext &) override { m_platform.shutdown(); }

private:
  AppStorePlatform m_platform;
  AppStoreCore m_core;
  AppStoreIap m_iap;
};

} // namespace
} // namespace nxm::store_app_store

NX_DECLARE_MODULE(store_app_store, nxm::store_app_store::StoreAppStoreModule)
