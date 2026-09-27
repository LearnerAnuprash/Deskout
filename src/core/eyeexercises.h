#pragma once

#include <QList>
#include <QString>

// Built-in eye exercises shown after an eye break is confirmed.
struct EyeExercise
{
    QString title;
    QString instructions;
    int seconds = 20;
};

namespace EyeExercises {

QList<EyeExercise> all();

// Returns the next exercise in rotation and advances the persisted index,
// so consecutive breaks cycle through the whole library.
EyeExercise takeNext();

} // namespace EyeExercises
