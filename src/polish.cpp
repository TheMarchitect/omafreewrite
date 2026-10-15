#include "polish.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <algorithm>
#include <vector>

namespace {

const QString enabledSetting = QStringLiteral("polish/enabled");
const QString baseUrlSetting = QStringLiteral("polish/baseUrl");
const QString modelSetting = QStringLiteral("polish/model");
const QString apiKeySetting = QStringLiteral("polish/apiKey");

const QChar leftSingleQuote(0x2018);
const QChar rightSingleQuote(0x2019);
const QChar leftDoubleQuote(0x201C);
const QChar rightDoubleQuote(0x201D);

bool hasCurlyQuotes(const QString &text) {
    return text.contains(leftSingleQuote) || text.contains(rightSingleQuote)
        || text.contains(leftDoubleQuote) || text.contains(rightDoubleQuote);
}

QString straightenQuotes(QString text) {
    text.replace(leftSingleQuote, QLatin1Char('\''));
    text.replace(rightSingleQuote, QLatin1Char('\''));
    text.replace(leftDoubleQuote, QLatin1Char('"'));
    text.replace(rightDoubleQuote, QLatin1Char('"'));
    return text;
}

QString leadingWhitespace(const QString &text) {
    int i = 0;
    while (i < text.size() && text.at(i).isSpace())
        ++i;
    return text.left(i);
}

QString trailingWhitespace(const QString &text) {
    int i = text.size();
    while (i > 0 && text.at(i - 1).isSpace())
        --i;
    return text.mid(i);
}

// Paragraphs the writer shaped on purpose (lists, headings, quotes, code,
// tables) are left exactly as the model returned them.
bool isMarkdownBlock(const QString &paragraph) {
    static const QRegularExpression blockStart(
        QStringLiteral("^\\s*(#{1,6}\\s|[-*+]\\s|\\d+[.)]\\s|>|```|~~~|\\||---|\\*\\*\\*)"));
    return paragraph.contains(QLatin1Char('\n')) || blockStart.match(paragraph).hasMatch();
}

bool isSentenceTerminator(QChar c) {
    return c == QLatin1Char('.') || c == QLatin1Char('!') || c == QLatin1Char('?');
}

bool isClosingMark(QChar c) {
    return c == QLatin1Char('"') || c == QLatin1Char('\'') || c == QLatin1Char(')')
        || c == QLatin1Char(']') || c == rightDoubleQuote || c == rightSingleQuote;
}

// Split one paragraph into sentences. A sentence ends at . ! or ? (plus any
// closing quote or bracket) followed by whitespace and then a capital letter,
// digit or opening quote, which keeps "e.g. this" and "i.e. that" together.
QStringList splitSentences(const QString &paragraph) {
    QStringList sentences;
    int sentenceStart = 0;
    const int size = paragraph.size();
    for (int i = 0; i < size; ++i) {
        if (!isSentenceTerminator(paragraph.at(i)))
            continue;
        int end = i + 1;
        while (end < size && (isSentenceTerminator(paragraph.at(end)) || isClosingMark(paragraph.at(end))))
            ++end;
        if (end >= size || !paragraph.at(end).isSpace())
            continue;
        int next = end;
        while (next < size && paragraph.at(next).isSpace())
            ++next;
        if (next >= size)
            break;
        const QChar first = paragraph.at(next);
        const bool startsSentence = first.isUpper() || first.isDigit()
            || first == QLatin1Char('"') || first == leftDoubleQuote
            || first == QLatin1Char('\'') || first == leftSingleQuote
            || first == QLatin1Char('(') || first == QLatin1Char('[');
        if (!startsSentence)
            continue;
        sentences.append(paragraph.mid(sentenceStart, end - sentenceStart).trimmed());
        sentenceStart = next;
        i = next - 1;
    }
    const QString tail = paragraph.mid(sentenceStart).trimmed();
    if (!tail.isEmpty())
        sentences.append(tail);
    return sentences;
}

} // namespace

Polish::Polish(QObject *parent) : QObject(parent) {
    QSettings settings;
    m_enabled = settings.value(enabledSetting, true).toBool();
    m_baseUrl = settings.value(baseUrlSetting, defaultBaseUrl()).toString().trimmed();
    if (m_baseUrl.isEmpty())
        m_baseUrl = defaultBaseUrl();
    m_model = settings.value(modelSetting, defaultModel()).toString().trimmed();
    if (m_model.isEmpty())
        m_model = defaultModel();
    m_apiKey = settings.value(apiKeySetting).toString();
    m_network.setTransferTimeout(requestTimeoutMs);
}

