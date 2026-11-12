#include "UiFacades.h"

namespace beygram::ui {

UiFacades::UiFacades(QObject* parent) : QObject(parent)
{
    // The facades are value members, so their lifetime follows this object and
    // no QObject parent is needed. Nothing is connected here: AppController
    // owns the wiring between request signals and the repository.
}

} // namespace beygram::ui
