#pragma once

#include "AppearanceFacade.h"
#include "BotsFacade.h"
#include "CallsFacade.h"
#include "ChatListFacade.h"
#include "ContactsFacade.h"
#include "DeepLinksFacade.h"
#include "EmojiFacade.h"
#include "FoldersFacade.h"
#include "GalleryFacade.h"
#include "GiftsFacade.h"
#include "NetworkFacade.h"
#include "NotificationsFacade.h"
#include "PaymentsFacade.h"
#include "PlaybackFacade.h"
#include "PremiumFacade.h"
#include "PrivacyFacade.h"
#include "ProfileFacade.h"
#include "ReactionsFacade.h"
#include "SearchFacade.h"
#include "SecurityFacade.h"
#include "SelectionFacade.h"
#include "SessionsFacade.h"
#include "ShortcutsFacade.h"
#include "StarsFacade.h"
#include "StatisticsFacade.h"
#include "StickerPanelFacade.h"
#include "StorageFacade.h"
#include "StoriesFacade.h"
#include "TranslationFacade.h"
#include "VoiceComposerFacade.h"

#include <QObject>

namespace beygram::ui {

/// Owns every QML-facing facade so AppController can expose them with one
/// member and one construction point. Each facade is an independent QObject;
/// none of them talks to TDLib directly, they only hold UI state and emit
/// request signals that AppController forwards to the repository.
class UiFacades final : public QObject {
    Q_OBJECT

public:
    explicit UiFacades(QObject* parent = nullptr);

    [[nodiscard]] StickerPanelFacade* stickerPanel() { return &m_stickerPanel; }
    [[nodiscard]] SelectionFacade* selection() { return &m_selection; }
    [[nodiscard]] PrivacyFacade* privacy() { return &m_privacy; }
    [[nodiscard]] SessionsFacade* sessions() { return &m_sessions; }
    [[nodiscard]] CallsFacade* calls() { return &m_calls; }
    [[nodiscard]] StoriesFacade* stories() { return &m_stories; }
    [[nodiscard]] GiftsFacade* gifts() { return &m_gifts; }
    [[nodiscard]] StarsFacade* stars() { return &m_stars; }
    [[nodiscard]] PaymentsFacade* payments() { return &m_payments; }
    [[nodiscard]] PremiumFacade* premium() { return &m_premium; }
    [[nodiscard]] GalleryFacade* gallery() { return &m_gallery; }
    [[nodiscard]] PlaybackFacade* playback() { return &m_playback; }
    [[nodiscard]] VoiceComposerFacade* voiceComposer() { return &m_voiceComposer; }
    [[nodiscard]] SearchFacade* search() { return &m_search; }
    [[nodiscard]] ChatListFacade* chatListControls() { return &m_chatList; }
    [[nodiscard]] FoldersFacade* folders() { return &m_folders; }
    [[nodiscard]] TranslationFacade* translation() { return &m_translation; }
    [[nodiscard]] EmojiFacade* emoji() { return &m_emoji; }
    [[nodiscard]] ReactionsFacade* reactions() { return &m_reactions; }
    [[nodiscard]] BotsFacade* bots() { return &m_bots; }
    [[nodiscard]] ProfileFacade* profile() { return &m_profile; }
    [[nodiscard]] ContactsFacade* contacts() { return &m_contacts; }
    [[nodiscard]] NotificationsFacade* notifications() { return &m_notifications; }
    [[nodiscard]] StorageFacade* storage() { return &m_storage; }
    [[nodiscard]] NetworkFacade* network() { return &m_network; }
    [[nodiscard]] AppearanceFacade* appearance() { return &m_appearance; }
    [[nodiscard]] ShortcutsFacade* shortcuts() { return &m_shortcuts; }
    [[nodiscard]] StatisticsFacade* statistics() { return &m_statistics; }
    [[nodiscard]] SecurityFacade* security() { return &m_security; }
    [[nodiscard]] DeepLinksFacade* deepLinks() { return &m_deepLinks; }

private:
    StickerPanelFacade m_stickerPanel;
    SelectionFacade m_selection;
    PrivacyFacade m_privacy;
    SessionsFacade m_sessions;
    CallsFacade m_calls;
    StoriesFacade m_stories;
    GiftsFacade m_gifts;
    StarsFacade m_stars;
    PaymentsFacade m_payments;
    PremiumFacade m_premium;
    GalleryFacade m_gallery;
    PlaybackFacade m_playback;
    VoiceComposerFacade m_voiceComposer;
    SearchFacade m_search;
    ChatListFacade m_chatList;
    FoldersFacade m_folders;
    TranslationFacade m_translation;
    EmojiFacade m_emoji;
    ReactionsFacade m_reactions;
    BotsFacade m_bots;
    ProfileFacade m_profile;
    ContactsFacade m_contacts;
    NotificationsFacade m_notifications;
    StorageFacade m_storage;
    NetworkFacade m_network;
    AppearanceFacade m_appearance;
    ShortcutsFacade m_shortcuts;
    StatisticsFacade m_statistics;
    SecurityFacade m_security;
    DeepLinksFacade m_deepLinks;
};

} // namespace beygram::ui
