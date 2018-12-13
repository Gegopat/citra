// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <future>
#include <QColor>
#include <QImage>
#include <QLabel>
#include <QList>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QTime>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include "citra/multiplayer/chat_room.h"
#include "citra/multiplayer/client_room.h"
#include "citra/multiplayer/message.h"
#include "citra/multiplayer/moderation_dialog.h"
#include "citra/multiplayer/state.h"
#include "citra/program_list_p.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "ui_chat_room.h"

class ChatMessage {
public:
    explicit ChatMessage(const Network::ChatEntry& chat, const QString& nickname, QTime ts = {}) {
        /// Convert the time to their default locale defined format
        QLocale locale;
        timestamp = locale.toString(ts.isValid() ? ts : QTime::currentTime(), QLocale::ShortFormat);
        this->nickname = QString::fromStdString(chat.nickname);
        message = QString::fromStdString(chat.message);
        contains_ping = message.contains(QString('@').append(nickname));
    }

    bool ContainsPing() const {
        return contains_ping;
    }

    /// Format the message using the member's color
    QString GetMemberChatMessage(u16 member) const {
        auto color{member_color[member % 16]};
        return QString("[%1] <font color='%2'>&lt;%3&gt;</font> <font style='%4'>%5</font>")
            .arg(timestamp, color, nickname.toHtmlEscaped(),
                 contains_ping ? "background-color: #FFFF00" : "", message.toHtmlEscaped());
    }

private:
    static constexpr std::array<const char*, 16> member_color{
        {"#0000FF", "#FF0000", "#8A2BE2", "#FF69B4", "#1E90FF", "#008000", "#00FF7F", "#B22222",
         "#DAA520", "#FF4500", "#2E8B57", "#5F9EA0", "#D2691E", "#9ACD32", "#FF7F50", "FFFF00"}};

    QString timestamp;
    QString nickname;
    QString message;
    bool contains_ping;
};

class StatusMessage {
public:
    explicit StatusMessage(const QString& msg, QTime ts = {}) {
        /// Convert the time to their default locale defined format
        QLocale locale;
        timestamp = locale.toString(ts.isValid() ? ts : QTime::currentTime(), QLocale::ShortFormat);
        message = msg;
    }

    QString GetSystemChatMessage() const {
        return QString("[%1] <font color='#FF8C00'>* %2</font>").arg(timestamp, message);
    }

private:
    QString timestamp;
    QString message;
};

ChatRoom::ChatRoom(QWidget* parent)
    : QWidget{parent}, ui{std::make_unique<Ui::ChatRoom>()},
      system{static_cast<ClientRoomWindow*>(parentWidget())->system} {
    ui->setupUi(this);
    // Set the item_model for member_view
    enum {
        COLUMN_NAME,
        COLUMN_PROGRAM,
        COLUMN_COUNT, // Number of columns
    };
    member_list = new QStandardItemModel(ui->member_view);
    ui->member_view->setModel(member_list);
    ui->member_view->setContextMenuPolicy(Qt::CustomContextMenu);
    member_list->insertColumns(0, COLUMN_COUNT);
    member_list->setHeaderData(COLUMN_NAME, Qt::Horizontal, "Nickname");
    member_list->setHeaderData(COLUMN_PROGRAM, Qt::Horizontal, "Program");
    constexpr int MaxChatLines{1000};
    ui->chat_history->document()->setMaximumBlockCount(MaxChatLines);
    // Register the network structs to use in slots and signals
    qRegisterMetaType<Network::ChatEntry>();
    qRegisterMetaType<Network::RoomInformation>();
    qRegisterMetaType<Network::RoomMember::State>();
    qRegisterMetaType<Network::StatusMessageEntry>();
    // Setup the callbacks for network updates
    auto& member{system.RoomMember()};
    member.BindOnChatMessageReceived(
        [this](const Network::ChatEntry& chat) { emit ChatReceived(chat); });
    member.BindOnStatusMessageReceived([this](const Network::StatusMessageEntry& status_message) {
        emit StatusMessageReceived(status_message);
    });
    connect(this, &ChatRoom::ChatReceived, this, &ChatRoom::OnChatReceive);
    connect(this, &ChatRoom::StatusMessageReceived, this, &ChatRoom::OnStatusMessageReceive);
    // Connect all the widgets to the appropriate events
    connect(ui->member_view, &QTreeView::customContextMenuRequested, this,
            &ChatRoom::PopupContextMenu);
    connect(ui->chat_message, &QLineEdit::returnPressed, ui->send_message, &QPushButton::released);
    connect(ui->chat_message, &QLineEdit::textChanged, this, &ChatRoom::OnChatTextChanged);
    connect(ui->send_message, &QPushButton::released, this, &ChatRoom::OnSendChat);
}

