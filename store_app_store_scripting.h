#pragma once

namespace nxe::script {
class Host;
}

namespace nxm::store_app_store {

class GameCenterLeaderboards;

/// The Game Center-only `host.store_app_store_*` surface (leaderboards) -
/// deliberately separate from store/store_scripting.cpp, which stays
/// neutral-only. Captured by direct reference rather than looked up
/// through ServiceRegistry: nothing outside store_app_store itself will
/// ever need to find it, so there's no "which backend provides this"
/// question to resolve.
void expose_store_app_store_extras(nxe::script::Host &host,
                                    GameCenterLeaderboards &leaderboards);

}