void Polish::setEnabled(bool enabled) {
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    QSettings().setValue(enabledSetting, enabled);
    emit enabledChanged();
}

void Polish::setBaseUrl(const QString &url) {
    QString trimmed = url.trimmed();
    while (trimmed.endsWith(QLatin1Char('/')))
        trimmed.chop(1);
    if (trimmed.isEmpty())
        trimmed = defaultBaseUrl();
    if (trimmed == m_baseUrl)
        return;
    m_baseUrl = trimmed;
    QSettings().setValue(baseUrlSetting, trimmed);
    m_availableModels.clear();
    emit availableModelsChanged();
    emit baseUrlChanged();
}

void Polish::setModel(const QString &model) {
    const QString trimmed = model.trimmed();
    if (trimmed.isEmpty() || trimmed == m_model)
        return;
    m_model = trimmed;
    QSettings().setValue(modelSetting, trimmed);
    emit modelChanged();
}

void Polish::setApiKey(const QString &key) {
    const QString trimmed = key.trimmed();
    if (trimmed == m_apiKey)
        return;
    m_apiKey = trimmed;
    QSettings().setValue(apiKeySetting, trimmed);
    emit apiKeyChanged();
}

QString Polish::systemPrompt() {
    return QStringLiteral(
        "You are a silent proofreader for a writer's freewriting. The writer types fast "
        "without looking at the screen, so the text is full of misspellings and typos. "
        "You clean it up without changing what was said.\n\n"
        "Do exactly these three things:\n"
        "1. Correct every misspelled or mistyped word to the word the writer meant. "
        "Examples: \"thinkign\" -> \"thinking\", \"teh\" -> \"the\", \"becuase\" -> "
        "\"because\", \"dont\" -> \"don't\", \"isnt\" -> \"isn't\", \"wil\" -> \"will\".\n"
        "2. Fix capitalization and punctuation.\n"
        "3. Split the text into short paragraphs separated by a blank line. A paragraph "
        "is one complete thought and never more than two sentences.\n\n"
        "Never do these:\n"
        "- Never replace a word with a different word, never rephrase, never add words, "
        "never delete words, never reorder words. Correcting spelling is not replacing a "
        "word; swapping in a synonym is.\n"
        "- Never change slang, fragments, repetition, or odd phrasing. They stay as written.\n"
        "- Never change Markdown.\n"
        "- Never add a title, comment, explanation, or summary.\n\n"
        "Example input:\n"
        "ok so the probelm with the onboarding is nobody reads the email. they sign up and "
        "then they just sit there. i think we shuold put the first step right on the screen "
        "they land on and skip the email alltogether. maybe keep the email as a backup\n\n"
        "Example output:\n"
        "Ok so the problem with the onboarding is nobody reads the email. They sign up and "
        "then they just sit there.\n\n"
        "I think we should put the first step right on the screen they land on and skip the "
        "email altogether.\n\n"
        "Maybe keep the email as a backup.\n\n"
        "Output only the corrected text.");
}

QString Polish::providerFor(const QString &baseUrl) {
    const QString host = QUrl(baseUrl).host().toLower();
    if (host == QStringLiteral("anthropic.com") || host.endsWith(QStringLiteral(".anthropic.com")))
        return QStringLiteral("anthropic");
    return QStringLiteral("openai");
}

QString Polish::endpointUrl(const QString &baseUrl, const QString &path) {
    QString base = baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    if (base.endsWith(QStringLiteral("/v1")))
        base.chop(3);
    return base + QStringLiteral("/v1/") + path;
}

QJsonObject Polish::requestBody(const QString &provider, const QString &model,
                                const QString &text) {
    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    if (provider == QStringLiteral("anthropic")) {
        // Room for the text to come back with paragraph breaks and corrections.
        body.insert(QStringLiteral("max_tokens"), 4096 + text.size());
        body.insert(QStringLiteral("system"), systemPrompt());
        body.insert(QStringLiteral("messages"), QJsonArray{
            QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                        {QStringLiteral("content"), text}}});
        return body;
    }
    body.insert(QStringLiteral("temperature"), 0);
    body.insert(QStringLiteral("messages"), QJsonArray{
        QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                    {QStringLiteral("content"), systemPrompt()}},
        QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                    {QStringLiteral("content"), text}}});
    return body;
}

