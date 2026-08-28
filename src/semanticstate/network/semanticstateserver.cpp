// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#include "semanticstate/network/semanticstateserver.h"

#include <QCryptographicHash>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>
#include <QtLogging>

#include "semanticstate/semanticstatestore.h"

namespace mixxx::semanticstate {

namespace {

constexpr qsizetype kMaximumHttpRequestBytes = 16 * 1024;
constexpr qsizetype kMaximumWebSocketInputBytes = 64 * 1024;
constexpr qint64 kMaximumQueuedOutputBytes = 1024 * 1024;
constexpr qsizetype kMaximumClients = 16;
constexpr int kHandshakeTimeoutMs = 10000;
constexpr auto kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

QHash<QByteArray, QByteArray> parseHeaders(const QList<QByteArray>& lines) {
    QHash<QByteArray, QByteArray> headers;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const qsizetype colon = lines.at(i).indexOf(':');
        if (colon > 0) {
            headers.insert(lines.at(i).left(colon).trimmed().toLower(),
                    lines.at(i).mid(colon + 1).trimmed());
        }
    }
    return headers;
}

bool headerContainsToken(const QByteArray& value, const QByteArray& token) {
    const QList<QByteArray> values = value.toLower().split(',');
    for (const QByteArray& candidate : values) {
        if (candidate.trimmed() == token) {
            return true;
        }
    }
    return false;
}

} // namespace

Server::Server(Store* pStore, QObject* pParent)
        : QObject(pParent),
          m_pStore(pStore),
          m_pServer(std::make_unique<QTcpServer>()) {
    connect(m_pServer.get(), &QTcpServer::newConnection, this, [this]() {
        acceptConnections();
    });
    connect(m_pStore, &Store::eventPublished, this, [this](const QByteArray& eventJson) {
        broadcastEvent(eventJson);
    });
}

Server::~Server() {
    stop();
    QObject::disconnect(m_pServer.get(), nullptr, this, nullptr);
    const auto sockets = m_clients.keys();
    for (QTcpSocket* pSocket : sockets) {
        QObject::disconnect(pSocket, nullptr, this, nullptr);
    }
    m_clients.clear();
    m_pServer.reset();
}

bool Server::start(quint16 requestedPort) {
    if (m_pServer->isListening()) {
        qWarning() << "TangoQ semantic monitor is already listening on port" << port();
        return false;
    }
    if (!m_pServer->listen(QHostAddress::AnyIPv4, requestedPort)) {
        qWarning() << "TangoQ semantic monitor could not listen on port"
                   << requestedPort << ':' << m_pServer->errorString();
        return false;
    }

    qWarning() << "Experimental TangoQ semantic monitor enabled on port" << port();
    bool loggedLanAddress = false;
    const auto addresses = QNetworkInterface::allAddresses();
    for (const auto& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol &&
                !address.isLoopback()) {
            qWarning().noquote()
                    << QStringLiteral("TangoQ semantic monitor: http://%1:%2/")
                               .arg(address.toString())
                               .arg(port());
            loggedLanAddress = true;
        }
    }
    if (!loggedLanAddress) {
        qWarning().noquote()
                << QStringLiteral("TangoQ semantic monitor: http://127.0.0.1:%1/")
                           .arg(port());
    }
    return true;
}

void Server::stop() {
    if (!m_pServer) {
        return;
    }
    m_pServer->close();
    const auto sockets = m_clients.keys();
    for (QTcpSocket* pSocket : sockets) {
        QObject::disconnect(pSocket, nullptr, this, nullptr);
        pSocket->abort();
    }
    m_clients.clear();
}

bool Server::isListening() const {
    return m_pServer->isListening();
}

quint16 Server::port() const {
    return m_pServer->serverPort();
}

