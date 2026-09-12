#pragma once

#include "store_app_store/store_app_store_gamecenter_platform.h"

#include "store/store_service.h"

#include <mutex>

namespace nxm::store_app_store {

/// Two of the five neutral services, backed by Game Center - the two that
/// genuinely map onto it (achievements, and Saved Games as
/// store.cloud_saves). store.presence stays absent: Game Center's own
/// friends list has been access-gated behind explicit per-player
/// authorization since iOS 14 (`loadFriendsAuthorizationStatus`), a real
/// scope reduction rather than an oversight, the same "narrower than the
/// full five" shape `store_stove`/`store_gog` already have for their own
/// missing subsystems.
///
/// GameKit's completion handlers (unlike StoreKit's transaction-observer
/// callbacks, which Apple documents as always landing on the main thread)
/// aren't guaranteed to run on any particular thread - closer to WinRT's
/// own `IAsyncOperation<T>::Completed()` than to StoreKit's own callback
/// shape, so both classes below guard their cached state with a mutex, the
/// same reason `MicrosoftCore`/`MicrosoftIap` already do
/// (store_microsoft_services.h). Unlike the StoreKit classes in this same
/// module, neither needs a static "current instance" + dispatch layer:
/// GameKit's APIs take completion-handler blocks, not a stateful delegate
/// protocol a separate persistent Objective-C object must conform to, so a
/// block can just capture `this` directly - the same shape
/// `MicrosoftCore::refresh_license()`'s own WinRT lambda already uses.
///
/// GameKit's numeric-stat concept doesn't exist separately from achievement
/// progress (unlike Steam/EOS's own stats API) - set_stat()/stat() always
/// refuse/return 0, a documented per-method gap within an otherwise-real
/// service, the same shape `StoveAchievements::unlock()` already has.
class GameCenterAchievements final : public store::StoreAchievements {
public:
  explicit GameCenterAchievements(GameCenterPlatform &platform) noexcept
      : m_platform(platform) {}

  /// Reports 100% progress via `+[GKAchievement reportAchievements:
  /// completionHandler:]` and optimistically updates the unlocked-ids cache
  /// immediately rather than waiting for the completion handler - the same
  /// "fire, assume success, update the cache now" shape
  /// `EgsAchievements::unlock()` already uses.
  bool unlock(nx::string_view id) override;
  [[nodiscard]] bool is_unlocked(nx::string_view id) const override;
  [[nodiscard]] nx::vector<nx::string> achievement_ids() const override;

  bool set_stat(nx::string_view, f64) override { return false; }
  [[nodiscard]] f64 stat(nx::string_view) const override { return 0.0; }

  /// Fires both `+[GKAchievementDescription
  /// loadAchievementDescriptionsWithCompletionHandler:]` (refreshing
  /// achievement_ids()) and `+[GKAchievement
  /// loadAchievementsWithCompletionHandler:]` (refreshing is_unlocked()) -
  /// two independent completion handlers, each only touching the cache
  /// field it owns, the same shape `EgsAchievements::refresh()` already
  /// has for its own three independent underlying queries.
  /// @p stat_ids is ignored - nothing to scope a bulk query by here.
  void refresh(const nx::vector<nx::string> &stat_ids = {}) override;

private:
  GameCenterPlatform &m_platform;
  mutable std::mutex m_mutex;
  nx::vector<nx::string> m_achievement_ids;
  nx::vector<nx::string> m_unlocked_ids;
};

/// Game Center's Saved Games, mapped onto store.cloud_saves - `key` is a
/// `GKSavedGame.name`, `value` its raw data bytes (an opaque blob, same as
/// every other backend's own cloud-save value).
class GameCenterCloudSaves final : public store::StoreCloudSaves {
public:
  explicit GameCenterCloudSaves(GameCenterPlatform &platform) noexcept
      : m_platform(platform) {}

  /// Fires `-[GKLocalPlayer saveGameData:withName:completionHandler:]`.
  bool write(nx::string_view key, nx::string_view value) override;

  /// Fires `-[GKLocalPlayer fetchSavedGamesWithCompletionHandler:]` to find
  /// the matching save, then `-[GKSavedGame
  /// loadDataWithCompletionHandler:]` - asynchronous like every call here,
  /// so this returns whatever was cached from the last successful read of
  /// this key (empty the first time), the exact shape
  /// `EgsCloudSaves::read()` already documents for the same reason.
  [[nodiscard]] nx::string read(nx::string_view key) const override;
  /// Reads the cached name list `refresh_keys()` populates - GameKit has no
  /// synchronous per-key existence check.
  [[nodiscard]] bool exists(nx::string_view key) const override;
  /// Fires `-[GKLocalPlayer deleteSavedGamesWithName:completionHandler:]`.
  bool remove(nx::string_view key) override;
  [[nodiscard]] nx::vector<nx::string> keys() const override;
  /// No quota-query API at all, the same honest gap EOS's own
  /// PlayerDataStorage already has.
  [[nodiscard]] u64 bytes_used() const override { return 0; }
  [[nodiscard]] u64 bytes_total() const override { return 0; }

  /// Fires `-[GKLocalPlayer fetchSavedGamesWithCompletionHandler:]`,
  /// refreshing keys()/exists(). Save-conflict resolution
  /// (`resolveConflictingSavedGames:...`, which GameKit can surface when
  /// the same name was written from two devices) is not handled - a real
  /// scope reduction, not an oversight.
  void refresh_keys() override;

private:
  GameCenterPlatform &m_platform;
  mutable std::mutex m_mutex;
  nx::vector<nx::string> m_keys;
  mutable nx::string m_read_buffer;
};

}
