#include "app/readingmode.h"

#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

namespace {

using Availability = GrayscaleBackend::Availability;

// Scriptable stand-in for the platform filter.
struct FakeState
{
    Availability availability = Availability::Ready;
    bool applied = false;
    bool failApply = false;
    int installs = 0;
};

class FakeBackend final : public GrayscaleBackend
{
public:
    explicit FakeBackend(FakeState *state) : m_state(state) {}
    QString name() const override { return QStringLiteral("fake"); }
    Status status() override { return {m_state->availability, QStringLiteral("because")}; }
    bool install(QString *) override
    {
        ++m_state->installs;
        m_state->availability = Availability::NeedsRelogin;
        return true;
    }
    bool apply(bool grayscale, QString *error) override
    {
        if (m_state->failApply) {
            if (error)
                *error = QStringLiteral("refused");
            return false;
        }
        m_state->applied = grayscale;
        return true;
    }

private:
    FakeState *m_state;
};

} // namespace

class TestReadingMode : public QObject
{
    Q_OBJECT

private:
    FakeState m_fake;
    std::unique_ptr<GrayscaleBackend> backend() { return std::make_unique<FakeBackend>(&m_fake); }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_readingmode"));
    }

    void init()
    {
        QSettings().clear();
        m_fake = FakeState();
    }

    void toggleWhenReady()
    {
        ReadingMode mode(backend());
        QVERIFY(!mode.isEnabled());
        QVERIFY(mode.setEnabled(true));
        QVERIFY(mode.isActive());
        QVERIFY(m_fake.applied);
        QVERIFY(mode.setEnabled(false));
        QVERIFY(!mode.isEnabled());
        QVERIFY(!m_fake.applied);
    }

    void choicePersistsButFilterIsRemovedOnQuit()
    {
        {
            ReadingMode mode(backend());
            mode.setEnabled(true);
        } // destructor = app quit
        QVERIFY(!m_fake.applied);

        ReadingMode restarted(backend());
        QVERIFY(restarted.isEnabled());
        QVERIFY(!restarted.isActive());
        restarted.start();
        QVERIFY(restarted.isActive());
        QVERIFY(m_fake.applied);
    }

    void unsupportedRefusesAndExplains()
    {
        m_fake.availability = Availability::Unsupported;
        ReadingMode mode(backend());
        QString message;
        QVERIFY(!mode.setEnabled(true, &message));
        QCOMPARE(message, QStringLiteral("because"));
        QVERIFY(!mode.isEnabled());
        QVERIFY(!QSettings().value(QStringLiteral("readingMode/enabled")).toBool());
    }

    void applyFailureTurnsChoiceOff()
    {
        m_fake.failApply = true;
        ReadingMode mode(backend());
        QString message;
        QVERIFY(!mode.setEnabled(true, &message));
        QCOMPARE(message, QStringLiteral("refused"));
        QVERIFY(!mode.isEnabled());
    }

    void reloginKeepsChoiceAndRetriesUntilReady()
    {
        m_fake.availability = Availability::NeedsInstall;
        ReadingMode mode(backend());
        mode.setRetryPolicy(10, 50);
        QVERIFY(mode.install(nullptr));
        QCOMPARE(m_fake.installs, 1);

        QString message;
        QVERIFY(!mode.setEnabled(true, &message));
        QVERIFY(mode.isEnabled());
        QVERIFY(mode.isPending());
        QCOMPARE(message, QStringLiteral("because"));

        // The shell loads the extension (e.g. after login).
        QSignalSpy changed(&mode, &ReadingMode::changed);
        m_fake.availability = Availability::Ready;
        QTRY_VERIFY(mode.isActive());
        QVERIFY(m_fake.applied);
        QVERIFY(!mode.isPending());
        QVERIFY(changed.count() >= 1);
    }

    void turningOffWhilePendingStopsRetrying()
    {
        m_fake.availability = Availability::NeedsRelogin;
        ReadingMode mode(backend());
        mode.setRetryPolicy(10, 50);
        mode.setEnabled(true);
        QVERIFY(mode.isPending());
        mode.setEnabled(false);
        m_fake.availability = Availability::Ready;
        QTest::qWait(60);
        QVERIFY(!mode.isActive());
        QVERIFY(!m_fake.applied);
    }
};

QTEST_GUILESS_MAIN(TestReadingMode)
#include "tst_readingmode.moc"
