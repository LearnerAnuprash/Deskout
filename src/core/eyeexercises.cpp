#include "core/eyeexercises.h"

#include "core/settingskeys.h"

#include <QCoreApplication>
#include <QSettings>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("EyeExercises", text);
}

} // namespace

namespace EyeExercises {

QList<EyeExercise> all()
{
    return {
        {tr("20-20-20 rule"),
         tr("Look at something at least 20 feet (6 metres) away and keep your focus there "
            "until the timer ends."),
         20},
        {tr("Focus shifting"),
         tr("Hold a finger a few centimetres from your eyes and focus on it. Slowly move it "
            "away, then shift your focus to something far away. Repeat three times."),
         30},
        {tr("Eye rolls"),
         tr("Slowly roll your eyes in a full circle clockwise five times, then "
            "counter-clockwise five times. Keep your head still."),
         30},
        {tr("Palming"),
         tr("Rub your palms together until they are warm, then cup them gently over your "
            "closed eyes without pressing. Breathe slowly and relax."),
         30},
        {tr("Blink reset"),
         tr("Blink quickly 15 times to refresh your eyes, then close them and rest until "
            "the timer ends."),
         20},
        {tr("Figure eight"),
         tr("Imagine a large figure eight on the floor about 3 metres away. Trace it slowly "
            "with your eyes, first one way, then the other."),
         30},
    };
}

EyeExercise takeNext()
{
    const QList<EyeExercise> exercises = all();
    QSettings settings;
    const int index = settings.value(SettingsKeys::EyeExerciseNext, 0).toInt();
    const int current = (index >= 0 && index < exercises.size()) ? index : 0;
    settings.setValue(SettingsKeys::EyeExerciseNext, (current + 1) % exercises.size());
    return exercises.at(current);
}

} // namespace EyeExercises