QString Polish::replyText(const QByteArray &body, const QString &provider, QString *error) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (!document.isObject()) {
        if (error)
            *error = QStringLiteral("the server did not answer with JSON");
        return QString();
    }
    const QJsonObject root = document.object();
    if (root.contains(QStringLiteral("error"))) {
        const QJsonValue value = root.value(QStringLiteral("error"));
        const QString message = value.isObject()
            ? value.toObject().value(QStringLiteral("message")).toString()
            : value.toString();
        if (error)
            *error = message.isEmpty() ? QStringLiteral("the server reported an error") : message;
        return QString();
    }

    QString text;
    if (provider == QStringLiteral("anthropic")) {
        const QJsonArray content = root.value(QStringLiteral("content")).toArray();
        for (const QJsonValue &block : content) {
            const QJsonObject object = block.toObject();
            if (object.value(QStringLiteral("type")).toString() == QStringLiteral("text"))
                text += object.value(QStringLiteral("text")).toString();
        }
        if (root.value(QStringLiteral("stop_reason")).toString() == QStringLiteral("refusal")) {
            if (error)
                *error = QStringLiteral("the model declined");
            return QString();
        }
    } else {
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty()) {
            text = choices.first().toObject().value(QStringLiteral("message")).toObject()
                .value(QStringLiteral("content")).toString();
        }
    }
    if (text.trimmed().isEmpty() && error)
        *error = QStringLiteral("the model returned nothing");
    return text;
}

