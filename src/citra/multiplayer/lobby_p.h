// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <utility>
#include <QStandardItem>
#include <QStandardItemModel>
#include "common/common_types.h"

namespace Column {

enum {
    Expand,
    RoomName,
    Creator,
    Members,
    Total,
};

} // namespace Column

class LobbyItem : public QStandardItem {
public:
    LobbyItem() = default;

    explicit LobbyItem(const QString& string) : QStandardItem{string} {}
    virtual ~LobbyItem() override = default;
};

class LobbyItemName : public LobbyItem {
public:
    static constexpr int NameRole{Qt::UserRole + 1};
    static constexpr int PasswordRole{Qt::UserRole + 2};

    LobbyItemName() = default;

    explicit LobbyItemName(bool has_password, QString name) : LobbyItem{} {
        setData(name, NameRole);
        setData(has_password, PasswordRole);
    }

    QVariant data(int role) const override {
        if (role == Qt::DecorationRole) {
            bool has_password{data(PasswordRole).toBool()};
            return has_password ? QIcon::fromTheme("lock").pixmap(16) : QIcon();
        }
        if (role != Qt::DisplayRole)
            return LobbyItem::data(role);
        return data(NameRole).toString();
    }

    bool operator<(const QStandardItem& other) const override {
        return data(NameRole).toString().localeAwareCompare(other.data(NameRole).toString()) < 0;
    }
};

class LobbyItemDescription : public LobbyItem {
public:
    LobbyItemDescription() = default;

    explicit LobbyItemDescription(QString description) {
        setData(description, DescriptionRole);
    }

    QVariant data(int role) const override {
        if (role != Qt::DisplayRole)
            return LobbyItem::data(role);
        auto description{data(DescriptionRole).toString()};
        description.prepend("Description: ");
        return description;
    }

    bool operator<(const QStandardItem& other) const override {
        return data(DescriptionRole)
                   .toString()
                   .localeAwareCompare(other.data(DescriptionRole).toString()) < 0;
    }

private:
    static constexpr int DescriptionRole{Qt::UserRole + 1};
};

class LobbyItemCreator : public LobbyItem {
public:
    LobbyItemCreator() = default;

    explicit LobbyItemCreator(QString creator, QString ip, u16 port) {
        setData(creator, CreatorRole);
        setData(ip, IpRole);
        setData(port, PortRole);
    }

    QVariant data(int role) const override {
        if (role != Qt::DisplayRole)
            return LobbyItem::data(role);
        return data(CreatorRole).toString();
    }

    bool operator<(const QStandardItem& other) const override {
        return data(CreatorRole).toString().localeAwareCompare(other.data(CreatorRole).toString()) <
               0;
    }

    static constexpr int CreatorRole{Qt::UserRole + 1};
    static constexpr int IpRole{Qt::UserRole + 2};
    static constexpr int PortRole{Qt::UserRole + 3};
};

class LobbyMember {
public:
    LobbyMember() = default;
    LobbyMember(const LobbyMember& other) = default;

    explicit LobbyMember(QString nickname, QString program)
        : nickname{std::move(nickname)}, program{std::move(program)} {}

    ~LobbyMember() = default;

    QString GetNickname() const {
        return nickname;
    }

    QString GetProgram() const {
        return program;
    }

private:
    QString nickname;
    QString program;
};

Q_DECLARE_METATYPE(LobbyMember);

class LobbyItemMemberList : public LobbyItem {
public:
    LobbyItemMemberList() = default;

    explicit LobbyItemMemberList(QList<QVariant> members, u32 max_members) {
        setData(members, MemberListRole);
        setData(max_members, MaxMembersRole);
    }

    QVariant data(int role) const override {
        if (role != Qt::DisplayRole)
            return LobbyItem::data(role);
        auto members = data(MemberListRole).toList();
        return QString("%1 / %2").arg(QString::number(members.size()),
                                      data(MaxMembersRole).toString());
    }

    bool operator<(const QStandardItem& other) const override {
        // Sort by rooms that have the most members
        int left_members{data(MemberListRole).toList().size()};
        int right_members{other.data(MemberListRole).toList().size()};
        return left_members < right_members;
    }

    static constexpr int MemberListRole{Qt::UserRole + 1};
    static constexpr int MaxMembersRole{Qt::UserRole + 2};
};

/// Member information for when a lobby is expanded in the UI
class LobbyItemExpandedMemberList : public LobbyItem {
public:
    LobbyItemExpandedMemberList() = default;

    explicit LobbyItemExpandedMemberList(QList<QVariant> members) {
        setData(members, MemberListRole);
    }

    QVariant data(int role) const override {
        if (role != Qt::DisplayRole)
            return LobbyItem::data(role);
        auto members = data(MemberListRole).toList();
        QString out;
        bool first{true};
        for (const auto& member : members) {
            if (!first)
                out += '\n';
            const auto& m{member.value<LobbyMember>()};
            auto program{m.GetProgram()};
            if (program.isEmpty())
                out += QString("%1 isn't runnning a program").arg(m.GetNickname());
            else
                out += QString("%1 is running %2").arg(m.GetNickname(), program);
            first = false;
        }
        return out;
    }

    static constexpr int MemberListRole{Qt::UserRole + 1};
};
