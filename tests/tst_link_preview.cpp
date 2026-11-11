#include "telegram/LinkPreview.h"

#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

class LinkPreviewTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesFullPreview();
    void parsesTitleDescriptionAndSite();
    void marksVideoPreviews();
    void ignoresMessagesWithoutPreview();
    void keepsMissingFieldsEmpty();
    void readsFileIdsFromNumbersAndStrings();
    void prefersSmallestSuitableThumbnail();
    void resolvesUrlFromFallbacks();
};

void LinkPreviewTest::parsesFullPreview()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 10,
        "chat_id": 42,
        "content": {
            "@type": "messageText",
            "text": {
                "@type": "formattedText",
                "text": "https://example.com/story"
            },
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/story",
                "display_url": "example.com/story",
                "site_name": "Example",
                "title": "A story worth reading",
                "description": {
                    "@type": "formattedText",
                    "text": "Short summary of the story."
                },
                "type": {
                    "@type": "linkPreviewTypePhoto",
                    "photo": {
                        "sizes": [
                            { "width": 90, "height": 90, "photo": { "id": 3 } },
                            { "width": 320, "height": 240, "photo": { "id": 4 } },
                            { "width": 800, "height": 600, "photo": { "id": 5 } }
                        ]
                    }
                }
            },
            "link_preview_options": {
                "@type": "linkPreviewOptions",
                "is_disabled": false,
                "url": "https://example.com/story",
                "force_small_media": false,
                "force_large_media": false,
                "show_above_text": false
            }
        }
    })")
                                    .object();

    const LinkPreview preview = linkPreviewFromMessage(message);
    QVERIFY(preview.hasPreview);
    QCOMPARE(preview.url, QStringLiteral("https://example.com/story"));
    QCOMPARE(preview.title, QStringLiteral("A story worth reading"));
    QCOMPARE(preview.description, QStringLiteral("Short summary of the story."));
    QCOMPARE(preview.siteName, QStringLiteral("Example"));
    QCOMPARE(preview.thumbnailFileId, 4);
    QVERIFY(!preview.isVideo);
}

void LinkPreviewTest::parsesTitleDescriptionAndSite()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "text": { "@type": "formattedText", "text": "https://blog.example.org/post" },
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://blog.example.org/post",
                "display_url": "blog.example.org/post",
                "site_name": "Example Blog",
                "title": "Why previews matter",
                "description": {
                    "@type": "formattedText",
                    "text": "A post about link cards."
                },
                "type": {
                    "@type": "linkPreviewTypeArticle",
                    "photo": null
                }
            },
            "link_preview_options": null
        }
    })")
                                    .object();

    const LinkPreview preview = linkPreviewFromMessage(message);
    QVERIFY(preview.hasPreview);
    QCOMPARE(preview.url, QStringLiteral("https://blog.example.org/post"));
    QCOMPARE(preview.title, QStringLiteral("Why previews matter"));
    QCOMPARE(preview.description, QStringLiteral("A post about link cards."));
    QCOMPARE(preview.siteName, QStringLiteral("Example Blog"));
    QCOMPARE(preview.thumbnailFileId, 0);
    QVERIFY(!preview.isVideo);
}

void LinkPreviewTest::marksVideoPreviews()
{
    const QJsonObject videoMessage = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "text": { "@type": "formattedText", "text": "https://example.com/clip" },
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/clip",
                "display_url": "example.com/clip",
                "site_name": "Example",
                "title": "A clip",
                "description": { "@type": "formattedText", "text": "Watch this." },
                "type": {
                    "@type": "linkPreviewTypeVideo",
                    "video": {
                        "thumbnail": { "file": { "id": 12 } }
                    },
                    "cover": {
                        "sizes": [
                            { "width": 100, "height": 60, "photo": { "id": 11 } }
                        ]
                    },
                    "start_timestamp": 0
                }
            }
        }
    })")
                                        .object();

    const LinkPreview video = linkPreviewFromMessage(videoMessage);
    QVERIFY(video.hasPreview);
    QVERIFY(video.isVideo);
    QCOMPARE(video.title, QStringLiteral("A clip"));
    QCOMPARE(video.thumbnailFileId, 11);

    const QJsonObject noteMessage = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "text": { "@type": "formattedText", "text": "https://example.com/round" },
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/round",
                "title": "Round video",
                "type": {
                    "@type": "linkPreviewTypeVideoNote",
                    "video_note": {
                        "thumbnail": { "file": { "id": 21 } },
                        "video": { "id": 22 }
                    }
                }
            }
        }
    })")
                                       .object();

    const LinkPreview note = linkPreviewFromMessage(noteMessage);
    QVERIFY(note.hasPreview);
    QVERIFY(note.isVideo);
    QCOMPARE(note.thumbnailFileId, 21);

    // The video flag must not leak into ordinary photo previews.
    const QJsonObject photoMessage = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/photo",
                "type": { "@type": "linkPreviewTypePhoto", "photo": null }
            }
        }
    })")
                                         .object();
    QVERIFY(!linkPreviewFromMessage(photoMessage).isVideo);
}

