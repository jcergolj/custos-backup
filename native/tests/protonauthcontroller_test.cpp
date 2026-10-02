#include "../src/protonauthcontroller.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class ProtonAuthControllerTest final : public QObject
{
    Q_OBJECT

    static bool writeFile(const QString &path, const QByteArray &contents, bool executable = false)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()) {
            return false;
        }
        if (executable) {
            return file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        }
        return true;
    }

private slots:
    void closingDuringAProbeStopsTheProcessCleanly()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString binary = home.filePath("fake proton-drive");
        QVERIFY(writeFile(binary, "#!/bin/sh\nexec /bin/sleep 30\n", true));
        auto *auth = new ProtonAuthController(binary);
        QTRY_VERIFY(auth->checking());
        delete auth;
    }

    void probesWithoutBlockingAndRefreshesAfterLogin()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString binary = home.filePath("fake proton-drive");
        QVERIFY(writeFile(binary, "#!/bin/sh\nbase=${0%/*}\n/bin/sleep 0.1\n"
                          "if [ \"$*\" != 'filesystem info /my-files --json' ]; then exit 2; fi\n"
                          "if [ -f \"$base/authenticated\" ]; then exit 0; fi\n"
                          "printf 'Not authenticated. Run auth login.' >&2\nexit 1\n", true));
        ProtonAuthController auth(binary);
        QSignalSpy messages(&auth, &ProtonAuthController::statusChanged);
        QTRY_VERIFY(auth.checking());
        QVERIFY(!auth.checked());
        QTRY_VERIFY(auth.checked() && !auth.checking());
        QVERIFY(!auth.authenticated());
        QVERIFY(auth.error().contains("Not authenticated"));
        QVERIFY(writeFile(home.filePath("authenticated"), "yes"));
        auth.refresh();
        QTRY_VERIFY(auth.authenticated());
        QVERIFY(auth.error().isEmpty());
        QCOMPARE(messages.count(), 1);
    }

    void missingCliReportsActionableError()
    {
        ProtonAuthController auth(QStringLiteral("/does/not/exist/proton-drive"));
        QTRY_VERIFY(auth.checked() && !auth.checking());
        QVERIFY(!auth.authenticated());
        QVERIFY(auth.error().contains("Install proton-drive"));
        QSignalSpy errors(&auth, &ProtonAuthController::failed);
        auth.signIn();
        QCOMPARE(errors.count(), 1);
    }

    void launchesTerminalWithSeparateLoginArguments()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString binary = home.filePath("fake proton-drive");
        QVERIFY(writeFile(binary, "#!/bin/sh\nexit 1\n", true));
        QVERIFY(writeFile(home.filePath("xdg-terminal-exec"),
                          "#!/bin/sh\nbase=${0%/*}\nprintf '%s\\n' \"$@\" > \"$base/arguments\"\n", true));
        const QByteArray originalPath = qgetenv("PATH");
        qputenv("PATH", home.path().toUtf8());
        ProtonAuthController auth(binary);
        QTRY_VERIFY(auth.checked() && !auth.checking());
        QSignalSpy errors(&auth, &ProtonAuthController::failed);
        QSignalSpy messages(&auth, &ProtonAuthController::statusChanged);
        auth.signIn();
        qputenv("PATH", originalPath);
        QCOMPARE(errors.count(), 0);
        QCOMPARE(messages.count(), 1);
        const QString argumentPath = home.filePath("arguments");
        QTRY_VERIFY(QFile::exists(argumentPath));
        QFile arguments(argumentPath);
        QVERIFY(arguments.open(QIODevice::ReadOnly));
        QCOMPARE(arguments.readAll(), binary.toUtf8() + "\nauth\nlogin\n");
    }
};

QTEST_GUILESS_MAIN(ProtonAuthControllerTest)
#include "protonauthcontroller_test.moc"