ChatRoom::~ChatRoom() = default;

void ChatRoom::SetModPerms(bool is_mod) {
    has_mod_perms = is_mod;
}

void ChatRoom::Clear() {
    ui->chat_history->clear();
    block_list.clear();
}

void ChatRoom::AppendStatusMessage(const QString& msg) {
    ui->chat_history->append(StatusMessage(msg).GetSystemChatMessage());
}

bool ChatRoom::Send(QString msg) {
    // Check if we're in a room
    auto& member{system.RoomMember()};
    if (member.GetState() != Network::RoomMember::State::Joined)
        return false;
    // Validate and send message
    auto message{std::move(msg).toStdString()};
    if (!ValidateMessage(message))
        return false;
    auto nickname{member.GetNickname()};
    Network::ChatEntry chat{nickname, message};
    auto members{member.GetMemberInformation()};
    auto it{std::find_if(members.begin(), members.end(),
                         [&chat](const Network::RoomMember::MemberInformation& member) {
                             return member.nickname == chat.nickname;
                         })};
    if (it == members.end())
        LOG_INFO(Network, "Can't find self in the member list when sending a message.");
    auto member_id{std::distance(members.begin(), it)};
    ChatMessage m{chat, QString::fromStdString(nickname)};
    member.SendChatMessage(message);
    AppendChatMessage(m.GetMemberChatMessage(member_id));
    return true;
}

void ChatRoom::HandleNewMessage(const QString& msg) {
    const auto& replies{static_cast<MultiplayerState*>(parentWidget()->parent())->GetReplies()};
    auto itr{replies.find(msg.toStdString())};
    if (itr != replies.end())
        Send(QString::fromStdString(itr->second));
}

void ChatRoom::AppendChatMessage(const QString& msg) {
    ui->chat_history->append(msg);
}

void ChatRoom::SendModerationRequest(Network::RoomMessageTypes type, const std::string& nickname) {
    auto& member{system.RoomMember()};
    auto members{member.GetMemberInformation()};
    auto it{std::find_if(members.begin(), members.end(),
                         [&nickname](const Network::RoomMember::MemberInformation& member) {
                             return member.nickname == nickname;
                         })};
    if (it == members.end()) {
        NetworkMessage::ShowError(NetworkMessage::NO_SUCH_USER);
        return;
    }
    member.SendModerationRequest(type, nickname);
}

bool ChatRoom::ValidateMessage(const std::string& msg) {
    return !msg.empty();
}

void ChatRoom::Disable() {
    ui->send_message->setDisabled(true);
    ui->chat_message->setDisabled(true);
}

void ChatRoom::Enable() {
    ui->send_message->setEnabled(true);
    ui->chat_message->setEnabled(true);
}

void ChatRoom::OnChatReceive(const Network::ChatEntry& chat) {
    if (!ValidateMessage(chat.message))
        return;
    // Get the ID of the member
    auto members{system.RoomMember().GetMemberInformation()};
    auto it{std::find_if(members.begin(), members.end(),
                         [&chat](const Network::RoomMember::MemberInformation& member) {
                             return member.nickname == chat.nickname;
                         })};
    if (it == members.end()) {
        LOG_INFO(Network, "Chat message received from unknown member. Ignoring it.");
        return;
    }
    if (block_list.count(chat.nickname)) {
        LOG_INFO(Network, "Chat message received from blocked member {}. Ignoring it.",
                 chat.nickname);
        return;
    }
    auto member{std::distance(members.begin(), it)};
    ChatMessage m{chat, QString::fromStdString(system.RoomMember().GetNickname())};
    AppendChatMessage(m.GetMemberChatMessage(member));
    if (m.ContainsPing())
        emit Pinged();
    auto message{QString::fromStdString(chat.message)};
    HandleNewMessage(message.remove(QChar('\0')));
}

