#include "store_app_store/store_app_store_gamecenter_platform.h"

#include "core/foundation/diagnostics/log.h"

#import <GameKit/GameKit.h>

namespace nxm::store_app_store {
namespace {
const nx::log::Category log_store_app_store = nx::log::category("store_app_store");
} // namespace

bool GameCenterPlatform::initialize() {
  GKLocalPlayer *const player = [GKLocalPlayer localPlayer];
  player.authenticateHandler = ^(UIViewController *viewController, NSError *error) {
    if (viewController != nil) {
      nx::logi(log_store_app_store,
                "Game Center requires sign-in (sheet not presented) - "
                "staying unauthenticated");
      return;
    }
    if (error != nil) {
      nx::logi(log_store_app_store, "Game Center authentication failed");
      return;
    }
    nx::logi(log_store_app_store, "Game Center authenticated");
  };
  return true;
}

bool GameCenterPlatform::authenticated() const noexcept {
  return [GKLocalPlayer localPlayer].isAuthenticated == YES;
}

}