void LinkPreviewTest::ignoresMessagesWithoutPreview()
{
    const QJsonObject noContent = QJsonDocument::fromJson(R"({
        "id": 1,
        "chat_id": 42
    })")
                                      .object();
    const LinkPreview empty = linkPreviewFromMessage(noContent);
    QVERIFY(!empty.hasPreview);
    QVERIFY(empty.url.isEmpty());
    QVERIFY(empty.title.isEmpty());
    QVERIFY(empty.description.isEmpty());
    QVERIFY(empty.siteName.isEmpty());
    QCOMPARE(empty.thumbnailFileId, 0);
    QVERIFY(!empty.isVideo);

    const QJsonObject nullPreview = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "text": { "@type": "formattedText", "text": "no link here" },
            "link_preview": null,
            "link_preview_options": null
        }
    })")
                                        .object();
    QVERIFY(!linkPreviewFromMessage(nullPreview).hasPreview);

    // A URL alone in link_preview_options is not a preview: there is no
    // server-supplied card to display.
    const QJsonObject optionsOnly = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "text": { "@type": "formattedText", "text": "https://example.com" },
            "link_preview": null,
            "link_preview_options": {
                "@type": "linkPreviewOptions",
                "url": "https://example.com"
            }
        }
    })")
                                        .object();
    const LinkPreview withoutCard = linkPreviewFromMessage(optionsOnly);
    QVERIFY(!withoutCard.hasPreview);
    QVERIFY(withoutCard.title.isEmpty());
    QVERIFY(withoutCard.description.isEmpty());
    QCOMPARE(withoutCard.thumbnailFileId, 0);

    // Non-text content never carries a preview.
    const QJsonObject photoContent = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messagePhoto",
            "photo": { "sizes": [] }
        }
    })")
                                         .object();
    QVERIFY(!linkPreviewFromMessage(photoContent).hasPreview);
}

void LinkPreviewTest::keepsMissingFieldsEmpty()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "text": { "@type": "formattedText", "text": "https://example.com" },
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com",
                "display_url": "example.com",
                "site_name": "",
                "title": "",
                "description": null,
                "type": { "@type": "linkPreviewTypeUnsupported" }
            }
        }
    })")
                                    .object();

    const LinkPreview preview = linkPreviewFromMessage(message);
    QVERIFY(preview.hasPreview);
    QCOMPARE(preview.url, QStringLiteral("https://example.com"));
    QVERIFY(preview.title.isEmpty());
    QVERIFY(preview.description.isEmpty());
    QVERIFY(preview.siteName.isEmpty());
    QCOMPARE(preview.thumbnailFileId, 0);
    QVERIFY(!preview.isVideo);

    // A preview with no fields other than the URL is still a preview, and the
    // parser must not invent a title from the URL.
    const QJsonObject bare = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/bare",
                "title": "",
                "description": { "@type": "formattedText", "text": "" }
            }
        }
    })")
                                 .object();
    const LinkPreview barePreview = linkPreviewFromMessage(bare);
    QVERIFY(barePreview.hasPreview);
    QVERIFY(barePreview.title.isEmpty());
    QVERIFY(barePreview.description.isEmpty());
}

