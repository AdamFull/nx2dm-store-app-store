#include "store_app_store/store_app_store_scripting.h"

#include "store_app_store/store_app_store_gamecenter_leaderboards.h"

#include "script/script_host.h"

namespace nxm::store_app_store {

void expose_store_app_store_extras(nxe::script::Host &host,
                                    GameCenterLeaderboards &leaderboards) {
  host.expose_as("store_app_store_leaderboard_submit_score",
                 [&leaderboards](const nx::string_view leaderboard_id,
                                 const f64 score) {
                   return leaderboards.submit_score(leaderboard_id,
                                                     nx::cast<i64>(score));
                 });
  host.expose_as("store_app_store_leaderboard_submit_pending", [&leaderboards]() {
    return leaderboards.submit_pending();
  });

  host.expose_as("store_app_store_leaderboard_download",
                 [&leaderboards](const nx::string_view leaderboard_id) {
                   return leaderboards.download(leaderboard_id);
                 });
  host.expose_as("store_app_store_leaderboard_download_pending", [&leaderboards]() {
    return leaderboards.download_pending();
  });
  host.expose_as("store_app_store_leaderboard_entry_count", [&leaderboards]() {
    return static_cast<f64>(leaderboards.entry_count());
  });
  host.expose_as("store_app_store_leaderboard_entry_rank",
                 [&leaderboards](const f64 index) {
                   return static_cast<f64>(
                       leaderboards.entry_rank(nx::cast<usize>(index)));
                 });
  host.expose_as("store_app_store_leaderboard_entry_score",
                 [&leaderboards](const f64 index) {
                   return static_cast<f64>(
                       leaderboards.entry_score(nx::cast<usize>(index)));
                 });
  host.expose_as("store_app_store_leaderboard_entry_name",
                 [&leaderboards](const f64 index) {
                   return leaderboards.entry_name(nx::cast<usize>(index));
                 });
}

}
