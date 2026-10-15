#include <QtTest>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>

#include "backend.h"
#include "markdownhighlighter.h"
#include "polish.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QTextDocument>

class OmaFreewriteTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
    }

    void countsWords() {
        QCOMPARE(Backend::countWords(QStringLiteral("one two-three don't 42")), 4);
        QCOMPARE(Backend::countWords(QStringLiteral("你好 世界")), 2);
        QCOMPARE(Backend::countWords(QString()), 0);
    }

    void normalizesLinks() {
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("www.example.com/path")),
                 QStringLiteral("https://www.example.com/path"));
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("mailto:writer@example.com")),
                 QStringLiteral("mailto:writer@example.com"));
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("example.com")).isEmpty());
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("file:///tmp/private")).isEmpty());
    }

    void suggestsSafeNames() {
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("My first draft\nBody")),
                 QStringLiteral("My first draft.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("A/B")), QStringLiteral("A-B.md"));
        QCOMPARE(Backend::suggestedFileName(QString()), QStringLiteral("Untitled.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("Already.md")),
                 QStringLiteral("Already.md"));
    }

    void keepsNotoFromCrowdingTheFontList() {
        const QStringList fonts = Backend::selectableFontFamilies({
            QStringLiteral("Noto Sans Devanagari"), QStringLiteral("Liberation Serif"),
            QStringLiteral("Noto Serif"), QStringLiteral("Noto Sans Tamil UI"),
            QStringLiteral("adwaita Sans"), QStringLiteral("IBM Plex Mono"),
            QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans Mono"),
            QStringLiteral("Liberation Serif"), QStringLiteral("Monospace"),
            QStringLiteral("Standard Symbols PS"), QStringLiteral("D050000L"),
            QStringLiteral("Noto Color Emoji"), QStringLiteral("Nimbus Sans [UKWN]"),
            QStringLiteral("Nimbus Sans [URW ]")});
        QCOMPARE(fonts, QStringList({QStringLiteral("IBM Plex Mono"),
                                     QStringLiteral("adwaita Sans"),
                                     QStringLiteral("Liberation Serif"),
                                     QStringLiteral("Nimbus Sans"),
                                     QStringLiteral("Noto Sans"),
                                     QStringLiteral("Noto Sans Mono"),
                                     QStringLiteral("Noto Serif")}));
    }

    void remembersEditorFont() {
        const QString installed = Backend().availableFonts().value(1);
        QVERIFY(!installed.isEmpty());

        {
            Backend backend;
            QCOMPARE(backend.editorFont(), Backend::defaultEditorFont());
            QSignalSpy fontSpy(&backend, &Backend::editorFontChanged);
            backend.setEditorFont(installed);
            QCOMPARE(fontSpy.count(), 1);
        }

        QCOMPARE(Backend().editorFont(), installed);

        // A remembered font that has since been uninstalled falls back.
        QSettings().setValue(QStringLiteral("editor/font"), QStringLiteral("No Such Font"));
        QCOMPARE(Backend().editorFont(), Backend::defaultEditorFont());
        QSettings().remove(QStringLiteral("editor/font"));
    }

    void remembersFocusLineFontSize() {
        {
            Backend backend;
            QCOMPARE(backend.focusLineFontSize(), Backend::defaultFocusLineFontSize);
            QCOMPARE(backend.focusWordThreshold(), Backend::defaultFocusWordThreshold);
            QVERIFY(backend.focusAutoEnter());
            QSignalSpy sizeSpy(&backend, &Backend::focusLineFontSizeChanged);
            backend.setFocusLineFontSize(18);
            QCOMPARE(sizeSpy.count(), 1);
            // Out-of-range sizes clamp instead of vanishing or exploding.
            backend.setFocusLineFontSize(1000);
            QCOMPARE(backend.focusLineFontSize(), Backend::maximumFocusLineFontSize);
            backend.setFocusLineFontSize(1);
            QCOMPARE(backend.focusLineFontSize(), Backend::minimumFocusLineFontSize);
            backend.setFocusLineFontSize(18);
        }

        QCOMPARE(Backend().focusLineFontSize(), 18);

        QSettings settings;
        settings.setValue(QStringLiteral("focus/wordThreshold"), 5);
        settings.setValue(QStringLiteral("focus/autoEnter"), false);
        settings.sync();
        QCOMPARE(Backend().focusWordThreshold(), 5);
        QVERIFY(!Backend().focusAutoEnter());
        settings.remove(QStringLiteral("focus"));
        settings.sync();
    }

    void focusModeArmsOnTypedWordsOnly() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        QTemporaryDir documents;
        QVERIFY(documents.isValid());
        const QString longPath = documents.filePath(QStringLiteral("long.md"));
        {
            QFile file(longPath);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
            file.write(QStringLiteral("word ").repeated(400).toUtf8());
        }

        Backend backend;
        Polish polish;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        engine.rootContext()->setContextProperty(QStringLiteral("polish"), &polish);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QObject *line = window->findChild<QObject *>(QStringLiteral("focusLineText"));
        QVERIFY(line);
        QVERIFY(!window->property("focusMode").toBool());

        // Opening a long document is reading, not writing: no blackout.
        backend.open(QUrl::fromLocalFile(longPath));
        QCOMPARE(backend.wordCount(), 400);
        QTest::qWait(300);
        QVERIFY(!window->property("focusMode").toBool());

        // Writing fewer than the threshold's worth of words leaves it alone.
        const QString fewWords = QStringLiteral("alpha ").repeated(Backend::defaultFocusWordThreshold - 1);
        editor->setProperty("text", editor->property("text").toString() + fewWords);
        QTRY_COMPARE(backend.wordCount(), 400 + Backend::defaultFocusWordThreshold - 1);
        QTest::qWait(50);
        QVERIFY(!window->property("focusMode").toBool());

        // The threshold word tips it over, and the line shows the tail.
        editor->setProperty("text", editor->property("text").toString() + QStringLiteral("omega\nlast"));
        editor->setProperty("cursorPosition", editor->property("text").toString().length());
        QTRY_VERIFY(window->property("focusMode").toBool());
        const QString shown = line->property("text").toString();
        QVERIFY2(shown.endsWith(QStringLiteral("omega  \u00b7  last")), qPrintable(shown));
        QVERIFY(!shown.contains(QLatin1Char('\n')));

        // Releasing it re-arms the counter from the current word count.
        QVERIFY(QMetaObject::invokeMethod(window.data(), "exitFocus", Q_ARG(QVariant, true)));
        QVERIFY(!window->property("focusMode").toBool());
        editor->setProperty("text", editor->property("text").toString() + QStringLiteral(" more words here"));
        QTRY_COMPARE(backend.wordCount(), 400 + Backend::defaultFocusWordThreshold + 4);
        QTest::qWait(50);
        QVERIFY(!window->property("focusMode").toBool());

        // The manual toggle always works.
        QVERIFY(QMetaObject::invokeMethod(window.data(), "toggleFocus"));
        QVERIFY(window->property("focusMode").toBool());
        QVERIFY(QMetaObject::invokeMethod(window.data(), "leaveFocusNow"));
        QVERIFY(!window->property("focusMode").toBool());
    }

    void polishPromptForbidsWordChanges() {
        const QString prompt = Polish::systemPrompt();
        QVERIFY(prompt.contains(QStringLiteral("never add words")));
        QVERIFY(prompt.contains(QStringLiteral("never delete words")));
        QVERIFY(prompt.contains(QStringLiteral("never more than two sentences")));
        QVERIFY(!prompt.contains(QStringLiteral("voice"), Qt::CaseInsensitive));
    }

    void polishCleansModelReplies() {
        // Fences and curly quotes go; the whitespace framing the span stays.
        const QString original = QStringLiteral("  dont do it\n");
        QCOMPARE(Polish::cleanReply(QStringLiteral("```\nDon’t do it.\n```"), original),
                 QStringLiteral("  Don't do it.\n"));
        QCOMPARE(Polish::cleanReply(QStringLiteral("“Hi,” she said.  \n  \n\n\nBye."),
                                    QStringLiteral("hi she said bye")),
                 QStringLiteral("\"Hi,\" she said.\n\nBye."));
        // A writer who types curly quotes keeps them.
        QCOMPARE(Polish::cleanReply(QStringLiteral("It’s"), QStringLiteral("it’s")),
                 QStringLiteral("It’s"));
    }

    void polishCapsParagraphsAtTwoSentences() {
        const QString five = QStringLiteral(
            "One is here. Two is here! Three is here? Four e.g. stays. Five is here.");
        QCOMPARE(Polish::capParagraphs(five),
                 QStringLiteral("One is here. Two is here!\n\nThree is here? Four e.g. stays.\n\nFive is here."));
        // Existing short paragraphs and trailing newlines are untouched.
        const QString shaped = QStringLiteral("One. Two.\n\nThree.\n");
        QCOMPARE(Polish::capParagraphs(shaped), shaped);
        // Markdown blocks are never re-flowed.
        const QString list = QStringLiteral("# Title. Here.\n\n- one. two. three.\n- four.\n\n> quoted. text. here.");
        QCOMPARE(Polish::capParagraphs(list), list);
        // Quotes and brackets closing a sentence belong to it.
        QCOMPARE(Polish::capParagraphs(QStringLiteral("He said \"go.\" (Really.) Then left. Done.")),
                 QStringLiteral("He said \"go.\" (Really.)\n\nThen left. Done."));
    }

    void polishRejectsRewrites() {
        const QString original = QStringLiteral(
            "so i was thinkign about teh launch and we dont actully know who the first ten customers are. "
            "we have a list, sure, but a list isnt the same as ten people who will pay");
        const QString spelled = QStringLiteral(
            "So I was thinking about the launch and we don't actually know who the first ten customers are.\n\n"
            "We have a list, sure, but a list isn't the same as ten people who will pay.");
        QVERIFY(Polish::preservesWords(original, spelled));
        // "alot" style splits and joins are fine.
        QVERIFY(Polish::preservesWords(QStringLiteral("i like it alot and every thing is ok here today"),
                                       QStringLiteral("I like it a lot and everything is ok here today.")));
        // A rewrite, a summary, an answer and a truncation are not.
        QVERIFY(!Polish::preservesWords(original, QStringLiteral(
            "I have been reflecting on the launch, and we do not truly know our first ten customers. "
            "A list exists, but it is not the same as ten committed buyers.")));
        QVERIFY(!Polish::preservesWords(original, QStringLiteral("The writer worries about finding ten paying customers.")));
        QVERIFY(!Polish::preservesWords(original, QStringLiteral(
            "So I was thinking about the launch and we don't actually know who the first ten customers are.")));
        QVERIFY(!Polish::preservesWords(original, spelled + QStringLiteral(
            " Here is some advice: call them this week and ask each one directly whether they will pay.")));
        QVERIFY(!Polish::preservesWords(original, QString()));
    }

    void polishBuildsProviderRequests() {
        QCOMPARE(Polish::providerFor(QStringLiteral("http://127.0.0.1:11434")), QStringLiteral("openai"));
        QCOMPARE(Polish::providerFor(QStringLiteral("https://api.openai.com/v1")), QStringLiteral("openai"));
        QCOMPARE(Polish::providerFor(QStringLiteral("https://api.anthropic.com")), QStringLiteral("anthropic"));
        QCOMPARE(Polish::endpointUrl(QStringLiteral("http://127.0.0.1:11434/"), QStringLiteral("chat/completions")),
                 QStringLiteral("http://127.0.0.1:11434/v1/chat/completions"));
        QCOMPARE(Polish::endpointUrl(QStringLiteral("https://api.openai.com/v1"), QStringLiteral("models")),
                 QStringLiteral("https://api.openai.com/v1/models"));

        const QJsonObject openai = Polish::requestBody(QStringLiteral("openai"), QStringLiteral("gemma3:4b"),
                                                       QStringLiteral("teh text"));
        QCOMPARE(openai.value(QStringLiteral("model")).toString(), QStringLiteral("gemma3:4b"));
        QCOMPARE(openai.value(QStringLiteral("temperature")).toInt(), 0);
        QCOMPARE(openai.value(QStringLiteral("messages")).toArray().size(), 2);
        QCOMPARE(openai.value(QStringLiteral("messages")).toArray().last().toObject()
                     .value(QStringLiteral("content")).toString(), QStringLiteral("teh text"));

        const QJsonObject anthropic = Polish::requestBody(QStringLiteral("anthropic"),
                                                          QStringLiteral("claude-haiku-5-5"),
                                                          QStringLiteral("teh text"));
        QCOMPARE(anthropic.value(QStringLiteral("system")).toString(), Polish::systemPrompt());
        QVERIFY(anthropic.value(QStringLiteral("max_tokens")).toInt() >= 4096);
        QVERIFY(!anthropic.contains(QStringLiteral("temperature")));
        QCOMPARE(anthropic.value(QStringLiteral("messages")).toArray().size(), 1);

        QSettings().setValue(QStringLiteral("polish/baseUrl"), QStringLiteral("https://api.anthropic.com"));
        QSettings().setValue(QStringLiteral("polish/apiKey"), QStringLiteral("sk-test"));
        {
            Polish polish;
            const QNetworkRequest request = polish.networkRequest(QStringLiteral("messages"));
            QCOMPARE(request.url().toString(), QStringLiteral("https://api.anthropic.com/v1/messages"));
            QCOMPARE(request.rawHeader("x-api-key"), QByteArray("sk-test"));
            QCOMPARE(request.rawHeader("anthropic-version"), QByteArray("2023-06-01"));
            QVERIFY(!request.hasRawHeader("Authorization"));
        }
        QSettings().setValue(QStringLiteral("polish/baseUrl"), QStringLiteral("https://api.groq.com/openai/v1/"));
        {
            Polish polish;
            const QNetworkRequest request = polish.networkRequest(QStringLiteral("chat/completions"));
            QCOMPARE(request.url().toString(), QStringLiteral("https://api.groq.com/openai/v1/chat/completions"));
            QCOMPARE(request.rawHeader("Authorization"), QByteArray("Bearer sk-test"));
            QVERIFY(!request.hasRawHeader("x-api-key"));
        }
        QSettings().remove(QStringLiteral("polish"));

        QString error;
        QCOMPARE(Polish::replyText(QByteArray("{\"choices\":[{\"message\":{\"content\":\"Fixed.\"}}]}"),
                                   QStringLiteral("openai"), &error), QStringLiteral("Fixed."));
        QCOMPARE(Polish::replyText(QByteArray("{\"content\":[{\"type\":\"text\",\"text\":\"Fixed.\"}],\"stop_reason\":\"end_turn\"}"),
                                   QStringLiteral("anthropic"), &error), QStringLiteral("Fixed."));
        QVERIFY(Polish::replyText(QByteArray("{\"error\":{\"message\":\"model not found\"}}"),
                                  QStringLiteral("openai"), &error).isEmpty());
        QCOMPARE(error, QStringLiteral("model not found"));
        QCOMPARE(Polish::modelNames(QByteArray("{\"data\":[{\"id\":\"qwen3:4b\"},{\"id\":\"gemma3:4b\"}]}")),
                 QStringList({QStringLiteral("gemma3:4b"), QStringLiteral("qwen3:4b")}));
    }

    void polishRemembersSettings() {
        QSettings().remove(QStringLiteral("polish"));
        {
            Polish polish;
            QVERIFY(polish.enabled());
            QCOMPARE(polish.baseUrl(), Polish::defaultBaseUrl());
            QCOMPARE(polish.model(), Polish::defaultModel());
            QVERIFY(polish.apiKey().isEmpty());
            polish.setEnabled(false);
            polish.setBaseUrl(QStringLiteral("http://localhost:8080/// "));
            polish.setModel(QStringLiteral(" llama3.2:3b "));
            polish.setModel(QString());
            polish.setApiKey(QStringLiteral("key"));
        }
        Polish polish;
        QVERIFY(!polish.enabled());
        QCOMPARE(polish.baseUrl(), QStringLiteral("http://localhost:8080"));
        QCOMPARE(polish.model(), QStringLiteral("llama3.2:3b"));
        QCOMPARE(polish.apiKey(), QStringLiteral("key"));
        QSettings().remove(QStringLiteral("polish"));
    }

    // A one-connection stand-in for an OpenAI-style server.
    static QTcpServer *fakeChatServer(QObject *parent, const QByteArray &reply, QByteArray *requestOut) {
        auto *server = new QTcpServer(parent);
        QObject::connect(server, &QTcpServer::newConnection, server, [server, reply, requestOut]() {
            QTcpSocket *socket = server->nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, reply, requestOut]() {
                requestOut->append(socket->readAll());
                const int headerEnd = requestOut->indexOf("\r\n\r\n");
                if (headerEnd < 0)
                    return;
                const QByteArray header = requestOut->left(headerEnd).toLower();
                const int lengthAt = header.indexOf("content-length:");
                const int length = lengthAt < 0 ? 0
                    : header.mid(lengthAt + 15, header.indexOf("\r\n", lengthAt) - lengthAt - 15).trimmed().toInt();
                if (requestOut->size() < headerEnd + 4 + length)
                    return;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                              + QByteArray::number(reply.size()) + "\r\nConnection: close\r\n\r\n" + reply);
                socket->disconnectFromHost();
            });
        });
        return server;
    }

    void polishRoundTripsThroughAServer() {
        const QString original = QStringLiteral("so teh launch is tomorow and i am not ready. nobody is ever ready i guess");
        const QString fixed = QStringLiteral(
            "So the launch is tomorrow and I am not ready. Nobody is ever ready I guess.");
        const QByteArray reply = QJsonDocument(QJsonObject{
            {QStringLiteral("choices"), QJsonArray{QJsonObject{
                {QStringLiteral("message"), QJsonObject{{QStringLiteral("content"), fixed}}}}}}})
            .toJson(QJsonDocument::Compact);

        QByteArray request;
        QTcpServer *server = fakeChatServer(this, reply, &request);
        QVERIFY(server->listen(QHostAddress::LocalHost));
        QSettings().setValue(QStringLiteral("polish/baseUrl"),
                             QStringLiteral("http://127.0.0.1:%1").arg(server->serverPort()));

        Polish polish;
        QSignalSpy startedSpy(&polish, &Polish::started);
        QSignalSpy finishedSpy(&polish, &Polish::finished);
        QSignalSpy failedSpy(&polish, &Polish::failed);

        // Too short to bother the model.
        polish.request(QStringLiteral("hi"), 0, 2);
        QCOMPARE(startedSpy.count(), 0);

        polish.request(original, 10, 10 + original.size());
        QCOMPARE(startedSpy.count(), 1);
        QVERIFY(polish.busy());
        QTRY_COMPARE(finishedSpy.count(), 1);
        QVERIFY(!polish.busy());
        QCOMPARE(failedSpy.count(), 0);
        QVERIFY2(request.startsWith("POST /v1/chat/completions"), request.constData());
        QVERIFY(request.contains("gemma3:4b"));

        const QList<QVariant> result = finishedSpy.takeFirst();
        QCOMPARE(result.at(0).toInt(), 10);
        QCOMPARE(result.at(1).toInt(), 10 + original.size());
        QCOMPARE(result.at(2).toString(), original);
        QCOMPARE(result.at(3).toString(), QStringLiteral(
            "So the launch is tomorrow and I am not ready. Nobody is ever ready I guess."));

        // The same reply as a rewrite is refused.
        server->close();
        QTcpServer *rewriter = fakeChatServer(this, QJsonDocument(QJsonObject{
            {QStringLiteral("choices"), QJsonArray{QJsonObject{
                {QStringLiteral("message"), QJsonObject{{QStringLiteral("content"),
                    QStringLiteral("Tomorrow we launch, and I feel unprepared. Perhaps nobody ever feels prepared.")}}}}}}})
            .toJson(QJsonDocument::Compact), &request);
        QVERIFY(rewriter->listen(QHostAddress::LocalHost));
        polish.setBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(rewriter->serverPort()));
        polish.request(original, 0, original.size());
        QTRY_COMPARE(failedSpy.count(), 1);
        QVERIFY2(failedSpy.first().first().toString().contains(QStringLiteral("changed the words")),
                 qPrintable(failedSpy.first().first().toString()));
        QCOMPARE(finishedSpy.count(), 0);

        // No server at all fails fast with a readable reason.
        rewriter->close();
        polish.request(original, 0, original.size());
        QTRY_COMPARE(failedSpy.count(), 2);
        QVERIFY(failedSpy.last().first().toString().contains(QStringLiteral("no server")));
        QSettings().remove(QStringLiteral("polish"));
    }

    void polishReplacesTheSpanInOneUndoStep() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        Polish polish;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        engine.rootContext()->setContextProperty(QStringLiteral("polish"), &polish);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);

        const QString before = QStringLiteral("# Notes\n\nkept as is. ");
        const QString session = QStringLiteral("teh new words\nmore here");
        editor->setProperty("text", before + session);
        const int start = before.size();
        const int end = before.size() + session.size();

        // A span that moved on is left alone.
        QVERIFY(!backend.replaceTextIfUnchanged(start, end, QStringLiteral("something else"),
                                                QStringLiteral("x")));
        QCOMPARE(editor->property("text").toString(), before + session);

        QVERIFY(backend.replaceTextIfUnchanged(start, end, session,
                                               QStringLiteral("The new words.\n\nMore here.")));
        QCOMPARE(editor->property("text").toString(),
                 QStringLiteral("# Notes\n\nkept as is. The new words.\n\nMore here."));
        QVERIFY(backend.modified());

        QVERIFY(QMetaObject::invokeMethod(editor, "undo"));
        QCOMPARE(editor->property("text").toString(), before + session);
    }

    void escapeFromFocusModeSendsTheSessionToPolish() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        // Point Polish at a closed port so requests fail quickly and quietly.
        QTcpServer closed;
        QVERIFY(closed.listen(QHostAddress::LocalHost));
        const quint16 port = closed.serverPort();
        closed.close();
        QSettings().setValue(QStringLiteral("polish/baseUrl"), QStringLiteral("http://127.0.0.1:%1").arg(port));

        Backend backend;
        Polish polish;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        engine.rootContext()->setContextProperty(QStringLiteral("polish"), &polish);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QSignalSpy startedSpy(&polish, &Polish::started);
        QSignalSpy failedSpy(&polish, &Polish::failed);

        // Words typed after the counter armed are the session.
        const QString opening = QStringLiteral("# Heading\n\n");
        editor->setProperty("text", opening);
        editor->setProperty("cursorPosition", opening.size());
        QVERIFY(QMetaObject::invokeMethod(window.data(), "rearmFocus"));
        const QString typed = QStringLiteral("alpha ").repeated(Backend::defaultFocusWordThreshold) + QStringLiteral("omega");
        editor->setProperty("text", opening + typed);
        editor->setProperty("cursorPosition", (opening + typed).size());
        QTRY_VERIFY(window->property("focusMode").toBool());
        QCOMPARE(window->property("focusStartPosition").toInt(), opening.size());

        // Shift+Escape leaves without a pass.
        QVERIFY(QMetaObject::invokeMethod(window.data(), "exitFocus", Q_ARG(QVariant, true)));
        QVERIFY(!window->property("focusMode").toBool());
        QCOMPARE(startedSpy.count(), 0);

        // Escape sends exactly the session span.
        QVERIFY(QMetaObject::invokeMethod(window.data(), "toggleFocus"));
        QVERIFY(window->property("focusMode").toBool());
        const QString more = QStringLiteral(" and then a few more words");
        editor->setProperty("text", opening + typed + more);
        editor->setProperty("cursorPosition", (opening + typed + more).size());
        QVERIFY(QMetaObject::invokeMethod(window.data(), "exitFocus", Q_ARG(QVariant, false)));
        QCOMPARE(startedSpy.count(), 1);
        QCOMPARE(startedSpy.first().at(0).toInt(), (opening + typed).size());
        QCOMPARE(startedSpy.first().at(1).toInt(), (opening + typed + more).size());
        QTRY_COMPARE(failedSpy.count(), 1);
        QVERIFY(backend.status().startsWith(QStringLiteral("Polish failed")));

        // Switched off, Escape just leaves.
        polish.setEnabled(false);
        QVERIFY(QMetaObject::invokeMethod(window.data(), "toggleFocus"));
        QVERIFY(QMetaObject::invokeMethod(window.data(), "exitFocus", Q_ARG(QVariant, false)));
        QCOMPARE(startedSpy.count(), 1);
        QSettings().remove(QStringLiteral("polish"));
    }

    void fileDialogsLeaveFocusModeFirst() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        Polish polish;
        polish.setEnabled(false);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        engine.rootContext()->setContextProperty(QStringLiteral("polish"), &polish);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        // Focus mode goes fullscreen; the portal's save dialog would open
        // behind that. Asking to save must drop both first.
        QVERIFY(QMetaObject::invokeMethod(window.data(), "toggleFocus"));
        QVERIFY(window->property("focusMode").toBool());
        backend.save();
        QVERIFY(!window->property("focusMode").toBool());
        QVERIFY(!window->property("focusTransitioning").toBool());
        QVERIFY(window->property("visibility").toInt() != 5 /* Window.FullScreen */);

        // Plain F11 fullscreen is restored once the dialog is done.
        QVERIFY(QMetaObject::invokeMethod(window.data(), "toggleFullScreen"));
        QTRY_COMPARE(window->property("visibility").toInt(), 5);
        backend.openDialog();
        QCOMPARE(window->property("visibility").toInt(), 2 /* Window.Windowed */);
        QVERIFY(window->property("dialogRestoresFullScreen").toBool());
        QVERIFY(QMetaObject::invokeMethod(window.data(), "finishDialog"));
        QCOMPARE(window->property("visibility").toInt(), 5);
        QSettings().remove(QStringLiteral("polish"));
    }

    void findsInlineMarkdownRanges() {
        const auto markup = MarkdownHighlighter::inlineMarkup(
            QStringLiteral("**bold** and *italic* and [site](https://example.com)"));
        QCOMPARE(markup.size(), 3);
        QCOMPARE(markup.at(0).content.start, 2);
        QCOMPARE(markup.at(0).content.length, 4);
        QCOMPARE(markup.at(2).content.length, 4);
        QCOMPARE(markup.at(2).markers[0].length, 1);
    }

    void loadsCurrentOmarchyTheme() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString themeDirectory = homeDirectory.path()
            + QStringLiteral("/.local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(themeDirectory));

        QFile colorsFile(themeDirectory + QStringLiteral("/colors.toml"));
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray palette(
            "mode = \"light\"\n"
            "accent = \"#112233\"\n"
            "selection = \"#445566\"\n"
            "background = \"#fefefe\"\n"
            "foreground = \"#101010\"\n");
        QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        colorsFile.close();

        Backend backend;
        QCOMPARE(backend.themeBackground(), QStringLiteral("#fefefe"));
        QCOMPARE(backend.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(backend.themeAccent(), QStringLiteral("#112233"));
        QCOMPARE(backend.themeSelection(), QStringLiteral("#445566"));
        QVERIFY(!backend.darkMode());
    }

    void ignoresFileWatcherEventsForSavedContents() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("first-save.md"));
        Backend backend;
        QSignalSpy externalChangeSpy(&backend, &Backend::externalChangeDetected);

        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(QFileInfo::exists(path));

        QFile sameContents(path);
        QVERIFY(sameContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        sameContents.close();
        QTest::qWait(100);
        QCOMPARE(externalChangeSpy.count(), 0);

        QFile changedContents(path);
        QVERIFY(changedContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(changedContents.write("changed elsewhere"), qint64(17));
        changedContents.close();
        QTRY_COMPARE(externalChangeSpy.count(), 1);
    }

    void keepsCursorAndSelectionStableAcrossInsertions() {
        const QString mutationsPath = QFINDTESTDATA("../src/EditorMutations.js");
        QVERIFY(!mutationsPath.isEmpty());

        QQmlEngine engine;
        QQmlComponent component(&engine);
        const QByteArray harness = R"QML(
            import QtQuick
            import "EditorMutations.js" as EditorMutations

            TextEdit {
                property string insertionText
                property int insertionCursor
                property string wrappedText
                property int wrappedSelectionStart
                property int wrappedSelectionEnd

                Component.onCompleted: {
                    text = "alpha omega";
                    cursorPosition = 5;
                    EditorMutations.replaceRange(this, 5, 5, "one\r\ntwo");
                    insertionText = text;
                    insertionCursor = cursorPosition;

                    text = "alpha beta omega";
                    select(6, 10);
                    EditorMutations.replaceRange(this, selectionStart, selectionEnd,
                                                 "**beta**", 2, 6);
                    wrappedText = text;
                    wrappedSelectionStart = selectionStart;
                    wrappedSelectionEnd = selectionEnd;
                }
            }
        )QML";
        const QUrl harnessUrl = QUrl::fromLocalFile(
            QFileInfo(mutationsPath).absolutePath() + QStringLiteral("/MutationHarness.qml"));
        component.setData(harness, harnessUrl);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));

        QCOMPARE(editor->property("insertionText").toString(),
                 QStringLiteral("alphaone\ntwo omega"));
        QCOMPARE(editor->property("insertionCursor").toInt(), 12);
        QCOMPARE(editor->property("wrappedText").toString(),
                 QStringLiteral("alpha **beta** omega"));
        QCOMPARE(editor->property("wrappedSelectionStart").toInt(), 8);
        QCOMPARE(editor->property("wrappedSelectionEnd").toInt(), 12);
    }

    void savesAndOpensFromFooterButtons() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        Polish polish;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        engine.rootContext()->setContextProperty(QStringLiteral("polish"), &polish);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QVERIFY(window->findChild<QObject *>(QStringLiteral("sourceEditor")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("renderedPreview")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("modeToggle")));

        QObject *saveButton = window->findChild<QObject *>(QStringLiteral("saveButton"));
        QObject *openButton = window->findChild<QObject *>(QStringLiteral("openButton"));
        QVERIFY(saveButton);
        QVERIFY(openButton);

        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(saveButton, "clicked"));
        QCOMPARE(saveDialogSpy.count(), 1);

        QSignalSpy openDialogSpy(&backend, &Backend::openDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(openButton, "clicked"));
        QCOMPARE(openDialogSpy.count(), 1);
    }

    void scalesTextWithDesktopTextSize() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        Polish polish;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        engine.rootContext()->setContextProperty(QStringLiteral("polish"), &polish);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 20);

        // `omarchy display text size 16` sets the GNOME factor to 16/12.
        backend.setTextScale(16.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 27);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 27);

        backend.setTextScale(9.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 15);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 15);
    }

    void remembersLastSaveDirectory() {
        QTemporaryDir saveDirectory;
        QVERIFY(saveDirectory.isValid());

        const QString savedPath = saveDirectory.filePath(QStringLiteral("first.md"));
        Backend savedDocument;
        savedDocument.saveAs(QUrl::fromLocalFile(savedPath));

        Backend nextDocument;
        QSignalSpy saveDialogSpy(&nextDocument, &Backend::saveDialogRequested);
        nextDocument.saveAsDialog();
        QCOMPARE(saveDialogSpy.count(), 1);

        const QUrl suggestedUrl = saveDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).absolutePath(),
                 saveDirectory.path());
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).fileName(),
                 QStringLiteral("Untitled.md"));

        QSettings().setValue(QStringLiteral("file/lastSaveDirectory"),
                             saveDirectory.filePath(QStringLiteral("missing")));
        Backend fallbackDocument;
        QSignalSpy fallbackDialogSpy(&fallbackDocument, &Backend::saveDialogRequested);
        fallbackDocument.saveAsDialog();
        const QUrl fallbackUrl = fallbackDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(fallbackUrl.toLocalFile()).absolutePath(), QDir::homePath());
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(OmaFreewriteTest)
#include "tst_omafreewrite.moc"
