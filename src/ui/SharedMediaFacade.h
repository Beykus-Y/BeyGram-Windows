#pragma once

#include "telegram/SharedMediaModel.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// QML-facing state for the shared-media section of a chat profile.
///
/// The facade is a thin shell over telegram::SharedMediaModel: the model owns
/// the tab, de-duplicates rows by message id and keeps them newest first, while
/// the facade turns the QML string lists into model items and exposes the
/// result back as parallel string lists. Nothing here talks to TDLib, loads a
/// file or formats a label: the owner hands ids, titles and dates in, QML
/// renders them, and a tapped row comes back out through itemActivated().
///
/// tabs() is fixed and follows the order of telegram::SharedMediaTab:
/// Photos, Videos, Files, Links, Voice, Music, GIFs. currentTab() is an index
/// into that list and therefore also the enum value, so a tab switch always
/// describes the same filter to the owner.
///
/// ids() and titles() are parallel and describe the loaded page of the current
/// tab. ids()[i] is the message id that carries the attachment, titles()[i] its
/// display label. The dates only order the list and are not exposed.
///
/// setItems() and appendItems() take three parallel lists: ids, titles and
/// dates (Unix seconds, as strings). Lists of different lengths describe
/// inconsistent data, so the call is ignored and the loaded page stays. An id
/// that is missing, unparsable or not positive is dropped together with its
/// title and date; a date that is not a number is read as 0. A repeated id
/// keeps its first copy. Both calls normalize the list to newest first by date
/// and then by descending message id.
///
/// Every value QML can read is a property with the single changed() notify
/// signal. A mutation emits changed() only when the exposed state (tab, ids or
/// titles) really differs, and only once per call; a date edit that does not
/// move a row is invisible and stays silent.
class SharedMediaFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList tabs READ tabs NOTIFY changed)
    Q_PROPERTY(int currentTab READ currentTab NOTIFY changed)
    Q_PROPERTY(QStringList titles READ titles NOTIFY changed)
    Q_PROPERTY(QStringList ids READ ids NOTIFY changed)

public:
    explicit SharedMediaFacade(QObject* parent = nullptr);

    /// The seven tab labels, in the order of telegram::SharedMediaTab. Never
    /// empty.
    [[nodiscard]] QStringList tabs() const;

    /// Index into tabs(), 0..6. Starts at 0 (Photos).
    [[nodiscard]] int currentTab() const;

    /// Display labels of the loaded rows, parallel to ids().
    [[nodiscard]] QStringList titles() const;

    /// Message ids of the loaded rows, parallel to titles().
    [[nodiscard]] QStringList ids() const;

    /// Switches to the tab at \a index. An index outside tabs() is ignored and
    /// the current tab is kept. A different tab starts empty because every tab
    /// has its own TDLib filter; selecting the current tab again is a no-op.
    Q_INVOKABLE void selectTab(int index);

    /// Replaces the loaded page of the current tab. See the class comment for
    /// the parallel-list, id and date rules. Empty lists clear the page.
    Q_INVOKABLE void setItems(const QStringList& ids, const QStringList& titles,
                              const QStringList& dates);

    /// Adds the rows whose message id is not loaded yet and re-sorts the page.
    /// Rows already stored keep their payload. See setItems() for the parallel
    /// list and parsing rules.
    Q_INVOKABLE void appendItems(const QStringList& ids, const QStringList& titles,
                                 const QStringList& dates);

    /// Asks the owner to open the row at \a index by emitting
    /// itemActivated(ids()[index]). An index outside the loaded page is
    /// ignored and emits nothing. The call never changes the facade state.
    Q_INVOKABLE void activate(int index);

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted by activate(); \a messageId is the id of the tapped row.
    void itemActivated(const QString& messageId);

private:
    /// Everything a QML binding can read. Mutators snapshot it, run, and emit
    /// changed() only when the snapshot moved, so a no-op call stays silent.
    struct State {
        int tab = 0;
        QStringList ids;
        QStringList titles;

        bool operator==(const State&) const = default;
    };

    [[nodiscard]] State state() const;
    void emitIfChanged(const State& before);

    telegram::SharedMediaModel m_model;
};

} // namespace beygram::ui
