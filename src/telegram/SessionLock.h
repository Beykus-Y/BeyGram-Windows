#pragma once

#include "core/Result.h"

#include <QLockFile>
#include <QString>

namespace beygram::telegram {

/// Prevents two BeyGram processes from opening the same TDLib database.
/// QLockFile also detects a lock left behind by a dead process.
class SessionLock {
public:
    explicit SessionLock(QString lockFilePath);

    [[nodiscard]] Status acquire();
    void release();
    [[nodiscard]] bool isLocked() const;
    [[nodiscard]] QString path() const;

private:
    QString m_path;
    QLockFile m_lock;
};

} // namespace beygram::telegram
