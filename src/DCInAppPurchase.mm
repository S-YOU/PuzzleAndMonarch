//
// 課金処理
//

#import "DCInAppPurchase.h"
#include <functional>
#include "AppText.hpp"
#include "Cocoa.h"


namespace ngs
{

// FIXME グローバル変数をやめる
std::function<void ()> purchase_completed;
std::function<void ()> purchase_canceled;


NSString* localizeText(const char* text)
{
  return createString(AppText::get(text));
}

}


@implementation DCInAppPurchase

- (id)init
{
  DOUT << "DCInAppPurchase:init" << std::endl;

  if (self = [super init])
  {
    [[SKPaymentQueue defaultQueue] addTransactionObserver:self];
  }
    
  return self;
}

- (void)dealloc
{
  DOUT << "DCInAppPurchase:dealloc" << std::endl;

  [[SKPaymentQueue defaultQueue] removeTransactionObserver:self];
  [super dealloc];
}


// アプリ内課金処理を開始
- (void)startPurchase:(NSString *)productId view:(id)vc
{
  if (![self canMakePayments])
  {
    // アプリ内課金が許可されていなければアラートを出して終了
    [self showAlert:ngs::localizeText("Purchase06") message:ngs::localizeText("Purchase05")];

    return;
  }
    
  // プロダクトID保持
  proccessingProductId = productId;
    
  // ビュー保持
  rootView = vc;

  DOUT << "DCInAppPurchase:startPurchase" << std::endl;

  // プロダクト情報の取得処理を開始
  NSSet *set = [NSSet setWithObjects:proccessingProductId, nil];
  SKProductsRequest *productsRequest = [[[SKProductsRequest alloc] initWithProductIdentifiers:set] autorelease];
  productsRequest.delegate = self;
  [productsRequest start];
}

// デリゲートメソッド (終了通知)
- (void)requestDidFinish:(SKRequest *)request
{
  DOUT << "DCInAppPurchase:requestDidFinish" << std::endl;
}

// デリゲートメソッド (アクセスエラー)
- (void)request:(SKRequest *)request didFailWithError:(NSError *)error
{
  DOUT << "DCInAppPurchase:request:didFailWithError" << std::endl;
  [self showAlert:ngs::localizeText("Purchase06") message:[error localizedDescription]];
}

