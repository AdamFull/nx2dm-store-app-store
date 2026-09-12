#include "store_app_store/store_app_store_services.h"

#include <utility>

#import <StoreKit/StoreKit.h>

namespace nxm::store_app_store {
namespace {

[[nodiscard]] nx::string format_price(SKProduct *const product) {
  NSNumberFormatter *const formatter = [NSNumberFormatter new];
  formatter.numberStyle = NSNumberFormatterCurrencyStyle;
  formatter.locale = product.priceLocale;
  NSString *const display = [formatter stringFromNumber:product.price];
  return display != nil ? nx::string(display.UTF8String) : nx::string{};
}

} // namespace
}

/// Owns the `SKProduct` objects a products request returns, keyed by
/// product id - `AppStoreIap::purchase()` reads from here, since
/// `SKPayment` is built from a live `SKProduct`, not a bare id string.
/// This one delegate instance is reused across every refresh_products()
/// call rather than created per-request, so a product cached from an
/// earlier call survives until the next successful response replaces it.
@interface NXStoreProductsDelegate : NSObject <SKProductsRequestDelegate>
@property(nonatomic, strong) NSMutableDictionary<NSString *, SKProduct *> *products;
@end

@implementation NXStoreProductsDelegate

- (instancetype)init {
  if ((self = [super init])) {
    _products = [NSMutableDictionary dictionary];
  }
  return self;
}

- (void)productsRequest:(SKProductsRequest *)request
      didReceiveResponse:(SKProductsResponse *)response {
  nx::vector<nxm::store::StoreProduct> results;
  results.reserve(response.products.count);
  for (SKProduct *product in response.products) {
    self.products[product.productIdentifier] = product;
    nxm::store::StoreProduct entry;
    entry.id = nx::string(product.productIdentifier.UTF8String);
    entry.title = nx::string(product.localizedTitle.UTF8String);
    entry.price_display = nxm::store_app_store::format_price(product);
    results.push_back(std::move(entry));
  }
  nxm::store_app_store::AppStoreIap::dispatch_product_details_response(results);
}

- (void)request:(SKRequest *)request didFailWithError:(NSError *)error {
  nxm::store_app_store::AppStoreIap::dispatch_product_details_response({});
}

@end

namespace nxm::store_app_store {

// -- AppStoreCore -----------------------------------------------------------

AppStoreCore *AppStoreCore::s_instance = nullptr;

AppStoreCore::AppStoreCore(AppStorePlatform &platform) noexcept : m_platform(platform) {
  s_instance = this;
}

AppStoreCore::~AppStoreCore() {
  if (s_instance == this)
    s_instance = nullptr;
}

bool AppStoreCore::is_owned(const nx::string_view dlc_id) const {
  if (!m_platform.ready())
    return false;
  if (dlc_id.empty())
    return true;
  for (const nx::string &id : m_owned_dlc_ids)
    if (id.view() == dlc_id)
      return true;
  return false;
}

void AppStoreCore::refresh_ownership(const nx::string_view) {
  if (!m_platform.ready())
    return;
  m_owned_dlc_ids.clear();
  [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
}

void AppStoreCore::on_transaction_restored(const nx::string_view product_id) {
  for (const nx::string &id : m_owned_dlc_ids)
    if (id.view() == product_id)
      return;
  m_owned_dlc_ids.push_back(nx::string(product_id));
}

void AppStoreCore::on_restore_finished(const bool) {
  // Nothing further to do - every restored transaction already arrived
  // through dispatch_transaction_restored() by the time StoreKit calls
  // this. Kept as its own method (rather than folded away) so a future
  // "restore in progress" flag has an obvious place to clear.
}

void AppStoreCore::dispatch_transaction_restored(const nx::string_view product_id) {
  if (s_instance != nullptr)
    s_instance->on_transaction_restored(product_id);
}

void AppStoreCore::dispatch_restore_finished(const bool success) {
  if (s_instance != nullptr)
    s_instance->on_restore_finished(success);
}

// -- AppStoreIap --------------------------------------------------------------

struct AppStoreIap::ProductCache {
  NXStoreProductsDelegate *delegate = [NXStoreProductsDelegate new];
};

AppStoreIap *AppStoreIap::s_instance = nullptr;

AppStoreIap::AppStoreIap(AppStorePlatform &platform) noexcept
    : m_platform(platform), m_cache(nx::make_unique<ProductCache>()) {
  s_instance = this;
}

AppStoreIap::~AppStoreIap() {
  if (s_instance == this)
    s_instance = nullptr;
}

bool AppStoreIap::purchase(const nx::string_view product_id) {
  if (!m_platform.ready())
    return false;
  NSString *const identifier =
      [NSString stringWithUTF8String:nx::string(product_id).c_str()];
  SKProduct *const product = m_cache->delegate.products[identifier];
  if (product == nil)
    return false;

  m_purchase_pending = true;
  m_purchase_error = nx::string{};
  [[SKPaymentQueue defaultQueue] addPayment:[SKPayment paymentWithProduct:product]];
  return true;
}

void AppStoreIap::refresh_products(const nx::vector<nx::string> &product_ids) {
  if (!m_platform.ready())
    return;
  NSMutableSet<NSString *> *const identifiers =
      [NSMutableSet setWithCapacity:product_ids.size()];
  for (const nx::string &id : product_ids)
    [identifiers addObject:[NSString stringWithUTF8String:id.c_str()]];

  SKProductsRequest *const request =
      [[SKProductsRequest alloc] initWithProductIdentifiers:identifiers];
  request.delegate = m_cache->delegate;
  [request start];
}

void AppStoreIap::on_product_details_response(
    const nx::vector<store::StoreProduct> &products) {
  m_products = products;
}

void AppStoreIap::on_purchase_result(const bool success, const nx::string_view) {
  m_purchase_pending = false;
  m_purchase_error = success ? nx::string{} : nx::string("App Store purchase failed");
}

void AppStoreIap::dispatch_product_details_response(
    const nx::vector<store::StoreProduct> &products) {
  if (s_instance != nullptr)
    s_instance->on_product_details_response(products);
}

void AppStoreIap::dispatch_purchase_result(const bool success,
                                           const nx::string_view product_id) {
  if (s_instance != nullptr)
    s_instance->on_purchase_result(success, product_id);
}

}
