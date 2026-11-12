#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for TDLib's invoice payment and Telegram Stars requests. They
/// shape the JSON payload only: no logging, no secrets and no @extra (the
/// transport adds the correlation id).
///
/// Shape follows the pinned td_api.tl (TDLib 1.8.69, commit 4c6377aa, see
/// docs/WINDOWS_TDLIB.md):
///
///   inputInvoiceMessage chat_id:int53 message_id:int53 = InputInvoice;
///   themeParameters background_color:int32 secondary_background_color:int32
///                   header_background_color:int32 bottom_bar_background_color:int32
///                   section_background_color:int32 section_separator_color:int32
///                   text_color:int32 accent_text_color:int32
///                   section_header_text_color:int32 subtitle_text_color:int32
///                   destructive_text_color:int32 hint_color:int32 link_color:int32
///                   button_color:int32 button_text_color:int32 = ThemeParameters;
///   address country_code:string state:string city:string street_line1:string
///           street_line2:string postal_code:string = Address;
///   orderInfo name:string phone_number:string email_address:string
///             shipping_address:address = OrderInfo;
///   inputCredentialsNew data:string allow_save:Bool = InputCredentials;
///
///   getPaymentForm input_invoice:InputInvoice theme:themeParameters = PaymentForm;
///   validateOrderInfo input_invoice:InputInvoice order_info:orderInfo
///                    allow_save:Bool = ValidatedOrderInfo;
///   sendPaymentForm input_invoice:InputInvoice payment_form_id:int64
///                   order_info_id:string shipping_option_id:string
///                   credentials:InputCredentials tip_amount:int53 = PaymentResult;
///
///   getStarTransactions owner_id:MessageSender subscription_id:string
///                       direction:TransactionDirection offset:string
///                       limit:int32 = StarTransactions;
///
/// The pinned schema addresses an invoice through an InputInvoice object, not
/// through a bare chat/message pair, so every builder below wraps the pair in
/// inputInvoiceMessage. The same wrapper covers an invoice message
/// (messageInvoice) and the paid media of a messagePaidMedia message; TDLib
/// reads the message and decides which invoice it is.

/// Builds getPaymentForm for the invoice or paid-media message (chatId,
/// messageId).
///
/// theme is sent as null, the schema's "use the default theme": this client has
/// no payment-form palette, and a themeParameters object would have to repeat
/// all 15 colours TDLib requires.
///
/// The request only reads a form. It never pays anything, so it is safe to
/// build for any message the caller believes carries an invoice; a message
/// without one is rejected by TDLib.
[[nodiscard]] QJsonObject getPaymentFormRequest(qint64 chatId, qint64 messageId);

/// Builds validateOrderInfo for the invoice message, asking TDLib to validate
/// the user's order information and return the available shipping options.
///
/// orderInfo is normalized to exactly the schema's orderInfo shape: name,
/// phone_number and email_address become empty strings when absent, and
/// shipping_address always becomes a complete address with all six fields,
/// empty when the caller supplied none. Values that are not strings are treated
/// as "not provided" instead of being stringified, and keys outside the schema
/// are dropped, so the payload cannot carry a field TDLib would ignore anyway.
/// The values themselves are not checked here: an empty name or a malformed
/// phone number is TDLib's to reject, and the answer (validatedOrderInfo) is
/// what tells the caller whether the order is usable.
///
/// allowSave is forwarded verbatim. It asks TDLib to keep the order
/// information for the account so a later form can reuse it; the caller owns
/// that consent decision.
[[nodiscard]] QJsonObject validateOrderInfoRequest(qint64 chatId, qint64 messageId,
                                                   const QJsonObject& orderInfo,
                                                   bool allowSave = true);

/// Builds sendPaymentForm for a payment in Telegram Stars.
///
/// The pinned method sends no invoice title, payload or provider token: the
/// invoice is identified by input_invoice, and a regular (real-money) payment
/// needs credentials produced by the provider's own SDK, which this signature
/// cannot express. The builder therefore only completes a Stars form:
///   - order_info_id is the empty string (this signature carries no
///     validateOrderInfo result),
///   - shipping_option_id is shipping.id, or the empty string when the caller
///     passed no shipping choice (a Stars form has no shipping, so an empty
///     object is the normal input),
///   - credentials is null, the schema's "payment in Telegram Stars",
///   - tip_amount is 0 (no tips).
///
/// title and payload are accepted so a caller can pass the bot invoice it
/// validated, but the pinned schema has no field for either, so they do not
/// change the request.
///
/// A non-empty providerToken marks a regular (real-money) invoice. Such a form
/// cannot be paid through this builder: the client hands real-money forms to
/// the provider's validated page instead of submitting card data itself, and
/// the credentials TDLib needs are produced by the provider's SDK, not by this
/// signature. A non-positive paymentFormId is not a form identifier
/// getPaymentForm could have returned either. In both cases the builder returns
/// an empty object, which the caller must not send, rather than dispatching a
/// payment request that cannot succeed.
[[nodiscard]] QJsonObject sendPaymentFormRequest(qint64 chatId, qint64 messageId,
                                                 qint64 paymentFormId,
                                                 const QString& title,
                                                 const QString& payload,
                                                 const QString& providerToken,
                                                 const QJsonObject& shipping);

/// Builds the first getStarTransactions page: subscription_id is empty (all
/// transactions), direction is null (incoming and outgoing), and offset is the
/// empty cursor.
///
/// owner_id is a required MessageSender in the pinned schema and this
/// two-argument form has no account id to name. It sends messageSenderUser with
/// user id 0, which is not an account: the schema has no "current user" owner
/// token, so a pinned server rejects the request. The form exists so the
/// payload can be assembled before getMe() resolves; a caller that has the
/// signed-in user id must use the owner overload below.
///
/// offset is an opaque cursor string, not a page number. A value <= 0 is the
/// empty cursor that asks for the first chunk; a positive value is sent as its
/// decimal string and will not match a real cursor. Paging therefore needs the
/// cursor overload below, fed with starTransactions.next_offset.
///
/// limit is clamped to 1..100: TDLib rejects a negative limit and Telegram
/// returns at most 100 transactions per chunk, so 0 would only ask for an empty
/// page.
[[nodiscard]] QJsonObject getStarTransactionsRequest(int offset, int limit);

/// Builds the same getStarTransactions page for the account or owned bot named
/// by ownerUserId (the signed-in user id comes from getMe), with the same
/// subscription, direction, cursor and limit rules as above.
[[nodiscard]] QJsonObject getStarTransactionsRequest(qint64 ownerUserId, int offset,
                                                     int limit);

/// Builds a getStarTransactions page for the opaque cursor returned as
/// starTransactions.next_offset; pass an empty string for the first chunk. limit
/// is clamped to 1..100 and every other field matches the owner overload above.
[[nodiscard]] QJsonObject getStarTransactionsRequest(qint64 ownerUserId,
                                                     const QString& offset, int limit);

} // namespace beygram::telegram
