#pragma once

#include "core/foundation/core/foundation.h"

namespace nxm::store_app_store {

/// Owns the StoreKit transaction-observer plumbing shared by both services.
/// `SKPaymentTransactionObserver`/`SKProductsRequestDelegate` are
/// Objective-C protocols an Objective-C object must conform to -
/// store_app_store_platform.mm defines exactly one such object
/// (`NXStoreKitObserver`) and keeps it alive for this class's lifetime;
/// nothing outside that one .mm file ever touches Objective-C directly -
/// the same "the cross-language plumbing stays behind one class" shape the
/// three JNI-shim mobile backends already established, just for a
/// same-process C++/Objective-C++ boundary instead of a cross-VM one (no
/// JNI needed at all: Objective-C++ lets this class's own .mm file call
/// StoreKit directly, and StoreKit calls back into plain C++ methods
/// directly too - the first mobile backend in this family with no
/// JNI-shim-equivalent indirection).
///
/// Exactly one AppStorePlatform (and one AppStoreCore/AppStoreIap) is ever
/// alive in a process, the same invariant `order_modules()` already
/// enforces for "only one store backend active" - AppStoreCore/AppStoreIap
/// each dispatch through their own static "current instance" pointer,
/// mirroring every other backend in this family.
class AppStorePlatform {
public:
  AppStorePlatform();
  ~AppStorePlatform();

  AppStorePlatform(const AppStorePlatform &) = delete;
  AppStorePlatform &operator=(const AppStorePlatform &) = delete;

  /// Registers the transaction observer with `SKPaymentQueue.defaultQueue`
  /// and checks `SKPaymentQueue.canMakePayments` - unlike the JNI-shim
  /// mobile backends, both are synchronous, so readiness is known
  /// immediately rather than through an async callback.
  bool initialize();
  void shutdown();

  [[nodiscard]] bool ready() const noexcept { return m_ready; }

private:
  struct Observer;
  nx::unique_ptr<Observer> m_observer;
  bool m_ready = false;
};

}