// デリゲートメソッド (プロダクト情報を取得)
- (void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response
{
  DOUT << "DCInAppPurchase:productsRequest:didReceiveResponse" << std::endl;

  // レスポンスがなければエラー処理
  if (response == nil)
  {
    [self showAlert:ngs::localizeText("Purchase06") message:ngs::localizeText("Purchase07")];
    return;
  }
    
  // プロダクトIDが無効な場合はアラートを出して終了
  if ([response.invalidProductIdentifiers count] > 0)
  {
    [self showAlert:ngs::localizeText("Purchase06") message:ngs::localizeText("Purchase08")];
    return;
  }

  // 購入処理開始
  for (SKProduct *product in response.products)
  {
    SKPayment *payment = [SKPayment paymentWithProduct:product];
    [[SKPaymentQueue defaultQueue] addPayment:payment];
  }
}

// 購入完了時の処理
- (void)completeTransaction:(SKPaymentTransaction *)transaction
{
  DOUT << "DCInAppPurchase:completeTransaction" << std::endl;
  // トランザクション記録
  [self recordTransaction:transaction];
    
  // コンテンツ提供記録
  [self provideContent:transaction.payment.productIdentifier];
}

// リストア完了時の処理
- (void)restoreTransaction:(SKPaymentTransaction *)transaction
{
  DOUT << "DCInAppPurchase:restoreTransaction" << std::endl;
  // トランザクション記録
  [self recordTransaction:transaction];
    
  // コンテンツ提供記録
  [self provideContent:transaction.originalTransaction.payment.productIdentifier];
}

// トランザクション記録
- (void)recordTransaction:(SKPaymentTransaction *)transaction
{
  NSLog(@"%@", transaction);
}

// コンテンツ提供
- (void)provideContent:(NSString *)productIdentifier
{
  ngs::purchase_completed();
}


// デリゲートメソッド (購入処理開始後に状態が変わるごとに随時コールされる)
- (void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray *)transactions
{
  for (SKPaymentTransaction *transaction in transactions)
  {
    switch (transaction.transactionState)
    {
    case SKPaymentTransactionStatePurchasing:
      {
        // 購入処理開始
        DOUT << "SKPaymentTransactionStatePurchasing" << std::endl;

        // インジケーター表示開始
        [self startActivityIndicator];
      }
      break;

    case SKPaymentTransactionStatePurchased:
      {
        // 購入処理成功
        DOUT << "SKPaymentTransactionStatePurchased" << std::endl;
            
        // 該当するプロダクトのロックを解除する
        [self completeTransaction:transaction];
        // ペイメントキューからトランザクションを削除
        [queue finishTransaction:transaction];
            
        // インジケータ非表示
        [self stopActivityIndicator];
      }
      break;

    case SKPaymentTransactionStateFailed:
      {
        // エラー(キャンセル含む)
        DOUT << "SKPaymentTransactionStateFailed" << std::endl;

        // ユーザーによるキャンセルでなければアラートを出す
        if (transaction.error.code != SKErrorPaymentCancelled)
        {
          [self showAlert:ngs::localizeText("Purchase06") message:[transaction.error localizedDescription]];
        }
        // ペイメントキューからトランザクションを削除
        [queue finishTransaction:transaction];
            
        ngs::purchase_canceled();

        // インジケータ非表示
        [self stopActivityIndicator];
      }
      break;

    case SKPaymentTransactionStateRestored:
      {
        DOUT << "SKPaymentTransactionStateRestored" << std::endl;

        // リストア処理完了
        isRestored = YES;
        [self showAlert:ngs::localizeText("Purchase09") message:ngs::localizeText("Purchase10")];
            
        // 購入済みのプロダクトのロックを再解除する
        [self restoreTransaction:transaction];
        // ペイメントキューからトランザクションを削除
        [queue finishTransaction:transaction];
      }
      break;

    case SKPaymentTransactionStateDeferred:
      {
        // 課金処理の遅延
        DOUT << "SKPaymentTransactionStateDeferred" << std::endl;
        
        // ユーザーによるキャンセルでなければアラートを出す
        if (transaction.error.code != SKErrorPaymentCancelled)
        {
          [self showAlert:ngs::localizeText("Purchase06") message:[transaction.error localizedDescription]];
        }
        ngs::purchase_canceled();

        // トランザクションの削除はしない

        // インジケータ非表示
        [self stopActivityIndicator];
      }
      break;
    }
  }
}

// リストア
- (void)restorePurchase:(NSString *)productId view:(id)vc
{
  DOUT << "DCInAppPurchase:restorePurchase" << std::endl;
    
  // プロダクトID保持
  proccessingProductId = productId;
    
  // View保持
  rootView = vc;
  // リストア成功したかどうか??
  isRestored = NO;

  // インジゲーター表示開始
  [self startActivityIndicator];

  // 購入済みプラグインのリストア処理を開始する
  [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
}

// リストア完了
- (void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue
{
  DOUT << "DCInAppPurchase:paymentQueueRestoreCompletedTransactionsFinished" << std::endl;

  // 購入済みでなかった場合アラート表示
  if (!isRestored)
  {
    [self showAlert:ngs::localizeText("Purchase06") message:ngs::localizeText("Purchase11")];
    ngs::purchase_canceled();
  }
            
  // インジケータ非表示
  [self stopActivityIndicator];
}

// リストア中断(キャンセルも含む)
- (void)paymentQueue:(SKPaymentQueue *)queue restoreCompletedTransactionsFailedWithError:(NSError *)error
{
  DOUT << "DCInAppPurchase:paymentQueue:restoreCompletedTransactionsFailedWithError" << std::endl;
  for (SKPaymentTransaction *transaction in queue.transactions)
  {
    NSLog(@"%@", transaction);
        
    // リストア失敗のアラート表示
    [self showAlert:ngs::localizeText("Purchase06") message:ngs::localizeText("Purchase12")];
  }

  ngs::purchase_canceled();

  // インジケータ非表示
  [self stopActivityIndicator];
}


// インジケーター表示
- (void)startActivityIndicator
{
  UIView* view = [(UIViewController*)rootView view];
  [self startActivityIndicator:view
      center:CGPointMake(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2)
      styleId:AI_WHITE hidesWhenStopped:YES];
}

// アクティビティインジケータのアニメーション開始
- (void)startActivityIndicator:(id)view center:(CGPoint)center styleId:(NSInteger)styleId hidesWhenStopped:(BOOL)hidesWhenStopped
{
  // インジケーター初期化
  indicator = [[[UIActivityIndicatorView alloc] init] autorelease];
    
  // スタイルを設定
  switch (styleId) {
  case AI_GRAY:
    indicator.activityIndicatorViewStyle = UIActivityIndicatorViewStyleGray;
    break;
  case AI_WHITE:
    indicator.activityIndicatorViewStyle = UIActivityIndicatorViewStyleWhite;
    break;
  case AI_WHITE_LARGE:
    indicator.activityIndicatorViewStyle = UIActivityIndicatorViewStyleWhiteLarge;
    break;
  }
    
  // スタイルに応じて寸法変更
  if (indicator.activityIndicatorViewStyle == UIActivityIndicatorViewStyleWhiteLarge) {
    indicator.frame = CGRectMake(0, 0, AI_LARGE_SIZE, AI_LARGE_SIZE);
  } else {
    indicator.frame = CGRectMake(0, 0, AI_SMALL_SIZE, AI_SMALL_SIZE);
  }
    
  // 座標をセンターに指定
  indicator.center = center;
    
  // インジケーターアニメーション開始
  [indicator startAnimating];
    
  // オーバーレイ表示
  indicatorOverlay = [[[UIView alloc] initWithFrame:CGRectMake(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT)] autorelease];
  indicatorOverlay.backgroundColor = AI_BG_COLOR;
  [view addSubview:indicatorOverlay];
    
  // 画面に追加
  [view addSubview:indicator];
}

// アクティビティインジケータのアニメーション停止
- (void)stopActivityIndicator
{
  [indicatorOverlay removeFromSuperview];
  indicatorOverlay = nil;

  [indicator removeFromSuperview];
  indicator = nil;
}

// アクティビティインジケータがアニメーション中であるか
- (BOOL)isAnimatingActivityIndicator
{
  return [indicator isAnimating];
}

// アラート表示
- (void)showAlert:(NSString *)title message:(NSString *)message
{
  UIAlertController* alert = [UIAlertController alertControllerWithTitle:title message:message
                                 preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* okAction = [UIAlertAction actionWithTitle:ngs::localizeText("Purchase13")
                                style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction * action)
                                {
                                  // ボタンタップ時の処理
                                  NSLog(@"OK button tapped.");
                                }];
  [alert addAction:okAction];

  [rootView presentViewController:alert animated:YES completion:nil];
}

// アプリ内課金が許可されているか
- (BOOL)canMakePayments
{
  return [SKPaymentQueue canMakePayments];
}

@end
