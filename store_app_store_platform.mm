#include "store_app_store/store_app_store_platform.h"
#include "store_app_store/store_app_store_services.h"

#include "core/foundation/diagnostics/log.h"

#import <StoreKit/StoreKit.h>

namespace nxm::store_app_store {
namespace {
const nx::log::Category log_store_app_store = nx::log::category("store_app_store");
} // namespace
}

/// The one Objective-C object this module ever creates outside
/// store_app_store_services.mm's own products-request delegate - registered
/// with `SKPaymentQueue.defaultQueue` for this module's entire lifetime, per
/// Apple's own guidance to add the observer as early as possible so a
/// transaction that completed while the app wasn't running (e.g. Ask to
/// Buy parental approval) is still delivered. Every method here forwards to
/// AppStoreCore/AppStoreIap's own static dispatch_*() functions rather than
/// holding C++ state itself - the same "plumbing stays thin, services own
/// the state" split the three JNI-shim mobile backends already established.
@interface NXStoreKitObserver : NSObject <SKPaymentTransactionObserver>
@end

@implementation NXStoreKitObserver

- (void)paymentQueue:(SKPaymentQueue *)queue
    updatedTransactions:(NSArray<SKPaymentTransaction *> *)transactions {
  for (SKPaymentTransaction *transaction in transactions) {
    const char *const productId = transaction.payment.productIdentifier.UTF8String;
    switch (transaction.transactionState) {
    case SKPaymentTransactionStatePurchased:
      [queue finishTransaction:transaction];
      nxm::store_app_store::AppStoreIap::dispatch_purchase_result(true, productId);
      break;
    case SKPaymentTransactionStateFailed:
      // SKErrorPaymentCancelled (the user dismissed the sheet) isn't
      // distinguished from any other failure here - the neutral
      // StoreIap::purchase_error() interface has no cancelled-vs-failed
      // split, the same scope limit every other backend's purchase_error()
      // already has.
      [queue finishTransaction:transaction];
      nxm::store_app_store::AppStoreIap::dispatch_purchase_result(false, productId);
      break;
    case SKPaymentTransactionStateRestored:
      // A non-consumable the signed-in Apple ID already owns, surfaced by
      // refresh_ownership()'s restoreCompletedTransactions call - finish it
      // the same as a fresh purchase (StoreKit requires every transaction,
      // restored or not, to be finished) and report it to StoreCore, not
      // StoreIap.
      [queue finishTransaction:transaction];
      nxm::store_app_store::AppStoreCore::dispatch_transaction_restored(productId);
      break;
    case SKPaymentTransactionStatePurchasing:
    case SKPaymentTransactionStateDeferred:
      // Deferred (e.g. Ask to Buy awaiting parental approval) is left
      // unfinished on purpose - StoreKit redelivers it through this same
      // observer once resolved, whether that happens in this session or a
      // future one.
      break;
    }
  }
}

- (void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue {
  nxm::store_app_store::AppStoreCore::dispatch_restore_finished(true);
}

- (void)paymentQueue:(SKPaymentQueue *)queue
    restoreCompletedTransactionsFailedWithError:(NSError *)error {
  nxm::store_app_store::AppStoreCore::dispatch_restore_finished(false);
}

@end

namespace nxm::store_app_store {

struct AppStorePlatform::Observer {
  NXStoreKitObserver *native = [NXStoreKitObserver new];
};

AppStorePlatform::AppStorePlatform() = default;
AppStorePlatform::~AppStorePlatform() { shutdown(); }

bool AppStorePlatform::initialize() {
  m_observer = nx::make_unique<Observer>();
  [[SKPaymentQueue defaultQueue] addTransactionObserver:m_observer->native];
  m_ready = [SKPaymentQueue canMakePayments] == YES;
  if (m_ready)
    nx::logi(log_store_app_store, "StoreKit payment queue ready");
  else
    nx::logi(log_store_app_store,
              "SKPaymentQueue.canMakePayments is false (parental controls?) "
              "- staying idle");
  return true;
}

void AppStorePlatform::shutdown() {
  if (m_observer != nullptr) {
    [[SKPaymentQueue defaultQueue] removeTransactionObserver:m_observer->native];
    m_observer.reset();
  }
  m_ready = false;
}

}
