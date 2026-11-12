#pragma once

#include <QJsonObject>
#include <QPair>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for the contact list: reading it, adding one user, removing
/// users and importing the device address book.
///
/// They shape TDLib's JSON payload only: no logging, no @extra (the transport
/// adds the correlation id) and no session, network or cache state.
///
/// Shapes follow the TDLib schema pinned by this client (1.8.69, commit
/// 4c6377aa, see docs/WINDOWS_TDLIB.md):
///
///   importedContact phone_number:string first_name:string last_name:string
///                   note:formattedText = ImportedContact;
///   addContact user_id:int53 contact:importedContact
///              share_phone_number:Bool = Ok;
///   importContacts contacts:vector<importedContact> = ImportedContacts;
///   getContacts = Users;
///   removeContacts user_ids:vector<int53> = Ok;
///
/// Three schema details drive the shapes below:
///   - importedContact carries a `note` formattedText. The JSON interface
///     treats it as optional and TDLib only edits a contact's note when the
///     field is present, so the builders leave it out. Writing an empty
///     formattedText instead would erase a note the user already wrote.
///   - getContacts takes no parameters at all. TDLib answers it with a contact
///     search for an empty query and a limit of one million, so the whole
///     contact list arrives in a single Users reply; there is no server-side
///     page to ask for.
///   - the schema bounds a contact name at 1-64 characters for the first name
///     and 0-64 for the last name, counted in Unicode code points. Names are
///     trimmed and then cut to 64 code points: anything longer could only be
///     rejected by the server, and for an address-book import one overlong
///     name must not fail the whole batch.

/// Builds getContacts, the request that returns every contact of the account.
///
/// offset and limit describe the caller's page window and do not appear in the
/// payload: this schema has no paging for getContacts, so the caller slices the
/// returned Users.user_ids itself. Writing offset/limit would not fail (TDLib
/// ignores fields it does not know) but would advertise a paging guarantee that
/// the reply does not have.
[[nodiscard]] QJsonObject getContactsRequest(int offset, int limit);

/// Builds addContact for a user the account already knows, adding them to the
/// contact list or editing their entry.
///
/// share_phone_number is always false. Adding somebody must never hand them the
/// current user's phone number behind their back; a flow that really wants to
/// share it needs an explicit opt-in and its own request.
///
/// The contact carries the trimmed phone number (TDLib allows an empty number
/// when the caller does not know it) and no note, so an existing note is left
/// alone. userId must be a real user id: a non-positive value is written as-is
/// and rejected by TDLib, because this builder has no session to look a user up.
[[nodiscard]] QJsonObject addContactRequest(qint64 userId, const QString& firstName,
                                            const QString& lastName, const QString& phone);

/// Builds removeContacts for a set of users.
///
/// Ids that are not positive are dropped (no TDLib user has one) and repeated
/// ids are collapsed, keeping first-seen order. An empty result is still a
/// valid request: TDLib resolves it without touching the server, so a delete
/// action with nothing selected is a no-op rather than an error.
[[nodiscard]] QJsonObject removeContactsRequest(const QVector<qint64>& userIds);

/// Builds importContacts from the device address book. Each pair is
/// (phone number, display name), so `.first` is the number and `.second` the
/// name; the pair carries no other meaning.
///
/// The name goes into first_name in full and last_name stays empty. A display
/// name does not say where a given name ends and a family name starts, and
/// guessing would silently rewrite names such as "van der Berg" or
/// "Анна-Мария Ивановна"; Telegram displays the combined name unchanged.
///
/// One importedContact is emitted per pair, in the same order, even when the
/// number is empty: ImportedContacts.user_ids is positional, so user_ids[i]
/// belongs to phoneAndName[i]. A contact that is not a registered user comes
/// back as 0 and must be skipped by the caller.
[[nodiscard]] QJsonObject importContactsRequest(
    const QVector<QPair<QString, QString>>& phoneAndName);

} // namespace beygram::telegram