void ChatRoom::OnStatusMessageReceive(const Network::StatusMessageEntry& status_message) {
    switch (status_message.type) {
    case Network::IdMemberJoined:
        AppendStatusMessage(
            QString("%1 has joined").arg(QString::fromStdString(status_message.nickname)));
        break;
    case Network::IdMemberLeft:
        AppendStatusMessage(
            QString("%1 has left").arg(QString::fromStdString(status_message.nickname)));
        break;
    case Network::IdMemberKicked:
        AppendStatusMessage(
            QString("%1 has been kicked").arg(QString::fromStdString(status_message.nickname)));
        break;
    case Network::IdMemberBanned:
        AppendStatusMessage(
            QString("%1 has been banned").arg(QString::fromStdString(status_message.nickname)));
        break;
    case Network::IdAddressUnbanned:
        AppendStatusMessage(
            QString("%1 has been unbanned").arg(QString::fromStdString(status_message.nickname)));
        break;
    }
}

void ChatRoom::OnSendChat() {
    auto message{ui->chat_message->text()};
    if (!Send(message))
        return;
    ui->chat_message->clear();
    HandleNewMessage(message);
}

void ChatRoom::SetMemberList(const Network::RoomMember::MemberList& member_list) {
    // TODO: Remember which row is selected
    this->member_list->removeRows(0, this->member_list->rowCount());
    for (const auto& member : member_list) {
        if (member.nickname.empty())
            continue;
        QList<QStandardItem*> l;
        std::vector<std::string> elements{member.nickname, member.program};
        for (const auto& item : elements) {
            auto child{new QStandardItem(QString::fromStdString(item))};
            child->setEditable(false);
            l.append(child);
        }
        this->member_list->invisibleRootItem()->appendRow(l);
    }
    // TODO: Restore row selection
}

void ChatRoom::OnChatTextChanged() {
    if (ui->chat_message->text().length() > Network::MaxMessageSize)
        ui->chat_message->setText(ui->chat_message->text().left(Network::MaxMessageSize));
}

void ChatRoom::PopupContextMenu(const QPoint& menu_location) {
    auto moderation_menu{[&] {
        if (has_mod_perms) {
            QMenu context_menu;
            auto moderation_action{context_menu.addAction("Moderation...")};
            connect(moderation_action, &QAction::triggered, [this] {
                ModerationDialog dialog{system.RoomMember(), this};
                dialog.exec();
            });
            context_menu.exec(ui->member_view->viewport()->mapToGlobal(menu_location));
        }
    }};
    auto item{ui->member_view->indexAt(menu_location)};
    if (!item.isValid()) {
        moderation_menu();
        return;
    }
    auto nickname{member_list->item(item.row())->text().toStdString()};
    // You can't block, kick or ban yourself
    if (nickname == system.RoomMember().GetNickname()) {
        moderation_menu();
        return;
    }
    QMenu context_menu;
    auto block_action{context_menu.addAction("Block Member")};
    block_action->setCheckable(true);
    block_action->setChecked(block_list.count(nickname) > 0);
    connect(block_action, &QAction::triggered, [this, nickname] {
        if (block_list.count(nickname))
            block_list.erase(nickname);
        else {
            auto result{QMessageBox::question(
                this, "Block Member",
                QString("When you block a member, you will no longer receive chat messages from "
                        "them.<br><br>Are you sure you would like to block %1?")
                    .arg(QString::fromStdString(nickname)),
                QMessageBox::Yes | QMessageBox::No)};
            if (result == QMessageBox::Yes)
                block_list.emplace(nickname);
        }
    });
    if (has_mod_perms) {
        context_menu.addSeparator();
        auto kick_action{context_menu.addAction("Kick")};
        auto ban_action{context_menu.addAction("Ban")};
        context_menu.addSeparator();
        auto moderation_action{context_menu.addAction("Moderation...")};
        connect(kick_action, &QAction::triggered, [this, nickname] {
            auto result{
                QMessageBox::question(this, "Kick Member",
                                      QString("Are you sure you would like to <b>kick</b> %1?")
                                          .arg(QString::fromStdString(nickname)),
                                      QMessageBox::Yes | QMessageBox::No)};
            if (result == QMessageBox::Yes)
                SendModerationRequest(Network::IdModKick, nickname);
        });
        connect(ban_action, &QAction::triggered, [this, nickname] {
            auto result{QMessageBox::question(
                this, "Ban Member",
                QString("Are you sure you would like to <b>kick and ban</b> %1?\n\nThis would "
                        "ban their IP address.")
                    .arg(QString::fromStdString(nickname)),
                QMessageBox::Yes | QMessageBox::No)};
            if (result == QMessageBox::Yes)
                SendModerationRequest(Network::IdModBan, nickname);
        });
        connect(moderation_action, &QAction::triggered, [this] {
            ModerationDialog dialog{system.RoomMember(), this};
            dialog.exec();
        });
    }
    context_menu.exec(ui->member_view->viewport()->mapToGlobal(menu_location));
}
