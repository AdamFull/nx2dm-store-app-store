#pragma once

#include "core/foundation/core/foundation.h"

namespace nxm::store_app_store {

/// Owns Game Center's own authentication state - a second, independent
/// Apple framework from StoreKit (AppStorePlatform above), with its own
/// readiness that has nothing to do with `SKPaymentQueue.canMakePayments`.
///
/// `GKLocalPlayer.localPlayer.authenticateHandler` can hand back a
/// `UIViewController` to present a sign-in sheet for a player not already
/// signed into Game Center - this is never presented (confirmed: this
/// engine has no UIViewController-presentation plumbing anywhere, unlike
/// Android's module-reachable `SDL_GetAndroidActivity()`), so only players
/// already signed in system-wide get achievements/saves/leaderboards. The
/// same "silently reuse a cached login, never interactively trigger" shape
/// `EgsPlatform` already established for Epic-account auth.
///
/// No `shutdown()`/destructor here, unlike every other platform class in
/// this module family - `authenticateHandler` is a block set once on
/// `GKLocalPlayer`'s own process-wide singleton, not a persistent object
/// this class creates or owns, so there is genuinely nothing to release.
class GameCenterPlatform {
public:
  bool initialize();

  /// `GKLocalPlayer.localPlayer.isAuthenticated`, read fresh every call -
  /// a cheap synchronous property, not something that needs caching the
  /// way an async query's result would.
  [[nodiscard]] bool authenticated() const noexcept;
};

}
