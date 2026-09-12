#include "store_app_store/store_app_store_gamecenter_leaderboards.h"

#include <utility>

#import <GameKit/GameKit.h>

namespace nxm::store_app_store {

bool GameCenterLeaderboards::submit_score(const nx::string_view leaderboard_id,
                                          const i64 score) {
  if (!m_platform.authenticated())
    return false;
  NSString *const identifier =
      [NSString stringWithUTF8String:nx::string(leaderboard_id).c_str()];

  {
    const std::lock_guard lock(m_mutex);
    m_submit_pending = true;
  }
  [GKLeaderboard submitScore:score
                      context:0
                       player:[GKLocalPlayer localPlayer]
               leaderboardIDs:@[ identifier ]
             completionHandler:^(NSError *error) {
               const std::lock_guard lock(m_mutex);
               m_submit_pending = false;
             }];
  return true;
}

bool GameCenterLeaderboards::submit_pending() const noexcept {
  const std::lock_guard lock(m_mutex);
  return m_submit_pending;
}

bool GameCenterLeaderboards::download(const nx::string_view leaderboard_id) {
  if (!m_platform.authenticated())
    return false;
  NSString *const identifier =
      [NSString stringWithUTF8String:nx::string(leaderboard_id).c_str()];

  {
    const std::lock_guard lock(m_mutex);
    m_download_pending = true;
  }
  [GKLeaderboard loadLeaderboardsWithIDs:@[ identifier ]
                        completionHandler:^(NSArray<GKLeaderboard *> *leaderboards,
                                            NSError *error) {
                          if (error != nil || leaderboards.count == 0) {
                            const std::lock_guard lock(m_mutex);
                            m_download_pending = false;
                            return;
                          }
                          GKLeaderboard *const leaderboard = leaderboards.firstObject;
                          [leaderboard
                              loadEntriesForPlayerScope:GKLeaderboardPlayerScopeGlobal
                                              timeScope:GKLeaderboardTimeScopeAllTime
                                                  range:NSMakeRange(1, 25)
                                       completionHandler:^(
                                           GKLeaderboardEntry *localPlayerEntry,
                                           NSArray<GKLeaderboardEntry *> *entries,
                                           NSInteger totalPlayerCount, NSError *loadError) {
                                         const std::lock_guard lock(m_mutex);
                                         m_download_pending = false;
                                         if (loadError != nil)
                                           return;
                                         nx::vector<Entry> results;
                                         results.reserve(entries.count);
                                         for (GKLeaderboardEntry *entry in entries) {
                                           Entry item;
                                           item.rank = entry.rank;
                                           item.score = entry.score;
                                           item.name = nx::string(
                                               entry.player.displayName.UTF8String);
                                           results.push_back(std::move(item));
                                         }
                                         m_entries = std::move(results);
                                       }];
                        }];
  return true;
}

bool GameCenterLeaderboards::download_pending() const noexcept {
  const std::lock_guard lock(m_mutex);
  return m_download_pending;
}

usize GameCenterLeaderboards::entry_count() const noexcept {
  const std::lock_guard lock(m_mutex);
  return m_entries.size();
}

i64 GameCenterLeaderboards::entry_rank(const usize index) const noexcept {
  const std::lock_guard lock(m_mutex);
  return index < m_entries.size() ? m_entries[index].rank : 0;
}

i64 GameCenterLeaderboards::entry_score(const usize index) const noexcept {
  const std::lock_guard lock(m_mutex);
  return index < m_entries.size() ? m_entries[index].score : 0;
}

nx::string_view GameCenterLeaderboards::entry_name(const usize index) const noexcept {
  const std::lock_guard lock(m_mutex);
  return index < m_entries.size() ? m_entries[index].name.view() : nx::string_view{};
}

}