void LinkPreviewTest::readsFileIdsFromNumbersAndStrings()
{
    const QJsonObject numeric = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/numeric",
                "type": {
                    "@type": "linkPreviewTypePhoto",
                    "photo": {
                        "sizes": [
                            { "width": 320, "height": 240, "photo": { "id": 7 } },
                            { "width": 640, "height": 480, "photo": { "id": 8 } }
                        ]
                    }
                }
            }
        }
    })")
                                     .object();

    const QJsonObject textual = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/textual",
                "type": {
                    "@type": "linkPreviewTypePhoto",
                    "photo": {
                        "sizes": [
                            { "width": 320, "height": 240, "photo": { "id": "7" } },
                            { "width": 640, "height": 480, "photo": { "id": "8" } }
                        ]
                    }
                }
            }
        }
    })")
                                     .object();

    const LinkPreview fromNumbers = linkPreviewFromMessage(numeric);
    const LinkPreview fromStrings = linkPreviewFromMessage(textual);
    QCOMPARE(fromNumbers.thumbnailFileId, 7);
    QCOMPARE(fromStrings.thumbnailFileId, fromNumbers.thumbnailFileId);

    const QJsonObject thumbnailStrings = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/video",
                "type": {
                    "@type": "linkPreviewTypeVideo",
                    "video": { "thumbnail": { "file": { "id": "31" } } },
                    "cover": null
                }
            }
        }
    })")
                                              .object();
    QCOMPARE(linkPreviewFromMessage(thumbnailStrings).thumbnailFileId, 31);

    // Zero and negative ids mean "no file" rather than a bogus id.
    const QJsonObject zeroIds = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/zero",
                "type": {
                    "@type": "linkPreviewTypeVideo",
                    "video": { "thumbnail": { "file": { "id": 0 } } },
                    "cover": {
                        "sizes": [
                            { "width": 320, "height": 240, "photo": { "id": "-4" } }
                        ]
                    }
                }
            }
        }
    })")
                                    .object();
    QCOMPARE(linkPreviewFromMessage(zeroIds).thumbnailFileId, 0);
}

void LinkPreviewTest::prefersSmallestSuitableThumbnail()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/sizes",
                "type": {
                    "@type": "linkPreviewTypePhoto",
                    "photo": {
                        "sizes": [
                            { "width": 1280, "height": 960, "photo": { "id": 9 } },
                            { "width": 640, "height": 480, "photo": { "id": 8 } },
                            { "width": 320, "height": 240, "photo": { "id": 7 } }
                        ]
                    }
                }
            }
        }
    })")
                                    .object();
    QCOMPARE(linkPreviewFromMessage(message).thumbnailFileId, 7);

    // When every size is below the target edge, the smallest one is used.
    const QJsonObject tiny = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/tiny",
                "type": {
                    "@type": "linkPreviewTypePhoto",
                    "photo": {
                        "sizes": [
                            { "width": 90, "height": 90, "photo": { "id": 1 } },
                            { "width": 60, "height": 60, "photo": { "id": 2 } }
                        ]
                    }
                }
            }
        }
    })")
                                 .object();
    QCOMPARE(linkPreviewFromMessage(tiny).thumbnailFileId, 2);
}

void LinkPreviewTest::resolvesUrlFromFallbacks()
{
    const QJsonObject displayOnly = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "",
                "display_url": "example.com/display",
                "title": "Display only",
                "type": { "@type": "linkPreviewTypeUnsupported" }
            }
        }
    })")
                                        .object();
    const LinkPreview display = linkPreviewFromMessage(displayOnly);
    QVERIFY(display.hasPreview);
    QCOMPARE(display.url, QStringLiteral("example.com/display"));
    QCOMPARE(display.title, QStringLiteral("Display only"));

    // An explicit link_preview_options URL beats the shortened display URL,
    // while the canonical linkPreview URL beats both.
    const QJsonObject withOptions = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "",
                "display_url": "example.com/display",
                "title": "Options URL",
                "type": { "@type": "linkPreviewTypeUnsupported" }
            },
            "link_preview_options": {
                "@type": "linkPreviewOptions",
                "url": "https://example.com/options"
            }
        }
    })")
                                         .object();
    QCOMPARE(linkPreviewFromMessage(withOptions).url,
             QStringLiteral("https://example.com/options"));

    const QJsonObject canonical = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageText",
            "link_preview": {
                "@type": "linkPreview",
                "url": "https://example.com/canonical",
                "display_url": "example.com/display",
                "type": { "@type": "linkPreviewTypeUnsupported" }
            },
            "link_preview_options": {
                "@type": "linkPreviewOptions",
                "url": "https://example.com/options"
            }
        }
    })")
                                       .object();
    QCOMPARE(linkPreviewFromMessage(canonical).url,
             QStringLiteral("https://example.com/canonical"));
}

QTEST_GUILESS_MAIN(LinkPreviewTest)
#include "tst_link_preview.moc"