QStringList Polish::modelNames(const QByteArray &body) {
    QStringList names;
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonArray data = root.value(QStringLiteral("data")).toArray();
    for (const QJsonValue &entry : data) {
        const QString id = entry.toObject().value(QStringLiteral("id")).toString();
        if (!id.isEmpty())
            names.append(id);
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

QString Polish::cleanReply(const QString &reply, const QString &original) {
    QString text = reply;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text = text.trimmed();

    // Some models wrap the answer in a code fence despite being told not to.
    static const QRegularExpression fence(
        QStringLiteral("^```[a-zA-Z]*\\n([\\s\\S]*?)\\n?```$"));
    const QRegularExpressionMatch fenced = fence.match(text);
    if (fenced.hasMatch())
        text = fenced.captured(1).trimmed();

    // Chat models love typographic quotes; the writer did not type any.
    if (!hasCurlyQuotes(original))
        text = straightenQuotes(text);

    // Blank lines never carry trailing spaces, and never more than one.
    static const QRegularExpression blankRun(QStringLiteral("[ \\t]*\\n[ \\t]*\\n(?:[ \\t]*\\n)*"));
    text.replace(blankRun, QStringLiteral("\n\n"));

    // The span came out of the middle of a document: hand back whatever
    // whitespace framed it so the cursor and the neighbours stay put.
    return leadingWhitespace(original) + text + trailingWhitespace(original);
}

QString Polish::capParagraphs(const QString &text, int maxSentences) {
    if (maxSentences < 1)
        return text;
    const QString lead = leadingWhitespace(text);
    const QString trail = trailingWhitespace(text);
    const QString body = text.trimmed();
    if (body.isEmpty())
        return text;

    static const QRegularExpression paragraphBreak(QStringLiteral("\\n[ \\t]*\\n+"));
    const QStringList paragraphs = body.split(paragraphBreak);
    QStringList result;
    for (const QString &paragraph : paragraphs) {
        if (isMarkdownBlock(paragraph)) {
            result.append(paragraph);
            continue;
        }
        const QStringList sentences = splitSentences(paragraph.trimmed());
        for (int i = 0; i < sentences.size(); i += maxSentences)
            result.append(sentences.mid(i, maxSentences).join(QLatin1Char(' ')));
    }
    return lead + result.join(QStringLiteral("\n\n")) + trail;
}

QStringList Polish::normalizedWords(const QString &text) {
    static const QRegularExpression separators(QStringLiteral("[^\\p{L}\\p{N}']+"));
    QString normalized = straightenQuotes(text).toLower();
    QStringList words;
    for (QString word : normalized.split(separators, Qt::SkipEmptyParts)) {
        while (word.startsWith(QLatin1Char('\'')))
            word.remove(0, 1);
        while (word.endsWith(QLatin1Char('\'')))
            word.chop(1);
        if (!word.isEmpty())
            words.append(word);
    }
    return words;
}

// A spelling pass keeps nearly every word where it was. The count may drift a
// little ("alot" becomes "a lot") and misspelled words will not match, but a
// rewrite, a summary, an answer to the text or a truncation all fail here.
bool Polish::preservesWords(const QString &original, const QString &polished) {
    const QStringList before = normalizedWords(original);
    const QStringList after = normalizedWords(polished);
    if (before.isEmpty() || after.isEmpty())
        return false;

    const int tolerance = std::max(3, int((before.size() + 9) / 10));
    if (std::abs(before.size() - after.size()) > tolerance)
        return false;

    // Longest common subsequence of exact words, in order.
    std::vector<int> previous(after.size() + 1, 0);
    std::vector<int> current(after.size() + 1, 0);
    for (int i = 1; i <= before.size(); ++i) {
        for (int j = 1; j <= after.size(); ++j) {
            if (before.at(i - 1) == after.at(j - 1))
                current[j] = previous[j - 1] + 1;
            else
                current[j] = std::max(previous[j], current[j - 1]);
        }
        std::swap(previous, current);
    }
    const int common = previous[after.size()];
    return common * 10 >= before.size() * 6;
}

QNetworkRequest Polish::networkRequest(const QString &path) const {
    QNetworkRequest request(QUrl(endpointUrl(m_baseUrl, path)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(requestTimeoutMs);
    if (provider() == QStringLiteral("anthropic")) {
        request.setRawHeader("anthropic-version", "2023-06-01");
        if (!m_apiKey.isEmpty())
            request.setRawHeader("x-api-key", m_apiKey.toUtf8());
    } else if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
    }
    return request;
}

void Polish::request(const QString &text, int start, int end) {
    if (normalizedWords(text).size() < minimumWords)
        return;
    cancel();

    m_original = text;
    m_start = start;
    m_end = end;
    const QByteArray payload = QJsonDocument(requestBody(provider(), m_model, text)).toJson(
        QJsonDocument::Compact);
    const QString path = provider() == QStringLiteral("anthropic")
        ? QStringLiteral("messages")
        : QStringLiteral("chat/completions");
    m_reply = m_network.post(networkRequest(path), payload);
    connect(m_reply, &QNetworkReply::finished, this, &Polish::handleReply);
    emit busyChanged();
    emit started(start, end);
}

void Polish::cancel() {
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->abort();
    reply->deleteLater();
    emit busyChanged();
}

void Polish::handleReply() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    if (reply != m_reply)
        return;
    m_reply = nullptr;
    emit busyChanged();

    const QByteArray body = reply->readAll();
    QString error;
    QString text = replyText(body, provider(), &error);
    if (text.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError && error.isEmpty())
            error = reply->errorString();
        if (reply->error() == QNetworkReply::ConnectionRefusedError)
            error = QStringLiteral("no server at ") + m_baseUrl;
        emit failed(error.isEmpty() ? QStringLiteral("no reply") : error);
        return;
    }

    const QString polished = capParagraphs(cleanReply(text, m_original));
    if (!preservesWords(m_original, polished)) {
        emit failed(QStringLiteral("the model changed the words, so nothing was touched"));
        return;
    }
    emit finished(m_start, m_end, m_original, polished);
}

void Polish::setModelsStatus(const QString &status) {
    if (status == m_modelsStatus)
        return;
    m_modelsStatus = status;
    emit modelsStatusChanged();
}

void Polish::refreshModels() {
    if (m_modelsReply) {
        m_modelsReply->abort();
        m_modelsReply->deleteLater();
        m_modelsReply = nullptr;
    }
    setModelsStatus(QStringLiteral("Looking up models…"));
    QNetworkRequest request = networkRequest(QStringLiteral("models"));
    request.setTransferTimeout(15000);
    m_modelsReply = m_network.get(request);
    connect(m_modelsReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply)
            return;
        reply->deleteLater();
        if (reply != m_modelsReply)
            return;
        m_modelsReply = nullptr;
        if (reply->error() != QNetworkReply::NoError) {
            setModelsStatus(reply->error() == QNetworkReply::ConnectionRefusedError
                ? QStringLiteral("No server at ") + m_baseUrl
                : reply->errorString());
            return;
        }
        const QStringList names = modelNames(reply->readAll());
        if (names != m_availableModels) {
            m_availableModels = names;
            emit availableModelsChanged();
        }
        setModelsStatus(names.isEmpty()
            ? QStringLiteral("No models at ") + m_baseUrl
            : QStringLiteral("%1 model%2 available").arg(names.size()).arg(names.size() == 1 ? "" : "s"));
    });
}