void Server::acceptConnections() {
    while (m_pServer->hasPendingConnections()) {
        QTcpSocket* pSocket = m_pServer->nextPendingConnection();
        if (m_clients.size() >= kMaximumClients) {
            qWarning() << "Rejecting TangoQ semantic monitor client: connection limit reached";
            pSocket->abort();
            pSocket->deleteLater();
            continue;
        }
        m_clients.insert(pSocket, {});
        connect(pSocket, &QTcpSocket::readyRead, this, [this, pSocket]() {
            readClient(pSocket);
        });
        connect(pSocket, &QTcpSocket::disconnected, this, [this, pSocket]() {
            m_clients.remove(pSocket);
            pSocket->deleteLater();
        });
        // The monitor is auxiliary. A client that never completes an HTTP or
        // WebSocket handshake must not occupy one of its bounded client slots.
        QTimer::singleShot(kHandshakeTimeoutMs, pSocket, [this, pSocket]() {
            const auto client = m_clients.constFind(pSocket);
            if (client != m_clients.cend() && !client->websocket) {
                pSocket->disconnectFromHost();
            }
        });
    }
}

void Server::readClient(QTcpSocket* pSocket) {
    auto client = m_clients.find(pSocket);
    if (client == m_clients.end()) {
        return;
    }
    client->input.append(pSocket->readAll());
    if (client->websocket) {
        handleWebSocketInput(pSocket);
        return;
    }
    if (client->input.size() > kMaximumHttpRequestBytes) {
        sendHttpResponse(pSocket,
                431,
                QByteArrayLiteral("Request Header Fields Too Large"),
                QByteArrayLiteral("text/plain; charset=utf-8"),
                QByteArrayLiteral("Request too large\n"));
        return;
    }
    const qsizetype requestEnd = client->input.indexOf("\r\n\r\n");
    if (requestEnd >= 0) {
        const QByteArray request = client->input.left(requestEnd + 4);
        client->input.remove(0, requestEnd + 4);
        handleHttpRequest(pSocket, request);
    }
}

