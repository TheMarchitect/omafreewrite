#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

class QNetworkReply;

// Polish: the spelling-and-paragraphs pass that runs on the text written in a
// focus session once Escape releases the mode. The model is only ever asked to
// fix spelling, capitalization, punctuation and paragraph breaks; a reply that
// changes the words themselves is thrown away.
//
// Any OpenAI-compatible chat endpoint works (Ollama locally by default), and
// the Anthropic Messages API is recognised by its host name.
class Polish : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString provider READ provider NOTIFY baseUrlChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY availableModelsChanged)
    Q_PROPERTY(QString modelsStatus READ modelsStatus NOTIFY modelsStatusChanged)

public:
    explicit Polish(QObject *parent = nullptr);

    static constexpr int minimumWords = 3;
    static constexpr int maximumSentencesPerParagraph = 2;
    static constexpr int requestTimeoutMs = 180000;
    static QString defaultBaseUrl() { return QStringLiteral("http://127.0.0.1:11434"); }
    static QString defaultModel() { return QStringLiteral("gemma3:4b"); }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString &url);
    QString model() const { return m_model; }
    void setModel(const QString &model);
    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString &key);
    QString provider() const { return providerFor(m_baseUrl); }
    bool busy() const { return m_reply != nullptr; }
    QStringList availableModels() const { return m_availableModels; }
    QString modelsStatus() const { return m_modelsStatus; }

    // The pure pieces, kept static so they can be tested without a server.
    static QString systemPrompt();
    static QString providerFor(const QString &baseUrl);
    static QString endpointUrl(const QString &baseUrl, const QString &path);
    static QJsonObject requestBody(const QString &provider, const QString &model,
                                   const QString &text);
    static QString replyText(const QByteArray &body, const QString &provider, QString *error);
    static QStringList modelNames(const QByteArray &body);
    static QString cleanReply(const QString &reply, const QString &original);
    static QString capParagraphs(const QString &text,
                                 int maxSentences = maximumSentencesPerParagraph);
    static QStringList normalizedWords(const QString &text);
    static bool preservesWords(const QString &original, const QString &polished);
    QNetworkRequest networkRequest(const QString &path) const;

    Q_INVOKABLE void request(const QString &text, int start, int end);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void refreshModels();

signals:
    void enabledChanged();
    void baseUrlChanged();
    void modelChanged();
    void apiKeyChanged();
    void busyChanged();
    void availableModelsChanged();
    void modelsStatusChanged();
    void started(int start, int end);
    void finished(int start, int end, const QString &original, const QString &polished);
    void failed(const QString &reason);

private:
    void handleReply();
    void setModelsStatus(const QString &status);

    bool m_enabled = true;
    QString m_baseUrl;
    QString m_model;
    QString m_apiKey;
    QStringList m_availableModels;
    QString m_modelsStatus;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QPointer<QNetworkReply> m_modelsReply;
    QString m_original;
    int m_start = 0;
    int m_end = 0;
};
