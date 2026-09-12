#pragma once

#include "store_app_store/store_app_store_gamecenter_platform.h"

#include "core/foundation/core/foundation.h"
#include "core/foundation/strings/utf8_string.h"

#include <mutex>

namespace nxm::store_app_store {

/// Game Center leaderboards - an App Store-only extra, no neutral
/// `store.leaderboards` service exists to fill (same shape as
/// `EgsLeaderboards`/`SteamLeaderboards`). Guards on
/// `GameCenterPlatform::authenticated()`, mutex-guarded for the same reason
/// `GameCenterAchievements`/`GameCenterCloudSaves` already are
/// (store_app_store_gamecenter_services.h's class comment).
///
/// `download()` always loads the global, all-time top 25 - there's no
/// range parameter exposed here, matching `EgsLeaderboards`'s own simpler
/// shape (its underlying query has no range control either) rather than
/// Steam's own explicit range_start/range_end.
class GameCenterLeaderboards {
public:
  explicit GameCenterLeaderboards(GameCenterPlatform &platform) noexcept
      : m_platform(platform) {}

  /// Fires `+[GKLeaderboard submitScore:context:player:leaderboardIDs:
  /// completionHandler:]`.
  bool submit_score(nx::string_view leaderboard_id, i64 score);
  [[nodiscard]] bool submit_pending() const noexcept;

  /// Fires `+[GKLeaderboard loadLeaderboardsWithIDs:completionHandler:]`
  /// then `-loadEntriesForPlayerScope:timeScope:range:completionHandler:`
  /// on the result, refreshing entry_count()/entry_rank()/entry_score()/
  /// entry_name().
  bool download(nx::string_view leaderboard_id);
  [[nodiscard]] bool download_pending() const noexcept;
  [[nodiscard]] usize entry_count() const noexcept;
  [[nodiscard]] i64 entry_rank(usize index) const noexcept;
  [[nodiscard]] i64 entry_score(usize index) const noexcept;
  /// The returned view is only valid at the moment of the call - a
  /// concurrent completion handler could reassign the string it points
  /// into right after this returns, the same caveat
  /// `MicrosoftIap::purchase_error()` already documents for the same
  /// not-guaranteed-calling-thread reason.
  [[nodiscard]] nx::string_view entry_name(usize index) const noexcept;

private:
  GameCenterPlatform &m_platform;
  mutable std::mutex m_mutex;
  bool m_submit_pending = false;
  bool m_download_pending = false;

  struct Entry {
    i64 rank = 0;
    i64 score = 0;
    nx::string name;
  };
  nx::vector<Entry> m_entries;
};

}