void Server::handleHttpRequest(QTcpSocket* pSocket, const QByteArray& request) {
    const QList<QByteArray> lines = request.split('\n');
    if (lines.isEmpty()) {
        pSocket->disconnectFromHost();
        return;
    }
    const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
    if (requestLine.size() != 3 || !requestLine.at(2).startsWith("HTTP/")) {
        sendHttpResponse(pSocket,
                400,
                QByteArrayLiteral("Bad Request"),
                QByteArrayLiteral("text/plain; charset=utf-8"),
                QByteArrayLiteral("Malformed HTTP request\n"));
        return;
    }
    if (requestLine.at(0) != QByteArrayLiteral("GET")) {
        sendHttpResponse(pSocket,
                405,
                QByteArrayLiteral("Method Not Allowed"),
                QByteArrayLiteral("text/plain; charset=utf-8"),
                QByteArrayLiteral("Read-only GET endpoints only\n"),
                QByteArrayLiteral("Allow: GET\r\n"));
        return;
    }

    const QByteArray path = requestLine.at(1).split('?').first();
    if (!path.startsWith('/')) {
        sendHttpResponse(pSocket,
                400,
                QByteArrayLiteral("Bad Request"),
                QByteArrayLiteral("text/plain; charset=utf-8"),
                QByteArrayLiteral("Invalid request target\n"));
        return;
    }
    const auto headers = parseHeaders(lines);
    if (path == QByteArrayLiteral("/api/events") &&
            headerContainsToken(headers.value(QByteArrayLiteral("upgrade")),
                    QByteArrayLiteral("websocket"))) {
        if (!headerContainsToken(headers.value(QByteArrayLiteral("connection")),
                    QByteArrayLiteral("upgrade")) ||
                headers.value(QByteArrayLiteral("sec-websocket-version")) !=
                        QByteArrayLiteral("13")) {
            sendHttpResponse(pSocket,
                    426,
                    QByteArrayLiteral("Upgrade Required"),
                    QByteArrayLiteral("text/plain; charset=utf-8"),
                    QByteArrayLiteral("WebSocket version 13 required\n"),
                    QByteArrayLiteral("Upgrade: websocket\r\n"));
            return;
        }
        const QByteArray key = headers.value(QByteArrayLiteral("sec-websocket-key"));
        if (QByteArray::fromBase64(key).size() != 16) {
            sendHttpResponse(pSocket,
                    400,
                    QByteArrayLiteral("Bad Request"),
                    QByteArrayLiteral("text/plain; charset=utf-8"),
                    QByteArrayLiteral("Invalid WebSocket key\n"));
            return;
        }
        QByteArray challenge = key;
        challenge.append(kWebSocketGuid);
        const QByteArray accept = QCryptographicHash::hash(
                challenge, QCryptographicHash::Sha1)
                                          .toBase64();
        QByteArray response = QByteArrayLiteral("HTTP/1.1 101 Switching Protocols\r\n") +
                QByteArrayLiteral("Upgrade: websocket\r\n") +
                QByteArrayLiteral("Connection: Upgrade\r\n") +
                QByteArrayLiteral("Sec-WebSocket-Accept: ") + accept +
                QByteArrayLiteral("\r\n\r\n");
        pSocket->write(response);
        m_clients[pSocket].websocket = true;
        sendSnapshot(pSocket);
        return;
    }

    if (path == QByteArrayLiteral("/api/events")) {
        sendHttpResponse(pSocket,
                426,
                QByteArrayLiteral("Upgrade Required"),
                QByteArrayLiteral("text/plain; charset=utf-8"),
                QByteArrayLiteral("WebSocket endpoint\n"),
                QByteArrayLiteral("Upgrade: websocket\r\n"));
        return;
    }

    if (path == QByteArrayLiteral("/api/state")) {
        sendHttpResponse(pSocket,
                200,
                QByteArrayLiteral("OK"),
                QByteArrayLiteral("application/json; charset=utf-8"),
                m_pStore->snapshotJson());
        return;
    }
    if (path == QByteArrayLiteral("/")) {
        QFile file(QStringLiteral(":/semanticmonitor/index.html"));
        if (file.open(QIODevice::ReadOnly)) {
            sendHttpResponse(pSocket,
                    200,
                    QByteArrayLiteral("OK"),
                    QByteArrayLiteral("text/html; charset=utf-8"),
                    file.readAll());
        } else {
            sendHttpResponse(pSocket,
                    500,
                    QByteArrayLiteral("Internal Server Error"),
                    QByteArrayLiteral("text/plain; charset=utf-8"),
                    QByteArrayLiteral("Monitor resource unavailable\n"));
        }
        return;
    }
    sendHttpResponse(pSocket,
            404,
            QByteArrayLiteral("Not Found"),
            QByteArrayLiteral("text/plain; charset=utf-8"),
            QByteArrayLiteral("Not found\n"));
}

void Server::handleWebSocketInput(QTcpSocket* pSocket) {
    auto client = m_clients.find(pSocket);
    if (client == m_clients.end()) {
        return;
    }
    if (client->input.size() > kMaximumWebSocketInputBytes) {
        pSocket->disconnectFromHost();
        return;
    }
    while (client->input.size() >= 2) {
        const auto first = static_cast<quint8>(client->input.at(0));
        const auto second = static_cast<quint8>(client->input.at(1));
        const quint8 opcode = first & 0x0f;
        const bool final = (first & 0x80) != 0;
        const bool hasReservedBits = (first & 0x70) != 0;
        const bool masked = (second & 0x80) != 0;
        quint64 payloadLength = second & 0x7f;
        qsizetype offset = 2;
        if (payloadLength == 126) {
            if (client->input.size() < 4) {
                return;
            }
            payloadLength = qFromBigEndian<quint16>(
                    reinterpret_cast<const uchar*>(client->input.constData() + 2));
            offset = 4;
        } else if (payloadLength == 127) {
            if (client->input.size() < 10) {
                return;
            }
            payloadLength = qFromBigEndian<quint64>(
                    reinterpret_cast<const uchar*>(client->input.constData() + 2));
            offset = 10;
        }
        if (!final || hasReservedBits || !masked ||
                payloadLength > static_cast<quint64>(kMaximumWebSocketInputBytes) ||
                ((opcode & 0x08) != 0 && payloadLength > 125)) {
            pSocket->disconnectFromHost();
            return;
        }
        if (client->input.size() < offset + 4 + static_cast<qsizetype>(payloadLength)) {
            return;
        }
        const QByteArray mask = client->input.mid(offset, 4);
        offset += 4;
        QByteArray payload = client->input.mid(offset, payloadLength);
        for (qsizetype i = 0; i < payload.size(); ++i) {
            payload[i] = payload.at(i) ^ mask.at(i % 4);
        }
        client->input.remove(0, offset + payload.size());

        if (opcode == 0x8) {
            sendWebSocketFrame(pSocket, 0x8, payload);
            pSocket->disconnectFromHost();
            return;
        }
        if (opcode == 0x9) {
            sendWebSocketFrame(pSocket, 0xA, payload);
            continue;
        }
        if (opcode == 0xA) {
            continue;
        }
        // The protocol is server-to-client only. Reject frames that could be
        // misread as future commands instead of accumulating an input surface.
        sendWebSocketFrame(pSocket, 0x8, QByteArray::fromHex("03f0"));
        pSocket->disconnectFromHost();
        return;
    }
}

