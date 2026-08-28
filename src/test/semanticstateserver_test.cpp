// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#include "semanticstate/network/semanticstateserver.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QThread>
#include <QtEndian>
#include <functional>
#include <optional>

#include "semanticstate/semanticstatestore.h"

namespace mixxx::semanticstate {

namespace {

constexpr int kTimeoutMs = 3000;

bool waitUntil(const std::function<bool()>& predicate) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kTimeoutMs) {
        if (predicate()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return predicate();
}

State makeState(qint64 positionMs = 1000) {
    State state;
    state.sessionId = QStringLiteral("network-session");
    state.startedAt = QDateTime::fromString(
            QStringLiteral("2026-08-28T12:00:00.000Z"), Qt::ISODateWithMs);
    const Track track{
            QStringLiteral("tangoq:network"),
            QStringLiteral("Orquesta"),
            QStringLiteral("Network tango"),
            180000,
    };
    state.playback.state = QStringLiteral("playing");
    state.playback.track = track;
    state.playback.positionMs = positionMs;
    state.queue.append({1, track});
    return state;
}

QByteArray httpRequest(quint16 port, const QByteArray& request) {
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    if (!waitUntil([&socket]() {
            return socket.state() == QAbstractSocket::ConnectedState;
        })) {
        return {};
    }
    socket.write(request);
    socket.flush();
    QByteArray response;
    waitUntil([&socket, &response]() {
        response.append(socket.readAll());
        return socket.state() == QAbstractSocket::UnconnectedState;
    });
    response.append(socket.readAll());
    return response;
}

std::optional<QByteArray> takeServerFrame(QByteArray* pWire) {
    if (pWire->size() < 2) {
        return std::nullopt;
    }
    const auto second = static_cast<quint8>(pWire->at(1));
    quint64 payloadLength = second & 0x7f;
    qsizetype offset = 2;
    if (payloadLength == 126) {
        if (pWire->size() < 4) {
            return std::nullopt;
        }
        payloadLength = qFromBigEndian<quint16>(
                reinterpret_cast<const uchar*>(pWire->constData() + 2));
        offset = 4;
    } else if (payloadLength == 127) {
        if (pWire->size() < 10) {
            return std::nullopt;
        }
        payloadLength = qFromBigEndian<quint64>(
                reinterpret_cast<const uchar*>(pWire->constData() + 2));
        offset = 10;
    }
    if (payloadLength > static_cast<quint64>(pWire->size() - offset)) {
        return std::nullopt;
    }
    QByteArray payload = pWire->mid(offset, payloadLength);
    pWire->remove(0, offset + static_cast<qsizetype>(payloadLength));
    return payload;
}

bool openWebSocket(
        QTcpSocket* pSocket, quint16 port, const QByteArray& key, QByteArray* pWire) {
    pSocket->connectToHost(QHostAddress::LocalHost, port);
    if (!waitUntil([pSocket]() {
            return pSocket->state() == QAbstractSocket::ConnectedState;
        })) {
        return false;
    }
    pSocket->write(QByteArrayLiteral("GET /api/events HTTP/1.1\r\n") +
            QByteArrayLiteral("Host: 127.0.0.1\r\n") +
            QByteArrayLiteral("Upgrade: websocket\r\n") +
            QByteArrayLiteral("Connection: Upgrade\r\n") +
            QByteArrayLiteral("Sec-WebSocket-Version: 13\r\n") +
            QByteArrayLiteral("Sec-WebSocket-Key: ") + key +
            QByteArrayLiteral("\r\n\r\n"));
    pSocket->flush();
    if (!waitUntil([pSocket, pWire]() {
            pWire->append(pSocket->readAll());
            return pWire->contains("\r\n\r\n");
        })) {
        return false;
    }
    const qsizetype headerEnd = pWire->indexOf("\r\n\r\n") + 4;
    const QByteArray headers = pWire->left(headerEnd);
    pWire->remove(0, headerEnd);
    return headers.startsWith("HTTP/1.1 101 ");
}

std::optional<QJsonObject> waitForMessage(QTcpSocket* pSocket, QByteArray* pWire) {
    std::optional<QByteArray> payload;
    const bool received = waitUntil([pSocket, pWire, &payload]() {
        pWire->append(pSocket->readAll());
        payload = takeServerFrame(pWire);
        return payload.has_value();
    });
    if (!received) {
        return std::nullopt;
    }
    return QJsonDocument::fromJson(*payload).object();
}

} // namespace

TEST(SemanticStateServerTest, DisabledServerCreatesNoListener) {
    Store store;
    Server server(&store);
    EXPECT_FALSE(server.isListening());
    EXPECT_EQ(0, server.port());
}

TEST(SemanticStateServerTest, HttpSnapshotIsReadOnlyAndHasBoundedRoutes) {
    Store store;
    ASSERT_TRUE(store.publish(makeState()));
    Server server(&store);
    ASSERT_TRUE(server.start(0));
    ASSERT_TRUE(server.isListening());

    const QByteArray response = httpRequest(server.port(),
            QByteArrayLiteral("GET /api/state HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    EXPECT_TRUE(response.startsWith("HTTP/1.1 200 OK\r\n"));
    const qsizetype bodyOffset = response.indexOf("\r\n\r\n") + 4;
    const QJsonObject snapshot =
            QJsonDocument::fromJson(response.mid(bodyOffset)).object();
    EXPECT_EQ(kSchemaVersion, snapshot.value(QStringLiteral("schemaVersion")).toInt());
    EXPECT_EQ(1, snapshot.value(QStringLiteral("revision")).toInt());
    EXPECT_EQ(QStringLiteral("network-session"),
            snapshot.value(QStringLiteral("session"))
                    .toObject()
                    .value(QStringLiteral("id"))
                    .toString());

    const QByteArray rejected = httpRequest(server.port(),
            QByteArrayLiteral("POST /api/state HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    EXPECT_TRUE(rejected.startsWith("HTTP/1.1 405 Method Not Allowed\r\n"));
    EXPECT_TRUE(rejected.contains("Allow: GET\r\n"));

    const QByteArray unknown = httpRequest(server.port(),
            QByteArrayLiteral("GET /not-a-route HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    EXPECT_TRUE(unknown.startsWith("HTTP/1.1 404 Not Found\r\n"));

    const QByteArray malformed = httpRequest(server.port(),
            QByteArrayLiteral("GET /api/state\r\nHost: localhost\r\n\r\n"));
    EXPECT_TRUE(malformed.startsWith("HTTP/1.1 400 Bad Request\r\n"));

    const QByteArray noUpgrade = httpRequest(server.port(),
            QByteArrayLiteral("GET /api/events HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    EXPECT_TRUE(noUpgrade.startsWith("HTTP/1.1 426 Upgrade Required\r\n"));
}

TEST(SemanticStateServerTest, MultipleClientsReceiveEventsAndReconnectSnapshot) {
    Store store;
    ASSERT_TRUE(store.publish(makeState()));
    Server server(&store);
    ASSERT_TRUE(server.start(0));

    QTcpSocket first;
    QTcpSocket second;
    QByteArray firstWire;
    QByteArray secondWire;
    ASSERT_TRUE(openWebSocket(
            &first, server.port(), QByteArrayLiteral("MDEyMzQ1Njc4OWFiY2RlZg=="), &firstWire));
    ASSERT_TRUE(openWebSocket(
            &second, server.port(), QByteArrayLiteral("ZmVkY2JhOTg3NjU0MzIxMA=="), &secondWire));

    const auto firstSnapshot = waitForMessage(&first, &firstWire);
    const auto secondSnapshot = waitForMessage(&second, &secondWire);
    ASSERT_TRUE(firstSnapshot.has_value());
    ASSERT_TRUE(secondSnapshot.has_value());
    EXPECT_EQ(QStringLiteral("snapshot"),
            firstSnapshot->value(QStringLiteral("type")).toString());
    EXPECT_EQ(1, firstSnapshot->value(QStringLiteral("revision")).toInt());
    EXPECT_EQ(1, secondSnapshot->value(QStringLiteral("revision")).toInt());

    ASSERT_TRUE(store.publish(makeState(2000)));
    const auto firstEvent = waitForMessage(&first, &firstWire);
    const auto secondEvent = waitForMessage(&second, &secondWire);
    ASSERT_TRUE(firstEvent.has_value());
    ASSERT_TRUE(secondEvent.has_value());
    EXPECT_EQ(QStringLiteral("state.changed"),
            firstEvent->value(QStringLiteral("type")).toString());
    EXPECT_FALSE(firstEvent->contains(QStringLiteral("change")));
    EXPECT_EQ(2, firstEvent->value(QStringLiteral("revision")).toInt());
    EXPECT_EQ(2, secondEvent->value(QStringLiteral("revision")).toInt());

    first.abort();
    ASSERT_TRUE(waitUntil([&first]() {
        return first.state() == QAbstractSocket::UnconnectedState;
    }));
    ASSERT_TRUE(store.publish(makeState(3000)));

    QTcpSocket reconnected;
    QByteArray reconnectedWire;
    ASSERT_TRUE(openWebSocket(&reconnected,
            server.port(),
            QByteArrayLiteral("YWJjZGVmMDEyMzQ1Njc4OQ=="),
            &reconnectedWire));
    const auto reconnectSnapshot = waitForMessage(&reconnected, &reconnectedWire);
    ASSERT_TRUE(reconnectSnapshot.has_value());
    EXPECT_EQ(QStringLiteral("snapshot"),
            reconnectSnapshot->value(QStringLiteral("type")).toString());
    EXPECT_EQ(3, reconnectSnapshot->value(QStringLiteral("revision")).toInt());
    EXPECT_EQ(3000,
            reconnectSnapshot->value(QStringLiteral("snapshot"))
                    .toObject()
                    .value(QStringLiteral("playback"))
                    .toObject()
                    .value(QStringLiteral("positionMs"))
                    .toInt());
}

TEST(SemanticStateServerTest, StopClosesListenerAndConnectedClients) {
    Store store;
    ASSERT_TRUE(store.publish(makeState()));
    Server server(&store);
    ASSERT_TRUE(server.start(0));

    QTcpSocket client;
    QByteArray wire;
    ASSERT_TRUE(openWebSocket(
            &client, server.port(), QByteArrayLiteral("MDEyMzQ1Njc4OWFiY2RlZg=="), &wire));
    ASSERT_TRUE(waitForMessage(&client, &wire).has_value());

    server.stop();
    EXPECT_FALSE(server.isListening());
    EXPECT_EQ(0, server.port());
    EXPECT_TRUE(waitUntil([&client]() {
        return client.state() == QAbstractSocket::UnconnectedState;
    }));
}

} // namespace mixxx::semanticstate
