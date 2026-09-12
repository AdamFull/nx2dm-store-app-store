#pragma once

#include "store_app_store/store_app_store_platform.h"

#include "store/store_service.h"

namespace nxm::store_app_store {

/// Two of the five neutral services (store_service.h), backed by StoreKit -
/// store.achievements/store.cloud_saves/store.presence are never
/// registered: StoreKit has none of those subsystems (Game Center is a
/// separate, unrelated product for that), the same "simply doesn't provide
/// it" shape the other three mobile backends already established.
///
/// Every call here is asynchronous via StoreKit's own delegate/observer
/// callbacks (see store_app_store_platform.h for the plumbing) - both
/// classes therefore hold a small cache populated by their own query's
/// callback, with the neutral interface's synchronous methods reading
/// whatever is cached so far, the same eventually-consistent shape the
/// other three mobile backends already established.

class AppStoreCore final : public store::StoreCore {
public:
  explicit AppStoreCore(AppStorePlatform &platform) noexcept;
  ~AppStoreCore() override;

  /// StoreKit has no "own the base game" concept at all - the App Store
  /// already gates who can install/run the app, so a ready platform
  /// implies base ownership. A non-empty @p dlc_id checks the cache
  /// refresh_ownership() populates instead.
  [[nodiscard]] bool is_owned(nx::string_view dlc_id = {}) const override;
  [[nodiscard]] nx::vector<nx::string> owned_dlc_ids() const override {
    return m_owned_dlc_ids;
  }
  [[nodiscard]] nx::string_view store_name() const noexcept override {
    return "app_store";
  }

  /// Fires `SKPaymentQueue.restoreCompletedTransactions` - StoreKit's only
  /// way to learn which non-consumable products a signed-in Apple ID
  /// already owns, refreshing owned_dlc_ids() as each restored transaction
  /// arrives. @p dlc_id is ignored, same as
  /// store::StoreCore::refresh_ownership() documents for any bulk-capable
  /// backend.
  void refresh_ownership(nx::string_view dlc_id = {}) override;

  static void dispatch_transaction_restored(nx::string_view product_id);
  static void dispatch_restore_finished(bool success);

private:
  void on_transaction_restored(nx::string_view product_id);
  void on_restore_finished(bool success);

  AppStorePlatform &m_platform;
  nx::vector<nx::string> m_owned_dlc_ids;

  static AppStoreCore *s_instance;
};

class AppStoreIap final : public store::StoreIap {
public:
  explicit AppStoreIap(AppStorePlatform &platform) noexcept;
  ~AppStoreIap() override;

  [[nodiscard]] nx::vector<store::StoreProduct> products() const override {
    return m_products;
  }
  /// Requires a prior refresh_products() call naming @p product_id -
  /// `SKPayment` is created from a live `SKProduct` object, not a bare id
  /// string, so this backend must have one cached first (the same
  /// "query before purchase" shape `store_google_play`'s own purchase()
  /// re-fetch already establishes, just enforced up front here instead of
  /// re-fetched inline).
  bool purchase(nx::string_view product_id) override;
  [[nodiscard]] bool purchase_pending() const override { return m_purchase_pending; }
  [[nodiscard]] nx::string_view purchase_error() const override {
    return m_purchase_error.view();
  }

  /// Fires an `SKProductsRequest` for exactly the ids given - like the
  /// other three mobile backends, StoreKit has no "list everything" query,
  /// the game must know its own product ids up front. Also populates the
  /// `SKProduct` cache purchase() reads from.
  void refresh_products(const nx::vector<nx::string> &product_ids) override;

  static void dispatch_product_details_response(
      const nx::vector<store::StoreProduct> &products);
  static void dispatch_purchase_result(bool success, nx::string_view product_id);

private:
  void on_product_details_response(const nx::vector<store::StoreProduct> &products);
  void on_purchase_result(bool success, nx::string_view product_id);

  AppStorePlatform &m_platform;
  struct ProductCache;
  nx::unique_ptr<ProductCache> m_cache;
  nx::vector<store::StoreProduct> m_products;
  bool m_purchase_pending = false;
  nx::string m_purchase_error;

  static AppStoreIap *s_instance;
};

}
