pragma ComponentBehavior: Bound

import QtQuick

/// One message row of the chat view.
///
/// This component exists because `ChatsPage.qml` uses
/// `pragma ComponentBehavior: Bound`: a delegate in such a file cannot read
/// `model` implicitly, and `MessageBubble` already owns properties with the
/// model's role names, so those roles cannot be declared on `MessageBubble`
/// itself. The required properties below receive the model roles from the
/// ListView and are forwarded to a private `MessageBubble`.
Item {
    id: root

    required property var messageId
    required property string text
    required property string textHtml
    required property string replyPreview
    required property string reactionsJson
    required property string linkUrl
    required property string linkTitle
    required property string linkDescription
    required property string linkSite
    required property int linkFileId
    required property bool linkVideo
    required property string pollQuestion
    required property string pollOptionsJson
    required property int pollTotal
    required property bool pollClosed
    required property bool pollQuiz
    required property bool pollMultiple
    required property bool outgoing
    required property string senderName
    required property var date
    required property bool pending
    required property bool failed
    required property bool edited
    required property string contentType
    required property int mediaFileId
    required property int mediaFullFileId
    required property int mediaWidth
    required property int mediaHeight
    required property bool mediaReady
    required property bool mediaFailed
    required property int mediaRevision

    /// Selection mode is owned by the page, not by the message model.
    property bool selectable: false
    property bool selected: false

    signal mediaOpenRequested(int fileId)
    signal mediaPlayRequested(int fileId)
    signal editRequested(var messageId, string text)
    signal deleteRequested(var messageId)
    signal reactionRequested(string emoji)
    signal addReactionRequested(var messageId, Item anchorItem)
    signal forwardRequested(var messageId)
    signal pollOptionSelected(int index)
    signal replyRequested(var messageId, string preview)
    signal selectionToggled(var messageId, bool isSelected)

    width: ListView.view ? ListView.view.width : 0
    implicitHeight: bubble.implicitHeight
    height: bubble.implicitHeight

    MessageBubble {
        id: bubble

        width: root.width
        messageId: root.messageId
        text: root.text
        textHtml: root.textHtml
        replyPreview: root.replyPreview
        reactionsJson: root.reactionsJson
        linkUrl: root.linkUrl
        linkTitle: root.linkTitle
        linkDescription: root.linkDescription
        linkSite: root.linkSite
        linkFileId: root.linkFileId
        linkVideo: root.linkVideo
        pollQuestion: root.pollQuestion
        pollOptionsJson: root.pollOptionsJson
        pollTotal: root.pollTotal
        pollClosed: root.pollClosed
        pollQuiz: root.pollQuiz
        pollMultiple: root.pollMultiple
        outgoing: root.outgoing
        senderName: root.senderName
        date: root.date
        pending: root.pending
        failed: root.failed
        edited: root.edited
        contentType: root.contentType
        mediaFileId: root.mediaFileId
        mediaFullFileId: root.mediaFullFileId
        mediaWidth: root.mediaWidth
        mediaHeight: root.mediaHeight
        mediaReady: root.mediaReady
        mediaFailed: root.mediaFailed
        mediaRevision: root.mediaRevision

        selectable: root.selectable
        selected: root.selected

        onMediaOpenRequested: (fileId) => root.mediaOpenRequested(fileId)
        onMediaPlayRequested: (fileId) => root.mediaPlayRequested(fileId)
        onEditRequested: (id, text) => root.editRequested(id, text)
        onDeleteRequested: (id) => root.deleteRequested(id)
        onReactionRequested: (emoji) => root.reactionRequested(emoji)
        onAddReactionRequested: (id, anchorItem) => root.addReactionRequested(id, anchorItem)
        onForwardRequested: (id) => root.forwardRequested(id)
        onPollOptionSelected: (index) => root.pollOptionSelected(index)
        onReplyRequested: (id, preview) => root.replyRequested(id, preview)
        onSelectionToggled: (id, isSelected) => root.selectionToggled(id, isSelected)
    }
}