void Server::broadcastEvent(const QByteArray& eventJson) {
    const auto sockets = m_clients.keys();
    for (QTcpSocket* pSocket : sockets) {
        if (m_clients.value(pSocket).websocket) {
            sendWebSocketFrame(pSocket, 0x1, eventJson);
        }
    }
}

void Server::sendSnapshot(QTcpSocket* pSocket) {
    const QJsonObject message{
            {QStringLiteral("schemaVersion"), kSchemaVersion},
            {QStringLiteral("type"), QStringLiteral("snapshot")},
            {QStringLiteral("revision"), static_cast<double>(m_pStore->revision())},
            {QStringLiteral("snapshot"), m_pStore->snapshot()},
    };
    sendWebSocketFrame(
            pSocket, 0x1, QJsonDocument(message).toJson(QJsonDocument::Compact));
}

void Server::sendHttpResponse(QTcpSocket* pSocket,
        int status,
        const QByteArray& reason,
        const QByteArray& contentType,
        const QByteArray& body,
        const QByteArray& extraHeaders) {
    QByteArray response = QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status) + ' ' +
            reason + QByteArrayLiteral("\r\nContent-Type: ") + contentType +
            QByteArrayLiteral("\r\nContent-Length: ") + QByteArray::number(body.size()) +
            QByteArrayLiteral("\r\nCache-Control: no-store\r\n") +
            QByteArrayLiteral("X-Content-Type-Options: nosniff\r\n") +
            extraHeaders +
            QByteArrayLiteral("Connection: close\r\n\r\n") + body;
    pSocket->write(response);
    pSocket->disconnectFromHost();
}

void Server::sendWebSocketFrame(
        QTcpSocket* pSocket, quint8 opcode, const QByteArray& payload) {
    QByteArray frame;
    frame.append(static_cast<char>(0x80 | (opcode & 0x0f)));
    if (payload.size() < 126) {
        frame.append(static_cast<char>(payload.size()));
    } else if (payload.size() <= 0xffff) {
        frame.append(static_cast<char>(126));
        const quint16 length = qToBigEndian(static_cast<quint16>(payload.size()));
        frame.append(reinterpret_cast<const char*>(&length), sizeof(length));
    } else {
        frame.append(static_cast<char>(127));
        const quint64 length = qToBigEndian(static_cast<quint64>(payload.size()));
        frame.append(reinterpret_cast<const char*>(&length), sizeof(length));
    }
    frame.append(payload);
    if (pSocket->bytesToWrite() + frame.size() > kMaximumQueuedOutputBytes) {
        disconnectSlowClient(pSocket);
        return;
    }
    pSocket->write(frame);
}

void Server::disconnectSlowClient(QTcpSocket* pSocket) {
    qWarning() << "Disconnecting slow TangoQ semantic monitor client"
               << pSocket->peerAddress().toString();
    pSocket->abort();
}

} // namespace mixxx::semanticstate
