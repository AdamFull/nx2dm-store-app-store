#include "store_app_store/store_app_store_gamecenter_services.h"

#include <cstring>
#include <utility>

#import <GameKit/GameKit.h>

namespace nxm::store_app_store {

// -- GameCenterAchievements -----------------------------------------------

bool GameCenterAchievements::unlock(const nx::string_view id) {
  if (!m_platform.authenticated())
    return false;
  NSString *const identifier =
      [NSString stringWithUTF8String:nx::string(id).c_str()];
  GKAchievement *const achievement =
      [[GKAchievement alloc] initWithIdentifier:identifier];
  achievement.percentComplete = 100.0;
  achievement.showsCompletionBanner = YES;
  [GKAchievement reportAchievements:@[ achievement ] completionHandler:nil];

  const std::lock_guard lock(m_mutex);
  for (const nx::string &unlocked : m_unlocked_ids)
    if (unlocked.view() == id)
      return true;
  m_unlocked_ids.emplace_back(id);
  return true;
}

bool GameCenterAchievements::is_unlocked(const nx::string_view id) const {
  const std::lock_guard lock(m_mutex);
  for (const nx::string &unlocked : m_unlocked_ids)
    if (unlocked.view() == id)
      return true;
  return false;
}

nx::vector<nx::string> GameCenterAchievements::achievement_ids() const {
  const std::lock_guard lock(m_mutex);
  return m_achievement_ids;
}

void GameCenterAchievements::refresh(const nx::vector<nx::string> &) {
  if (!m_platform.authenticated())
    return;

  [GKAchievementDescription loadAchievementDescriptionsWithCompletionHandler:
      ^(NSArray<GKAchievementDescription *> *descriptions, NSError *error) {
        if (error != nil)
          return;
        nx::vector<nx::string> ids;
        ids.reserve(descriptions.count);
        for (GKAchievementDescription *description in descriptions)
          ids.emplace_back(description.identifier.UTF8String);
        const std::lock_guard lock(m_mutex);
        m_achievement_ids = std::move(ids);
      }];

  [GKAchievement loadAchievementsWithCompletionHandler:
      ^(NSArray<GKAchievement *> *achievements, NSError *error) {
        if (error != nil)
          return;
        nx::vector<nx::string> unlocked;
        for (GKAchievement *achievement in achievements)
          if (achievement.percentComplete >= 100.0)
            unlocked.emplace_back(achievement.identifier.UTF8String);
        const std::lock_guard lock(m_mutex);
        m_unlocked_ids = std::move(unlocked);
      }];
}

// -- GameCenterCloudSaves ---------------------------------------------------

bool GameCenterCloudSaves::write(const nx::string_view key,
                                 const nx::string_view value) {
  if (!m_platform.authenticated())
    return false;
  NSString *const name = [NSString stringWithUTF8String:nx::string(key).c_str()];
  NSData *const data = [NSData dataWithBytes:value.data() length:value.size()];
  [[GKLocalPlayer localPlayer] saveGameData:data
                                    withName:name
                           completionHandler:nil];
  return true;
}

nx::string GameCenterCloudSaves::read(const nx::string_view key) const {
  if (!m_platform.authenticated())
    return {};
  // One in-flight read at a time, the same simplification EgsCloudSaves'
  // own m_read_buffer already documents - the completion handlers below
  // always overwrite m_read_buffer unconditionally rather than re-checking
  // which key they were answering.
  NSString *const name = [NSString stringWithUTF8String:nx::string(key).c_str()];
  [[GKLocalPlayer localPlayer]
      fetchSavedGamesWithCompletionHandler:^(NSArray<GKSavedGame *> *savedGames,
                                             NSError *error) {
        if (error != nil)
          return;
        for (GKSavedGame *savedGame in savedGames) {
          if (![savedGame.name isEqualToString:name])
            continue;
          [savedGame loadDataWithCompletionHandler:^(NSData *data, NSError *loadError) {
            if (loadError != nil)
              return;
            nx::string content;
            content.resize(data.length);
            if (data.length > 0)
              std::memcpy(content.data(), data.bytes, data.length);
            const std::lock_guard lock(m_mutex);
            m_read_buffer = std::move(content);
          }];
          break;
        }
      }];
  // The read is asynchronous - like every other query in this module, this
  // returns whatever was cached from the last successful read (empty the
  // first time), and the real content lands once the completion handlers
  // above run.
  const std::lock_guard lock(m_mutex);
  return m_read_buffer;
}

bool GameCenterCloudSaves::exists(const nx::string_view key) const {
  const std::lock_guard lock(m_mutex);
  for (const nx::string &existing : m_keys)
    if (existing.view() == key)
      return true;
  return false;
}

bool GameCenterCloudSaves::remove(const nx::string_view key) {
  if (!m_platform.authenticated())
    return false;
  NSString *const name = [NSString stringWithUTF8String:nx::string(key).c_str()];
  [[GKLocalPlayer localPlayer] deleteSavedGamesWithName:name completionHandler:nil];
  return true;
}

nx::vector<nx::string> GameCenterCloudSaves::keys() const {
  const std::lock_guard lock(m_mutex);
  return m_keys;
}

void GameCenterCloudSaves::refresh_keys() {
  if (!m_platform.authenticated())
    return;
  [[GKLocalPlayer localPlayer]
      fetchSavedGamesWithCompletionHandler:^(NSArray<GKSavedGame *> *savedGames,
                                             NSError *error) {
        if (error != nil)
          return;
        nx::vector<nx::string> names;
        names.reserve(savedGames.count);
        for (GKSavedGame *savedGame in savedGames)
          names.emplace_back(savedGame.name.UTF8String);
        const std::lock_guard lock(m_mutex);
        m_keys = std::move(names);
      }];
}

}
