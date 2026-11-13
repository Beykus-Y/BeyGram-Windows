#pragma once

#include "AppearanceFacade.h"
#include "SwipeSettingsFacade.h"
#include "SharedMediaFacade.h"
#include "SavedMessagesFacade.h"
#include "QuickSwitcherFacade.h"
#include "NotificationScopesFacade.h"
#include "MuteScheduleFacade.h"
#include "MusicSearchFacade.h"
#include "MusicPlayerFacade.h"
#include "MusicLibraryFacade.h"
#include "MultiForwardFacade.h"
#include "MessageActionsFacade.h"
#include "LyricsFacade.h"
#include "GroupAdminFacade.h"
#include "ForumTopicsFacade.h"
#include "EqualizerFacade.h"
#include "CallHistoryFacade.h"
#include "BgMusicFacade.h"
#include "AttachmentFacade.h"
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

    [[nodiscard]] MusicLibraryFacade* musicLibrary() { return &m_musicLibrary; }
    [[nodiscard]] MusicPlayerFacade* musicPlayer() { return &m_musicPlayer; }
    [[nodiscard]] MusicSearchFacade* musicSearch() { return &m_musicSearch; }
    [[nodiscard]] EqualizerFacade* equalizer() { return &m_equalizer; }
    [[nodiscard]] BgMusicFacade* bgMusic() { return &m_bgMusic; }
    [[nodiscard]] LyricsFacade* lyrics() { return &m_lyrics; }
    [[nodiscard]] SavedMessagesFacade* savedMessages() { return &m_savedMessages; }
    [[nodiscard]] NotificationScopesFacade* notificationScopes() { return &m_notificationScopes; }
    [[nodiscard]] SwipeSettingsFacade* swipeSettings() { return &m_swipeSettings; }
    [[nodiscard]] MuteScheduleFacade* muteSchedule() { return &m_muteSchedule; }
    [[nodiscard]] AttachmentFacade* attachment() { return &m_attachment; }
    [[nodiscard]] MultiForwardFacade* multiForward() { return &m_multiForward; }
    [[nodiscard]] MessageActionsFacade* messageActions() { return &m_messageActions; }
    [[nodiscard]] QuickSwitcherFacade* quickSwitcher() { return &m_quickSwitcher; }
    [[nodiscard]] ForumTopicsFacade* forumTopics() { return &m_forumTopics; }
    [[nodiscard]] SharedMediaFacade* sharedMedia() { return &m_sharedMedia; }
    [[nodiscard]] GroupAdminFacade* groupAdmin() { return &m_groupAdmin; }
    [[nodiscard]] CallHistoryFacade* callHistory() { return &m_callHistory; }

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
    MusicLibraryFacade m_musicLibrary;
    MusicPlayerFacade m_musicPlayer;
    MusicSearchFacade m_musicSearch;
    EqualizerFacade m_equalizer;
    BgMusicFacade m_bgMusic;
    LyricsFacade m_lyrics;
    SavedMessagesFacade m_savedMessages;
    NotificationScopesFacade m_notificationScopes;
    SwipeSettingsFacade m_swipeSettings;
    MuteScheduleFacade m_muteSchedule;
    AttachmentFacade m_attachment;
    MultiForwardFacade m_multiForward;
    MessageActionsFacade m_messageActions;
    QuickSwitcherFacade m_quickSwitcher;
    ForumTopicsFacade m_forumTopics;
    SharedMediaFacade m_sharedMedia;
    GroupAdminFacade m_groupAdmin;
    CallHistoryFacade m_callHistory;
};

} // namespace beygram::ui
