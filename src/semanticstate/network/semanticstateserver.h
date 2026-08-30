// TangoQ, a Mixxx fork purpose-built for Argentine Tango DJs.
// Copyright (C) 2026 Seemanta Dutta (TangoQ).
// Licensed under the GNU General Public License, version 2 or later.

#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <memory>

class QTcpServer;
class QTcpSocket;

namespace mixxx::semanticstate {

class Store;

class Server final : public QObject {
  public:
    explicit Server(Store* pStore, QObject* pParent = nullptr);
    ~Server() override;

    bool start(quint16 port);
    void stop();
    bool isListening() const;
    quint16 port() const;
    QStringList urls() const;

  private:
    struct Client {
        QByteArray input;
        bool websocket{false};
    };

    void acceptConnections();
    void readClient(QTcpSocket* pSocket);
    void handleHttpRequest(QTcpSocket* pSocket, const QByteArray& request);
    void handleWebSocketInput(QTcpSocket* pSocket);
    void broadcastEvent(const QByteArray& eventJson);
    void sendSnapshot(QTcpSocket* pSocket);
    void sendHttpResponse(QTcpSocket* pSocket,
            int status,
            const QByteArray& reason,
            const QByteArray& contentType,
            const QByteArray& body,
            const QByteArray& extraHeaders = {});
    void sendWebSocketFrame(QTcpSocket* pSocket, quint8 opcode, const QByteArray& payload);
    void disconnectSlowClient(QTcpSocket* pSocket);

    Store* const m_pStore;
    std::unique_ptr<QTcpServer> m_pServer;
    QHash<QTcpSocket*, Client> m_clients;
};

} // namespace mixxx::semanticstate
