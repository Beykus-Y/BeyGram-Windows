# BeyGram — Android → Windows: аудит функционального соответствия

**Снимок состояния:** 12 ноября 2026, ~16:05 локального времени (финальная сверка).
Android: `F:\BeyGram` (рабочее дерево, 678 Kotlin-файлов продукта, 135 459 строк; 416 тестовых файлов, 33 119 строк).
Windows: `F:\BeyGram Windows`, HEAD `e1bdc260` («feat(parity): add final 14 controllers and fix folder symbol clash») **плюс 128 незакоммиченных файлов**, которые добавлялись другим процессом прямо во время аудита.

> Внимание: Windows-дерево активно изменялось параллельно с аудитом: за время работы HEAD сменился с `995c6e2e` на `e1bdc260`, число telegram-заголовков выросло со 146 до 175, тестовых файлов — со 170 до 214, QML-файлов — с 63 до 67. Ближе к концу появился слой `src/ui/*Facade` (30 фасадов), из-за чего число «заголовков без потребителя» уменьшилось со 101 до 50. Все количественные выводы ниже относятся к финальному снимку; там, где число менялось, это указано явно. Исходный код не изменялся; создан только этот файл.

---

## Как читать этот документ и методика

**Единица анализа — пользовательская функция**, а не файл и не класс. Функции сгруппированы в 19 категорий; внутри категории строка матрицы = одна возможность, которой на Android соответствует реальный пользовательский сценарий (экран, действие, запрос TDLib, состояние). Каждая категория открывается абзацем **«Android в двух словах»** — это описание поведения домена; колонка «Android» в строке уточняет файлы и детали конкретной функции. Родственные под-возможности (например, «venue / контакт / dice») объединены в одну строку, если у них общий пользовательский смысл и общий статус переноса. Итоговое число строк — **198 Android-функций**; ещё **12 строк** вынесены в отдельную таблицу Windows-only и в покрытие Android не входят.

Для каждой функции независимо оценены четыре направления:

- **Logic** — бизнес-логика: разбор TDLib-объектов, правила, состояния, валидация.
- **Integration** — подключение: реальный вызов из продакшн-кода, TDLib-запрос, участие в состоянии приложения.
- **UI** — доступность сценария через Windows-интерфейс (QML).
- **Tests** — наличие *зарегистрированных* в `tests/CMakeLists.txt` автотестов и их содержательность.

Статусы: `COMPLETE`, `PARTIAL`, `STUB`, `MISSING`, `DIFFERENT`, `UNKNOWN`.

Ключевые различения, которые применялись последовательно:

- **Код существует и компилируется ≠ функция работает.** Компонент, который не `#include`-ится и не инстанцируется ни одним продакшн-файлом, получает `Integration = MISSING` даже при наличии логики и зелёных тестов.
- **Тест ≠ UI.** Наличие `tst_*` не повышает `UI`.
- **STUB** — есть заготовка, пустой/локальный метод или имитация без реального эффекта (например, FSM звонка без TDLib, `setInstalled()` меняющий только флаг в памяти).
- **DIFFERENT** — сознательное платформенное отличие при сохранённом основном сценарии.

**Формула покрытия (единственная используемая):**

```
Coverage(dimension) = (COMPLETE + 0.5 × PARTIAL + 1.0 × DIFFERENT) / (N − UNKNOWN)
```

`STUB` и `MISSING` дают 0. Это **взвешенное** покрытие; рядом всегда приводится «полностью готово» = `COMPLETE / N`. Проценты считаются по 198 строкам матрицы, а не по числу файлов или строк кода.

**Ограничения достоверности (важнее любых процентов):**

1. Аудит **полностью статический**. Приложение не запускалось, вход в Telegram не выполнялся, ни один сетевой сценарий не проверен. Всё, что помечено `COMPLETE`, означает «код присутствует и подключён», а не «проверено в работе». Runtime-подтверждение требуется для каждой подключённой функции.
2. Автотесты **запущены**: 182 тестовых исполняемых файла, 3 217 тест-кейсов, все с кодом возврата 0. Но они покрывают чистые модели/билдеры и почти не покрывают QML, сетевой обмен и жизненный цикл TDLib. `tst_tdlib_integration` и `tst_telegram_session` в этот прогон не включены (исключены как требующие сети/креденшелов).
3. Часть строк (например, «Архив», «Папки», «Опросы») опирается на чтение `TelegramRepository.cpp`; поведение TDLib 1.8.69 в реальном ответе не проверялось.
4. Windows-дерево менялось во время аудита. Новые незакоммиченные фасады (`src/ui/*Facade.*`) на момент снимка **не подключены** к `AppController`; это зафиксировано отдельно в разделе Stubs and Dead Code.

---

## Executive Summary

Windows-клиент сегодня — это **работающее ядро одиночного аккаунта**: авторизация (телефон/код/пароль/QR, плюс регистрация и email, которых нет на Android), список чатов с архивом и серверными папками, история с revision-safe слиянием, отправка текста и одного файла, правка/удаление/ответ/пересылка, базовые реакции и опросы, глобальный и внутричатовый поиск, просмотр и редактирование своего профиля, черновики, уведомления через трей, просмотрщик фото и полноэкранный плеер, логирование с редакцией секретов, DPAPI-защита ключей и шифрование TDLib-БД.

Всё остальное — **не «недоделанные экраны», а отсоединённые компоненты**. Из 175 заголовков `src/telegram` **50 не включается ни одним другим файлом `src/`** (в начале аудита было 101; разницу дал появившийся слой фасадов): `StoryRequests`, `StickerSetModel`, `PaymentRequests`, `PremiumRequests`, `PrivacyRules`, `ChatBlocking`, `ContactRequests`, `PollAdmin`, `UnreadCounter` и т.п. Они аккуратно написаны, снабжены контрактными комментариями по схеме TDLib и покрыты тестами, но не достижимы ни из `TelegramRepository`, ни из QML. Отдельно: **36 QML-файлов не инстанцируются** (включая `StickerPicker.qml`, `WallpaperPicker.qml`, `SelectionToolbar.qml`, `ArchivePage.qml`, `ProfileEditDialog.qml`, `StoryViewer.qml`, `PremiumCard.qml`).

Новый слой фасадов (`UiFacades`, 30 штук) **выставлен в QML через `Q_PROPERTY`**, но остаётся инертным: он держит состояние и испускает сигналы вида `stickerActivated`, однако ни один сигнал не подключён к `AppController`/`TelegramRepository`, и ни один QML-файл его не использует. То есть это подготовка UI-контракта, а не интеграция.

Итог по 198 функциям: **34 COMPLETE**, **22 PARTIAL**, **2 STUB**, **139 MISSING**, **1 DIFFERENT** (по интеграции; по другим осям числа в дашборде). Взвешенное подтверждённое покрытие: **23.2%**; полностью готовых функций — **17.2%**. UI-покрытие ниже: **18.4%**.

Практический разрыв огромен в шести областях: **музыка и `.bgmusic`**, **истории**, **стикеры/GIF/эмодзи**, **Premium/Stars/подарки/платежи**, **секретные чаты/приватность/stealth**, **контакты и администрирование групп**. Там, где Android имеет полноценный продуктовый слой (10 000+ строк Compose только в музыке), Windows имеет либо ноль, либо модели-острова.

При этом Windows **сильнее Android** в безопасности хранения (DPAPI + 256-битный ключ БД против пустого ключа на Android), в авторизации (регистрация/email), в редакции логов и в наличии `tg://`-QR-подтверждения. Это не отменяет функционального отставания, но показывает, что это не механическая копия.

**Оценка зрелости:** Windows находится примерно на уровне «Phase 1.5»: базовый мессенджер работает, инфраструктура и дисциплина тестов хорошие, но продуктовое покрытие — примерно пятая часть Android. До функционального паритета (без VoIP, которого нет и на Android) — не месяцы полировки, а 10–14 инженеро-месяцев по объёму недостающих доменов.

---

## Coverage Dashboard

**Методика.** Строка матрицы = одна пользовательская функция (198 строк, 19 категорий). В таблице A счётчики взяты по оси **Integration** — она отвечает на вопрос «функция реально подключена к приложению». Таблица B агрегирует все 198 строк по каждому направлению. Проценты — по формуле из раздела «Как читать»: `COMPLETE`=1, `PARTIAL`=0.5, `DIFFERENT`=1, `STUB`/`MISSING`=0, `UNKNOWN` исключается из знаменателя (строк с UNKNOWN нет).

### A. Сводка по категориям (ось Integration)

| Категория | Функций | COMPLETE | PARTIAL | STUB | MISSING | DIFFERENT | UNKNOWN | Покрытие Integration |
|---|---|---|---|---|---|---|---|---|
| Авторизация и аккаунты | 9 | 5 | 0 | 1 | 3 | 0 | 0 | 55.6% |
| Чаты, папки, архив и сортировка | 12 | 5 | 2 | 0 | 5 | 0 | 0 | 50.0% |
| Сообщения и действия с ними | 14 | 4 | 5 | 0 | 5 | 0 | 0 | 46.4% |
| Медиа, загрузки и воспроизведение | 12 | 3 | 1 | 0 | 8 | 0 | 0 | 29.2% |
| Музыка и плейлисты | 12 | 0 | 1 | 1 | 10 | 0 | 0 | 4.2% |
| Истории | 9 | 0 | 0 | 0 | 9 | 0 | 0 | 0.0% |
| Стикеры, GIF и эмодзи | 11 | 1 | 0 | 0 | 10 | 0 | 0 | 9.1% |
| Реакции | 9 | 1 | 2 | 0 | 6 | 0 | 0 | 22.2% |
| Поиск | 10 | 3 | 0 | 0 | 7 | 0 | 0 | 30.0% |
| Контакты и профили | 12 | 0 | 2 | 0 | 10 | 0 | 0 | 8.3% |
| Звонки | 4 | 1 | 0 | 0 | 3 | 0 | 0 | 25.0% |
| Уведомления | 13 | 3 | 1 | 0 | 9 | 0 | 0 | 26.9% |
| Premium, Stars, подарки и платежи | 11 | 0 | 1 | 0 | 10 | 0 | 0 | 4.5% |
| Опросы и геопозиция | 10 | 2 | 3 | 0 | 5 | 0 | 0 | 35.0% |
| Боты, мини-приложения, deep links и share | 10 | 1 | 0 | 0 | 9 | 0 | 0 | 10.0% |
| Секретные чаты и безопасность | 7 | 0 | 1 | 0 | 6 | 0 | 0 | 7.1% |
| Настройки и персонализация | 12 | 1 | 3 | 0 | 7 | 1 | 0 | 29.2% |
| Сеть и прокси | 10 | 1 | 0 | 0 | 9 | 0 | 0 | 10.0% |
| Диагностика, аналитика, обновления, экспорт и прочее | 11 | 3 | 0 | 0 | 8 | 0 | 0 | 27.3% |
| **Итого** | **198** | **34** | **22** | **2** | **139** | **1** | **0** | **23.2%** |

### B. Четыре направления (все 198 функций)

| Направление | COMPLETE | PARTIAL | STUB | MISSING | DIFFERENT | UNKNOWN | Взвешенное покрытие | Полностью готово |
|---|---|---|---|---|---|---|---|---|
| Logic | 29 | 61 | 41 | 66 | 1 | 0 | 30.6% | 14.6% |
| Integration | 34 | 22 | 2 | 139 | 1 | 0 | 23.2% | 17.2% |
| UI | 18 | 37 | 1 | 142 | 0 | 0 | 18.4% | 9.1% |
| Tests | 101 | 19 | 0 | 78 | 0 | 0 | 55.8% | 51.0% |

### C. Покрытие по категориям и направлениям (%)

| Категория | Logic | Integration | UI | Tests | COMPLETE по Integration |
|---|---|---|---|---|---|
| Авторизация и аккаунты | 61.1 | 55.6 | 55.6 | 50.0 | 55.6 |
| Чаты, папки, архив и сортировка | 50.0 | 50.0 | 41.7 | 70.8 | 41.7 |
| Сообщения и действия с ними | 39.3 | 46.4 | 39.3 | 82.1 | 28.6 |
| Медиа, загрузки и воспроизведение | 25.0 | 29.2 | 25.0 | 70.8 | 25.0 |
| Музыка и плейлисты | 4.2 | 4.2 | 8.3 | 16.7 | 0.0 |
| Истории | 22.2 | 0.0 | 0.0 | 44.4 | 0.0 |
| Стикеры, GIF и эмодзи | 13.6 | 9.1 | 4.5 | 54.5 | 9.1 |
| Реакции | 50.0 | 22.2 | 16.7 | 44.4 | 11.1 |
| Поиск | 30.0 | 30.0 | 20.0 | 60.0 | 30.0 |
| Контакты и профили | 45.8 | 8.3 | 8.3 | 58.3 | 0.0 |
| Звонки | 12.5 | 25.0 | 12.5 | 50.0 | 25.0 |
| Уведомления | 50.0 | 26.9 | 19.2 | 65.4 | 23.1 |
| Premium, Stars, подарки и платежи | 27.3 | 4.5 | 0.0 | 72.7 | 0.0 |
| Опросы и геопозиция | 20.0 | 35.0 | 25.0 | 60.0 | 20.0 |
| Боты, мини-приложения, deep links и share | 45.0 | 10.0 | 5.0 | 70.0 | 10.0 |
| Секретные чаты и безопасность | 21.4 | 7.1 | 7.1 | 42.9 | 0.0 |
| Настройки и персонализация | 20.8 | 29.2 | 16.7 | 66.7 | 8.3 |
| Сеть и прокси | 10.0 | 10.0 | 10.0 | 30.0 | 10.0 |
| Диагностика, аналитика, обновления, экспорт и прочее | 18.2 | 27.3 | 22.7 | 27.3 | 27.3 |
| **Всего** | **30.6** | **23.2** | **18.4** | **55.8** | **17.2** |

### D. Как читать эти числа

- **Logic выше Integration почти везде** — это и есть главный вывод аудита: логика написана, проводки нет.
- **Tests высокий, UI низкий.** Зелёный тест-сьют (3 217 кейсов) относится к чистым моделям и билдерам; QML не покрыт, поэтому Tests нельзя читать как готовность продукта.
- **UI — самая честная ось** для ответа «что пользователь может сделать сейчас»: она учитывает только реально смонтированные экраны и элементы.
- **Полностью готово** — доля строк со статусом COMPLETE по Integration. Это самая консервативная оценка.

---

## Detailed Feature Matrix

Обозначения: **L** = Logic, **I** = Integration, **U** = UI, **T** = Tests. Файлы указаны относительно корня соответствующего репозитория. Полные пути Android: `app/src/main/java/com/beykus/beygram/…`; Windows: `src/…`, `qml/…`, `tests/…`.

### 1. Авторизация и аккаунты

**Android в двух словах:** Полный вход по телефону, коду и двухшаговому паролю, QR-вход с watchdog и отменой; реальный мультиаккаунт со вторым TDLib-клиентом на аккаунт; управление активными сессиями; безопасность аккаунта (облачный пароль, recovery email, смена номера, удаление); App Lock с PIN и биометрией. Регистрации нового аккаунта и email-входа в Android нет.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 1.1 | Вход по номеру и коду с повторной отправкой | Ввод телефона, код, resend; `ui/screens/auth/LoginPhoneScreen.kt`, `LoginCodeScreen.kt`, `TdLibTelegramRepository.kt:2054` | `qml/AuthPage.qml:303-368`, `telegram/AuthRequests.cpp:35`, `TelegramSession.cpp:253` | COMPLETE | COMPLETE | COMPLETE | COMPLETE | Windows нормализует номер и разбирает FLOOD_WAIT/PHONE_CODE_*; сценарий полный, runtime не проверялся | `AppController.cpp:1391`; `tst_auth_requests` |
| 1.2 | Двухшаговый пароль и подсказка | `LoginPasswordScreen.kt`, `TdLibTelegramRepository.kt:2081` | `AuthPage.qml:370-413`, `AuthRequests.cpp:51`, `TelegramSession.cpp:282` | COMPLETE | COMPLETE | COMPLETE | PARTIAL | Windows дополнительно предупреждает об отсутствии recovery email; тест только на билдер | `tst_auth_requests` |
| 1.3 | QR-вход (запрос, рендер, обновление) | `LoginPhoneScreen.kt:294-433`, `QrLogin.kt` (watchdog, отмена) | `AuthRequests.cpp:87`, `TelegramSession.cpp:339,435`, `QrCodeImageProvider.cpp`, `AuthPage.qml:553-608` | PARTIAL | COMPLETE | COMPLETE | PARTIAL | Реальный `requestQrCodeAuthentication` → `waitQrCode.link` → PNG через qrcodegen. Нет watchdog устаревшего токена, отмены и копирования ссылки | `AppController.cpp:1454`; `main.cpp:80` |
| 1.4 | Выход из аккаунта | `BeyGramApp.kt` (logout), `AccountSwitcherSheet.kt:485` | `TelegramSession.cpp:169`, `AppController.cpp:1486`, `OverviewPage.qml:144` | COMPLETE | COMPLETE | COMPLETE | MISSING | `logOut` + закрытие сессии с таймаутом 120 с; теста нет, цикл перезапуска не проверен | `OverviewPage.qml:185-221` |
| 1.5 | Активные сессии: список, завершение, завершить все | `ActiveSessionsScreen.kt`, `BeyGramApp.kt:2868`, `TdAccountSecurity.kt` | `SessionRequests.*`, `SessionListModel.*`, `SessionTerminationController.*`, `ui/SessionsFacade.*` | COMPLETE | MISSING | MISSING | COMPLETE | Фасад `sessions` выставлен в `AppController` и имеет Q_INVOKABLE `requestTerminate`, но сигнал `terminateRequested` не подключён к репозиторию, QML его не использует, экрана нет | `AppController.cpp:957`; `SessionsFacade.h:88` |
| 1.6 | Мультиаккаунт и переключение | `TdAccountRegistry.kt`, `TdSecondarySessionCoordinator.kt`, `AccountSwitcherSheet.kt` | `AccountRegistry.*` | STUB | STUB | MISSING | PARTIAL | Ровно одна `TelegramSession` (`AppController.cpp:236`); `AccountRegistry.h:17` прямо говорит «Phase 1 uses a single primary account»; `setActive()` не вызывается | `AccountRegistry.h:17-19` |
| 1.7 | Безопасность аккаунта: облачный пароль, recovery email, смена номера, удаление | `AccountSecurityScreen.kt`, `AccountRecoveryScreen.kt`, `PhoneNumberChangeDialog.kt`, `TdAccountSecurity.kt:20-99` | — | MISSING | MISSING | MISSING | MISSING | Ни `getPasswordState`, ни `setPassword`, ни `sendPhoneNumberCode`, ни `deleteAccount` в Windows нет | grep `src/` — пусто |
| 1.8 | App Lock и биометрия | `security/AppLock*.kt`, `BiometricAuthenticator.kt`, `AppLockSettingsScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | `SessionLock` — это мьютекс БД, а не блокировка приложения | `SessionLock.cpp:14` |
| 1.9 | Хранение секретов сессии и ключа БД | Android: пустой `databaseEncryptionKey` (`TdLibTelegramRepository.kt:3103`) | `CredentialStore.*`, `platform/windows/DpapiSecretStore.*`, `DatabaseKeyStore.*` | COMPLETE | COMPLETE | COMPLETE | COMPLETE | Windows сильнее: DPAPI + entropy, 256-битный ключ, повреждённый ключ не пересоздаётся | `DpapiSecretStore.cpp:46`; `tst_secret_store` |

### 2. Чаты, папки, архив и сортировка

**Android в двух словах:** Серверные папки с созданием, редактированием, удалением и порядком; отдельный список архива; ленивая подгрузка чатов и папок; pin и mute с учётом scope; mark read/unread со свайпом и подтверждением; sheet действий (pin, mute, unread, delete); создание ЛС, групп, каналов и секретных чатов; публичный поиск; инвайты, участники, права, блокировки; статистика и бусты.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 2.1 | Загрузка и пейджинг списка чатов | `TdLibTelegramRepository.kt:3122,3236` (LoadChats main+archive, рекурсия) | `TelegramRepository.cpp:143-216` (getChats null + loadChats null, limit 100) | PARTIAL | COMPLETE | COMPLETE | PARTIAL | Одна страница 100, «load more» нет, повторный `loadChats` не вызывается | `TelegramRepository.cpp:150,199` |
| 2.2 | Сортировка и закреплённые чаты | `ChatPreview.kt:56` (pinned→order→id), pinned-секция `ChatsScreen.kt:334` | `TelegramRepository.cpp:242` (order→date→title) | PARTIAL | PARTIAL | PARTIAL | PARTIAL | `pinned` парсится, но не участвует ни в сортировке, ни в отрисовке; отдельной секции и иконки нет | `TelegramTypes.cpp:425`; `ChatRow.qml:12` |
| 2.3 | Счётчики непрочитанного и фильтры All/People/Groups/Unread | `ChatPreview.unreadCount`, `ChatsScreen.kt:191-193,325` | badge в `ChatRow.qml:111-126`; `ChatFilterModel.cpp:22` | PARTIAL | PARTIAL | PARTIAL | PARTIAL | Нет суммарного счётчика и фильтров People/Groups/Unread; только локальный подстрочный поиск | `UnreadCounter` не инстанцируется |
| 2.4 | Mute чата с учётом scope | `TdLibTelegramRepository.kt:743`, `ChatMuteState.kt:36` | `TelegramRepository.cpp:375`, `ChatNotificationRequests.*` | PARTIAL | COMPLETE | COMPLETE | PARTIAL | Mute = `mute_for>0`, scope и `use_default_mute_for` игнорируются; иконки mute в строке нет | `ChatsPage.qml:1080` |
| 2.5 | Архив | `ArchiveScreen.kt`, ChatListArchive, `ArchiveSettingsStore.kt` | `ArchiveChats.cpp:12-58`, `TelegramRepository.cpp:267-373`, inline-список `ChatsPage.qml:175-213` | COMPLETE | COMPLETE | PARTIAL | COMPLETE | Реальный отдельный список `chatListArchive`; `ArchivePage.qml` не подключён, настроек архива нет | `ArchiveChats.cpp:45`; `tst_archive_chats` |
| 2.6 | Серверные папки: табы и содержимое | `TdLibTelegramRepository.kt:3171` (updateChatFolders), `:707` LoadChats(folder) | `ChatFolders.cpp:105`, `ChatFolderListModel.*`, `TelegramRepository.cpp:803` | PARTIAL | COMPLETE | COMPLETE | COMPLETE | Папки только из `updateChatFolders`; `getChatFolders` не отправляется, folder-позиции не парсятся → порядок и pin внутри папки неверны | `TelegramTypes.cpp:416-432` |
| 2.7 | Создание, переименование, удаление и порядок папок | `TdLibTelegramRepository.kt:602-652`, `FolderManagerScreen.kt` | `FolderEdit.cpp:112-130` (только билдеры) | COMPLETE | MISSING | MISSING | COMPLETE | Управления папками в UI нет вообще; билдеры не вызываются | grep `FolderEdit` — 0 |
| 2.8 | Действия списка: pin, отметить непрочитанным, удалить, покинуть, свайп | `ChatActionsSheet.kt:50-86`, `SwipeChatRow.kt:107` | `ChatListActions.cpp:38,47` (билдеры) | PARTIAL | MISSING | PARTIAL | PARTIAL | Из QML доступны только Mute и Archive; удаления/покидания чата нет даже в билдерах | `ChatsPage.qml:1075-1089` |
| 2.9 | Прочитано / отметить непрочитанным | `TdLibTelegramRepository.kt:1061`, `MarkReadConfirmation.kt` | авто `viewMessages` в `TelegramRepository.cpp:1105` | PARTIAL | COMPLETE | MISSING | PARTIAL | Только автоматическое чтение открытого чата; ручного действия и подтверждения нет | `TelegramRepository.cpp:612` |
| 2.10 | Создание ЛС, группы, канала, секретного чата; публичный поиск | `NewChatScreen.kt`, `CreateChannelScreen.kt`, `PublicChatDiscovery.kt`, `TdLibTelegramRepository.kt:827,909,976,1098` | только `DeepLinks.cpp:682-718` (не подключён) | MISSING | MISSING | MISSING | PARTIAL | Ни одной фабрики чата и ни одного экрана; `searchPublicChats` не отправляется | `tst_deep_links` |
| 2.11 | Инвайт-ссылки, участники, права, блокировки | `GroupMembersScreen.kt`, `GroupInvitesScreen.kt`, `TdGroupAdministration.kt:129-319` | `ChatInviteLinkModel.*`, `ChatMembers.*`, `ChatPermissions.*`, `ChatBlocking.*` | PARTIAL | MISSING | MISSING | COMPLETE | Только парсеры и value-модели; ни `setChatMemberStatus`, ни `createChatInviteLink`, ни `processChatJoinRequest` не строятся | grep `src/` — 0 |
| 2.12 | Статистика канала и бусты | `ChannelStatisticsScreen.kt:54`, `TdLibTelegramRepository.kt:1298,1471` | `ChatStatisticsModel.*`, `BoostModel.*` | STUB | MISSING | MISSING | COMPLETE | Только структуры строк; запросов `getChatStatistics`/`getBoostStatus` нет | `BoostModel.h:26` |

### 3. Сообщения и действия с ними

**Android в двух словах:** История с авто-подгрузкой и revision-safe слиянием; отправка текста с entities; медиа, стикеры, голосовые и альбомы; правка, удаление с выбором скоупа, ответ со свайпом, пересылка (в т.ч. копией); мультивыбор с bulk-баром; закреплённые, запланированные и облачные черновики; перевод, spoiler, эффекты, dice и сервисные сообщения; треды и топики; команды и кнопки ботов.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 3.1 | История, пагинация, авто-подгрузка | `MessageHistoryStore.kt`, `HistoryAutoLoad.kt` | `MessageHistoryStore.*`, `TelegramRepository.cpp:569-616` | COMPLETE | COMPLETE | PARTIAL | COMPLETE | Revision-safe merge с tombstone и job-id защитой; авто-подгрузки нет, только кнопка | `TelegramRepository.cpp:572`; `tst_message_history_store` |
| 3.2 | Отправка текста с entities и разметкой | `TdConversationActions.kt:323`, `FormattingTextField.kt` | `TelegramRequests.cpp:10-27`, `TextFormatting.cpp:319` | PARTIAL | PARTIAL | COMPLETE | COMPLETE | `entities` всегда пустой массив; `formatTdlibText` только рендерит входящие entities в HTML, markdown не разбирается | `TelegramRequests.cpp:17` |
| 3.3 | Вложения: медиа, голосовые, стикеры, альбомы | `TdConversationActions.kt:430-448`, `AudioNoteRecorder.kt`, `StickerPanel.kt` | `MediaSend.*` (Photo/Video/Document) | PARTIAL | PARTIAL | PARTIAL | PARTIAL | Один файл за раз; voice, стикеры и альбомы не отправляются | `MediaSend.h:10` |
| 3.4 | Правка, удаление, копирование | `MessageActions.kt`, `MessageContextDialog.kt` | `TelegramRepository.cpp:1063-1103`, `MessageBubble.qml:362-407` | COMPLETE | COMPLETE | COMPLETE | COMPLETE | Только текст; правки подписи медиа нет; `MessageEditController` не используется, состояние правки живёт в QML | `ChatsPage.qml:1031` |
| 3.5 | Ответ и цитата | `SwipeToReply.kt`, `ChatScreen.kt:2893` | `ReplyInfo.cpp`, `TelegramRepository.cpp`, `MessageBubble.qml:148` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | Нет swipe-to-reply; превью ответа достраивается только по загруженному окну; reply из черновика теряется | `tst_reply_info` |
| 3.6 | Пересылка (в т.ч. как копия, в топик) | `ForwardChatDialog.kt`, `ChatScreen.kt:2387` | `ForwardMessages.*`, `ForwardDialog.qml` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | Только одно сообщение, `send_copy=false`; «переслано от» (`ForwardInfo`) не показывается | `tst_forward_messages` |
| 3.7 | Мультивыбор и массовые действия | `MessageSelection.kt`, `SelectionActionBar.kt` | `MessageSelection{Model,Controller}.*` | STUB | MISSING | MISSING | COMPLETE | Входа в режим выбора нет; `SelectionToolbar.qml` — мёртвый файл | `qml/components/SelectionToolbar.qml` |
| 3.8 | Pin/unpin и переход к закреплённому | `PinnedMessages.kt` | `PinnedMessages.*`, `TelegramRepository.cpp:942-986` | PARTIAL | PARTIAL | PARTIAL | COMPLETE | Закрепить из UI нельзя (`pinChatMessage` не вызывается); список закреплённых отсутствует; клик по баннеру — `onActivated: {}` | `ChatsPage.qml:495` |
| 3.9 | Запланированные и отложенные сообщения | `ScheduledMessages.kt`, `data/delayed/*`, `DelayedSendStrip.kt` | `ScheduledMessages.*`, `ScheduleSendController.*`, `MessageSendOptions.*` | STUB | MISSING | MISSING | COMPLETE | `scheduleDate`/`withSendOptions` не вызываются ниоткуда; отложенной отправки нет | `MessageSendOptions.cpp` |
| 3.10 | Черновики | `TdChatDrafts.kt` (облачные, TDLib) | `DraftStore.*` (локальный JSON), `DraftSync.*` (мёртв) | PARTIAL | PARTIAL | PARTIAL | COMPLETE | Черновики не синхронизируются между устройствами; `draftReplyToMessageId` не читается QML | `AppController.cpp:1081` |
| 3.11 | Прочитано, typing, unread | `ChatUnread.kt`, `TdConversationEffects.kt` | `TypingStatus.*`, `TelegramRepository.cpp:988,1125` | PARTIAL | PARTIAL | PARTIAL | PARTIAL | Нет галочек прочтения (`updateChatReadOutbox` не обрабатывается); typing без TTL; `TypingRoster`/`ReadMarkerTracker`/`UnreadCounter` не инстанцируются | `TelegramUpdateScheduler.cpp:84` |
| 3.12 | Группировка сообщений, альбомы, разделители дат | `MessageAlbum.kt`, `FloatingMessageDate.kt` | `MessageGroupingModel.*`, `AlbumModel.*`, `AlbumGrouper.*` | STUB | MISSING | MISSING | COMPLETE | Плоский список; `MessageData` не содержит `grouped_id`; `AlbumGrid.qml` не используется | `TelegramTypes.h:31-75` |
| 3.13 | Спец-сообщения: dice, spoiler, перевод, эффекты, сервисные | `DiceMessageView.kt`, `MessageTranslation.kt`, `SafeSpoilerOverlay.kt`, `TdMessageEffects.kt` | `TranslationController.*`, `SpoilerRevealModel.*` (оба мёртвы) | MISSING | MISSING | MISSING | PARTIAL | Сервисные рендерятся как `[service]`; dice/эффектов/спойлера/перевода нет | `TelegramTypes.cpp:515` |
| 3.14 | Треды, топики, обсуждения | `MessageThreadScreen.kt`, `ForumTopicsScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | Ни модели, ни экрана, ни запросов | grep `src/` — 0 |

### 4. Медиа, загрузки и воспроизведение

**Android в двух словах:** Импорт медиа со сжатием и лимитами; до 10 вложений с отправкой альбомом; встроенная камера и видеозаметки с диагностикой; фоторедактор; inline-видео на ExoPlayer и viewer-pager; запись голосовых через MediaRecorder; политика автодокачки с учётом сети и Data Saver; учёт трафика; экран хранилища через getStorageStatistics/optimizeStorage; shared media в профиле.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 4.1 | Отправка одного фото/видео/документа с подписью | `MediaComposer.kt:1039`, `OutgoingMediaImporter.kt:32`, `MediaLimits.kt:11` | `MediaSend.cpp:129-180`, `AppController.cpp:1117`, `ChatsPage.qml:1004` | PARTIAL | COMPLETE | COMPLETE | COMPLETE | Тип по расширению; нет лимитов (10 МБ фото / 2 ГБ видео), сжатия, прогресса, «как файл»/spoiler/self-destruct | `MediaSend.cpp:180` |
| 4.2 | Альбомы и мультивложение до 10 файлов | `MediaComposer.kt:369-471`, `SendMessageAlbum`, `MessageAlbumGroupingTest` | `AlbumSend.*`, `AlbumModel.*`, `AlbumGrouper.*`, `AlbumGrid.qml` | STUB | MISSING | MISSING | COMPLETE | `sendAlbumRequest` не вызывается; альбомы не отправляются и не группируются при показе | `AlbumSend.cpp` |
| 4.3 | Импорт, сжатие и лимиты медиа | `OutgoingMediaImporter.kt`, `VideoCompression*`, `AudioCompression*`, `VideoImportPolicy.kt` | — | MISSING | MISSING | MISSING | MISSING | Windows отдаёт путь в TDLib без анализа и перекодирования | — |
| 4.4 | Фоторедактор (кроп, поворот, качество) | `PhotoEditorScreen.kt`, `PhotoEditor.kt`, `PhotoEditPolicy.kt` | — | MISSING | MISSING | MISSING | MISSING | Отсутствует полностью | — |
| 4.5 | Камера и видеозаметки | `CameraCaptureScreen.kt`, `VideoNoteRecorder.kt` (67 КБ), `VideoNoteDiagnostics.kt` | — | MISSING | MISSING | MISSING | MISSING | Ни камеры, ни записи, ни диагностики | grep `QCamera` — 0 |
| 4.6 | Загрузка входящих: превью и полный файл с прогрессом | `MediaFileStore.kt`, `TdMediaMapper.kt`, `MediaViews.kt:347` | `MediaFileStore.cpp:141`, `MediaImageProvider.*` | PARTIAL | PARTIAL | PARTIAL | PARTIAL | Автоматически качается только превью; полный файл — по клику; процента, паузы и отмены нет; `MediaFileState` не используется | `TelegramRepository.cpp:1406` |
| 4.7 | Политика автодокачки, очередь и правила кэша | `AutoDownloadPolicy.kt`, `AutoDownloadNetwork.kt`, `AutoDownloadTdlibMapping.kt:19`, TdRepo `:2267` | `AutoDownloadPolicy.*`, `MediaDownloadQueue.*`, `MediaCachePolicy.*` | PARTIAL | MISSING | MISSING | COMPLETE | Все три модели не инстанцируются; `autoDownloadFile` и `setOption(autoDownloadSettings)` не отправляются; сеть не детектируется | grep — 0 |
| 4.8 | Просмотрщик фото: fullscreen, zoom, листание | `MediaViews.kt:879`, `MediaViewerPager.kt`, `MediaViewerPolicy.kt` | `MediaViewerController.cpp:81`, `MediaViewerOverlay.qml` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | Нет листания, zoom/pan и share; ошибка не различает «нет файла» и «нет сети» | `tst_media_viewer` |
| 4.9 | Воспроизведение видео | `MediaViews.kt:1474` (ExoPlayer inline, autoplay) | `MediaPlaybackController.cpp:62`, `MediaPlaybackOverlay.qml` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | Только fullscreen-оверлей по кнопке; inline/autoplay нет; `VideoPlayerState` не используется | `QMediaPlayer` + `VideoOutput` |
| 4.10 | Голосовые: запись с микрофона и волна | `AudioNoteRecorder.kt:63` (MediaRecorder), `VoiceBubble.kt` | `VoiceRecordingController.*`, `VoiceWaveformModel.*` (мёртвы); плеер общий | PARTIAL | MISSING | PARTIAL | COMPLETE | Записи с микрофона нет вообще (`QAudioSource`/`QMediaRecorder` отсутствуют); воспроизведение идёт через fullscreen-плеер без волны | grep — 0 |
| 4.11 | Хранилище, трафик и очистка кэша | `StorageSettingsScreen.kt:82`, `TdStorage.kt:30` (getStorageStatistics/optimizeStorage), `TrafficUsage.kt` | `StorageStatsModel.*`, `CacheCleanupController.*`, `DataUsageModel.*` | STUB | MISSING | MISSING | COMPLETE | Ни одного TDLib-запроса хранилища; три модели никем не наполняются | grep `getStorageStatistics` — 0 |
| 4.12 | Общая галерея медиа чата (shared media) | `SharedMediaSection.kt:110`, `SharedMediaWindow.kt` | `MediaGalleryController.*` | STUB | MISSING | MISSING | COMPLETE | Галереи и вкладок Media/Files/Links/Voice в профиле нет | `tst_media_gallery_controller` |

### 5. Музыка и плейлисты

**Android в двух словах:** Per-account музыкальная библиотека (плейлисты, избранное, история, умные подборки) с атомарной записью; контейнер .bgmusic v1 с импортом, экспортом и карточкой в чате; очередь на ExoPlayer с shuffle, repeat, speed, sleep timer и восстановлением сессии; эквалайзер; BG Beat и BG Mix через JNI; поиск по своим аудио и inline-музыкоботу; музыка профиля.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 5.1 | Музыкальная библиотека per-account (JSON, бэкапы, карантин) | `MusicModels.kt`, `MusicLibraryFileStore.kt`, `MusicLibraryJson.kt`, `MusicLibraryRepository.kt` | — | MISSING | MISSING | MISSING | MISSING | Схемы v1, лимитов и атомарной записи нет | — |
| 5.2 | Плейлисты, избранное, обложки, сортировка, moveTrack | `MusicLibraryEdits.kt`, `MusicLibraryRepository.kt:207-338` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 5.3 | История прослушиваний, recent, умные подборки | `MusicHistoryRecorder.kt`, `MusicSmartCollections.kt` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 5.4 | Контейнер `.bgmusic` v1: чтение, запись, импорт, экспорт, карточка в чате | `data/music/bgmusic/*` (11 файлов), `ui/screens/bgmusic/*` | — | MISSING | MISSING | MISSING | MISSING | Спека скопирована в `docs/formats/bgmusic-v1.md`, кода нет | — |
| 5.5 | Музыкальный UI: My Music, плейлист, поиск, mini player, полный плеер | `ui/screens/music/*`, `NowPlayingBar.kt`, `MusicPlayerSheet.kt` | — | MISSING | MISSING | MISSING | MISSING | В `Shell.qml` нет музыкального раздела | `Shell.qml:135-170` |
| 5.6 | Плеер: очередь, shuffle, repeat, speed, sleep timer, restore сессии | `PlaybackEngine.kt` (160 КБ), `PlaybackShuffleOrder.kt`, `MusicSession*` | `MediaPlaybackController.cpp` (один файл), `MediaPlaylistModel.*` (мёртв) | STUB | MISSING | PARTIAL | COMPLETE | Только проигрывание одного файла; очередь, shuffle и restore не подключены | `tst_media_playlist_model` |
| 5.7 | Эквалайзер | `Equalizer{Controller,Runtime,Store}.kt`, `EqualizerScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | Привязан к `audioSessionId`, на `QMediaPlayer` невозможен без отдельного DSP | — |
| 5.8 | BG Beat (BPM/биты) и BG Mix (2 деки, переходы) | `media/bgbeat/*`, `media/bgmix/*`, `app/src/main/cpp/*_jni.cpp` | `media/BgMediaStatus.*` (строка возможностей) | STUB | STUB | STUB | MISSING | Сабмодуль `third_party/bg-media` не инициализирован, `BEYGRAM_WITH_BGMEDIA=OFF` | `EnvironmentPage.qml:149` |
| 5.9 | Музыка профиля (getUserProfileAudios, add/remove/move) | `ProfileMusic.kt`, `ProfileMusicModel.kt`, `domain/model/ProfileAudio.kt` | — | MISSING | MISSING | MISSING | MISSING | Полей аудио в профиле Windows нет | — |
| 5.10 | Аудио в чате: play, очередь из истории, add-to-playlist | `ChatMusicQueue.kt`, `ChatScreen.kt:1868` | `MessageBubble.qml:255`, `AppController.cpp:802` | PARTIAL | PARTIAL | PARTIAL | COMPLETE | Один файл в оверлее, без очереди и метаданных | `tst_media_playback` |
| 5.11 | Провайдеры треков (Yandex), музыкальный inline-бот | `provider/*.kt`, `BotTrackResolver.kt`, `YandexTrackResolver.kt`, `MusicSearchScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | Ни `getInlineQueryResults`, ни фильтра audio | — |
| 5.12 | Синхронизированные тексты (LRC) | `domain/lyrics/SyncedLyrics.kt` (только парсер) | — | MISSING | MISSING | MISSING | MISSING | На Android тоже нет UI-потребителя | — |

### 6. Истории

**Android в двух словах:** Лента историй в списке чатов; полноэкранный вьюер с прогресс-сегментами, паузой, переходами, реакциями и ответами; публикация фото и видео с подготовкой под лимиты и приватностью; архив и «оставить в профиле»; около 12 story-запросов TDLib.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 6.1 | Story-бар в списке чатов (кольца, «Добавить», self-first) | `StoryBar.kt:57`, `Stories.kt:179`, `ChatsScreen.kt:745` | — | MISSING | MISSING | MISSING | MISSING | Нет ни модели `ChatStories`, ни виджета | — |
| 6.2 | Активные истории, пагинация и live-updates | `TdStories.kt:158,164,208,309` | `StoryRequests.cpp:124` (один билдер + парсер) | PARTIAL | MISSING | MISSING | COMPLETE | Нет подписки на `updateChatActiveStories`, нет состояния | `StoryRequests.h:67` |
| 6.3 | Полноэкранный вьюер и загрузка медиа | `StoryViewerScreen.kt:109`, `StoryMediaViews.kt:66` | `StoryRequests.cpp:132` (builder `getStory`) | PARTIAL | MISSING | MISSING | COMPLETE | Нет экрана, роута и story-медиапровайдера | — |
| 6.4 | Прогресс-сегменты, таймеры, пауза, next/prev | `StoryViewerScreen.kt:321-410`, `StoryRules.kt:71` | `StoryProgressModel.*`, `StoryViewerController.*` | PARTIAL | MISSING | MISSING | COMPLETE | Обе модели не создаются и не связаны друг с другом; таймера нет | `StoryProgressModel.h:10` |
| 6.5 | Отметка просмотра (openStory/closeStory) | `TdStories.kt:438-452` | `StoryRequests.cpp:144,153` (билдеры) | PARTIAL | MISSING | MISSING | COMPLETE | Запросы никогда не отправляются | — |
| 6.6 | Реакции и ответы на историю | `TdStories.kt:459-468`, `StoryViewerScreen.kt:785` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 6.7 | Публикация истории и подготовка медиа | `TdStories.kt:995-1088` (PostStory), `StoryPublisherScreen.kt`, `StoryMediaPreparer.kt` | — | MISSING | MISSING | MISSING | MISSING | Нет ни postStory, ни лимитов 1080×1920 / 720×1280 ≤60 с | — |
| 6.8 | Архив историй, «оставить в профиле», приватность и close friends | `StoryArchiveViewerScreen.kt`, `TdStories.kt:500-575,1268` | — | MISSING | MISSING | MISSING | MISSING | Приватность и `isForCloseFriends` не парсятся | — |
| 6.9 | Удаление и правка подписи истории | `TdStories.kt:491,556` | — | MISSING | MISSING | MISSING | MISSING | — | — |

### 7. Стикеры, GIF и эмодзи

**Android в двух словах:** Одна кнопка в композере открывает панель с табами Emoji, Stickers и GIF: недавние, избранное, установленные наборы, поиск; отправка стикера и GIF; TGS проигрывается через Lottie, WebM-стикеры и кастомные эмодзи сведены к статичной миниатюре.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 7.1 | Вход в стикер-панель и панель с табами | `ChatScreen.kt:2175,2604`, `StickerPanel.kt:133`, `StickerPanelLogic.kt` | — | MISSING | MISSING | MISSING | MISSING | В композере только Attach и Send | `ChatsPage.qml:772` |
| 7.2 | Установленные и архивные наборы | `TdStickerCatalog.kt:48` (GetInstalledStickerSets) | `StickerCatalog.cpp:153-173` (билдеры + парсер) | PARTIAL | MISSING | MISSING | COMPLETE | Ни один запрос не отправляется | grep — 0 |
| 7.3 | Превью набора, установка и удаление | `TdStickerCatalog.kt:55,64`, `StickerSetSheet.kt:169` | `StickerInfo.cpp:96`, `StickerSetInstallController.cpp:99` | STUB | MISSING | MISSING | COMPLETE | `setInstalled()` меняет только локальный флаг; `changeStickerSet` не вызывается | `StickerSetInstallController.cpp:99` |
| 7.4 | Недавние и избранные стикеры | `TdStickerCatalog.kt:34-43` | `RecentStickerModel.*` (только ids) | STUB | MISSING | MISSING | COMPLETE | `getRecentStickers` и `getFavoriteStickers` не строятся | — |
| 7.5 | Поиск стикеров | `TdStickerCatalog.kt:75` (SearchStickers) | `StickerSearchController.*` | STUB | MISSING | MISSING | COMPLETE | Билдера `searchStickers` нет вовсе | — |
| 7.6 | Отправка стикера | `OutgoingMediaContent.kt:63`, `StickerPanel.kt:208` | `StickerSend.cpp:47`, `StickerSendController.cpp:37` | PARTIAL | MISSING | MISSING | COMPLETE | Корректный `sendMessage`+`inputMessageSticker`, но не подключён | `StickerSend.cpp:47` |
| 7.7 | Анимация TGS/WebM | Lottie `StickerViews.kt:113,170`, `TgsReader.kt` | — | MISSING | MISSING | MISSING | MISSING | В Windows нет lottie/rlottie/TGS/WebM | grep — 0 |
| 7.8 | Панель эмодзи, каталог, недавние | `EmojiPanel.kt:55`, `EmojiCatalog.kt`, `RecentEmojiStore` | `EmojiCatalogModel.*` (value-holder) | STUB | MISSING | MISSING | COMPLETE | Панели эмодзи нет; единственный emoji в QML — `ReactionPicker.qml` | — |
| 7.9 | Кастомные эмодзи | `TdCustomEmojis.kt:64`, `StickerViews.kt:200` | `TextFormatting.cpp:187` оставляет литерал | MISSING | MISSING | MISSING | MISSING | Кастомные эмодзи не резолвятся ни в тексте, ни в реакциях | `TextFormatting.cpp:187` |
| 7.10 | GIF: каталог (недавние, сохранённые, поиск) и отправка | `TdStickerCatalog.kt:84,149,108`, `MediaComposer.kt:324` | — | MISSING | MISSING | MISSING | MISSING | `OutgoingMediaKind` не содержит animation | `MediaSend.h:10` |
| 7.11 | Отрисовка входящего стикера/GIF | Lottie/WebM + миниатюра | `MessageBubble.qml:238`, `StickerBubble.qml` | PARTIAL | COMPLETE | PARTIAL | MISSING | Статичная миниатюра, `animated: false` жёстко, бейдж ANIM не появляется | `StickerBubble.qml:6` |

### 8. Реакции

**Android в двух словах:** Чипы реакций с подсветкой своей; пикер из GetMessageAvailableReactions (top, recent, набор чата, activeEmoji для Premium); toggle с вытеснением по лимиту; платные Stars-реакции через add/commit/rollback; custom emoji; живое обновление из UpdateMessageReactions и UpdateMessageInteractionInfo.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 8.1 | Полоса реакций под сообщением (emoji, custom, paid, счётчик, своя) | `MessageReactions.kt:66-116` | `ReactionBar.qml:62-126`, `MessageBubble.qml:346-352` | PARTIAL | PARTIAL | COMPLETE | MISSING | JSON реакций разбирается инлайн в QML, `MessageReactions::reactionsFromMessage` для баров не используется | `MessageBubble.qml:57-82` |
| 8.2 | Добавление реакции тапом по существующему чипу | `ChatScreen.kt:503`, `TdConversationActions.kt:697-716` | `TelegramRepository.cpp:925-940`, `MessageReactions.cpp:114` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | Реальный `addMessageReaction`; без оптимистичного обновления и без обновления UI после ответа | `AppController.cpp:1206` |
| 8.3 | Удаление своей реакции повторным тапом | `TdConversationActions.kt:701-704` | `MessageReactions.cpp:140` (builder) | PARTIAL | MISSING | MISSING | PARTIAL | Билдер не вызывается и формирует `reaction_type: null`, что невалидно для pinned TDLib | `MessageReactions.h:43` |
| 8.4 | Пикер доступных реакций из TDLib | `TdConversationActions.kt:685`, `TdMessageReactions.kt:47` | `ReactionCatalog.cpp:245-326`, `ReactionPickerModel.*` | COMPLETE | MISSING | MISSING | COMPLETE | Парсер и модель готовы, `getMessageAvailableReactions` не строится; `ReactionPicker.qml` не инстанцируется | `ReactionCatalog.cpp:245` |
| 8.5 | Реакция на сообщение без реакций | `ChatScreen.kt:1633` (кнопка в меню и long-press) | `ReactionBar.qml:129` | PARTIAL | PARTIAL | MISSING | MISSING | В `MessageBubble.qml:350` жёстко `canReact: false`, поэтому «+» недостижим | `MessageBubble.qml:350` |
| 8.6 | Custom emoji реакция | `TdMessageReactions.kt:60,72`, `MessageReactions.kt:120` | `MessageReactions.cpp:67,128`, `ReactionBar.qml:24` | PARTIAL | MISSING | MISSING | PARTIAL | Сигнал `customReactionToggled` никем не ловится; id рисуется как глиф | — |
| 8.7 | Живое обновление реакций | `TdLibTelegramRepository.kt:2716-2728` | — | MISSING | MISSING | MISSING | MISSING | `updateMessageReactions`/`updateMessageInteractionInfo` не обрабатываются; UI не меняется до переоткрытия чата | `TelegramRepository.cpp:1144-1393` |
| 8.8 | Платные Stars-реакции | `TdPaidReactions.kt:19-52`, `PaidReactionDialog.kt` | только отрисовка `reactionTypePaid` | MISSING | MISSING | MISSING | MISSING | Запросов add/commit/rollback нет | — |
| 8.9 | Лимит реакций и вытеснение | `TdConversationActions.kt:706-712`, `ReactionReplacement.kt` | `ReactionCatalog.cpp:218-226` | COMPLETE | MISSING | MISSING | COMPLETE | Лимит парсится, но отправка его не соблюдает | `tst_reaction_catalog` |

### 9. Поиск

**Android в двух словах:** Единый экран поиска с четырьмя секциями (чаты, контакты, публичные чаты, сообщения), фильтрами и offset-пейджингом; отдельный поиск внутри чата с фильтрами, подсветкой совпадений и переходом к сообщению; серверный поиск через SearchChats, SearchChatsOnServer, SearchContacts и SearchPublicChats.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 9.1 | Поиск внутри чата | `ChatSearch.kt`, `ChatScreen.kt:1288-1334` | `TelegramRepository.cpp:618-651`, `MessageSearch.cpp:68`, `ChatsPage.qml:105-116` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | 50 хитов, без пейджинга, фильтров и подсветки; клик только закрывает панель | `MessageSearch.cpp:72` |
| 9.2 | Фильтры поиска в чате (отправитель, тип, даты) | `ChatSearchFilter.kt`, `ChatSearchRequest.kt:15-44` | `SearchFilters.cpp:88-106` | STUB | MISSING | MISSING | COMPLETE | Билдеры мертвы; `filter`/`sender_id` всегда null | grep — 0 |
| 9.3 | Глобальный поиск сообщений с пейджингом | `TdConversationActions.kt:786`, `GlobalSearchScreen.kt:455`, `SearchPaging.kt` | `TelegramRepository.cpp:661-728`, `GlobalSearchModel.*` | COMPLETE | COMPLETE | PARTIAL | COMPLETE | Реальный `next_offset` и кнопка Load more; нет секций и подсветки | `TelegramRepository.cpp:691` |
| 9.4 | Секции глобального поиска: чаты, люди, публичные чаты | `GlobalSearchDiscovery.kt:50`, `GlobalSearchScreen.kt:304-410` | только плоский список хитов | MISSING | MISSING | MISSING | MISSING | Ни `searchChats`, ни `searchContacts`, ни `searchPublicChats` в Windows не отправляются | — |
| 9.5 | Фильтры глобального поиска (тип, тип чата, период) | `GlobalSearchFilter.kt`, `GlobalSearchScreen.kt:411-433` | `SearchFilters.h:32-37` (частично) | STUB | MISSING | MISSING | COMPLETE | `chat_type_filter` жёстко null | `MessageSearch.cpp:93` |
| 9.6 | Поиск по списку чатов | `SearchChats` + `SearchChatsOnServer`, `TdLibTelegramRepository.kt:1134` | `ChatFilterModel.cpp:22-33` (локальный proxy) | PARTIAL | COMPLETE | COMPLETE | MISSING | Локальная подстрока по title/preview вместо серверного поиска | `ChatsPage.qml:222` |
| 9.7 | Публичные чаты и username | `TdLibTelegramRepository.kt:1171,1250` | `DeepLinks.cpp:682` (builder) | STUB | MISSING | MISSING | MISSING | Ввода `@name` и выдачи публичных чатов нет | — |
| 9.8 | Поиск контактов | `TdContacts.kt:38` | `ContactSearch.cpp:108-148` | COMPLETE | MISSING | MISSING | COMPLETE | Билдер не вызывается; экрана контактов нет | `tst_contact_search` |
| 9.9 | Переход к сообщению из результата и его подсветка | `ChatScreen.kt:1336-1350`, `BeyGramApp.kt:2329` | `Shell.qml:223-226` | STUB | MISSING | MISSING | COMPLETE | `messageId` молча отбрасывается; `MessageJumpController` не инстанцируется | `Shell.qml:223` |
| 9.10 | Подсветка совпадений в сниппете | `ChatSearch.kt:403-483` | `SearchResultModel`/`GlobalSearchModel` отдают plain text | MISSING | MISSING | MISSING | MISSING | Роль `textHtml` есть, QML её не использует | — |

### 10. Контакты и профили

**Android в двух словах:** Список контактов с алфавитными группами и онлайн-статусом; добавление, импорт, правка и удаление; профиль с фото, био, username, телефоном, last seen, галереей, музыкой и общими медиа; редактор своего профиля со сменой аватара; участники и администрирование групп; блок-лист; настройки last seen.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 10.1 | Список контактов с A-Z группами и онлайн-фильтром | `ContactsScreen.kt:96`, `TdContacts.kt:22` | `ContactSortModel.*`, `ContactRequests.cpp:53` | COMPLETE | MISSING | MISSING | COMPLETE | Экрана контактов нет; модель не инстанцируется | grep — 0 |
| 10.2 | Добавление, импорт и удаление контактов | `TdContacts.kt:49-75` | `ContactRequests.cpp:62-92` | COMPLETE | MISSING | MISSING | COMPLETE | Билдеры не вызываются | — |
| 10.3 | Профиль пользователя/группы (фото, био, телефон, тип, участники) | `ProfileScreen.kt:82`, `TdProfiles.kt:583` | `TelegramRepository.cpp:730-800`, `UserProfile.cpp:148-198` | PARTIAL | PARTIAL | PARTIAL | MISSING | Панель только для чтения; фото берётся из `currentChatPhotoFileId`, а не `profilePhotoFileId` | `ChatsPage.qml:863` |
| 10.4 | Действия в профиле: написать, позвонить, добавить, заблокировать, общие медиа | `ProfileScreen.kt:82`, `ChatInfoMenu.kt`, `SharedMediaSection.kt` | — | MISSING | MISSING | MISSING | MISSING | В панели только Close | `ChatsPage.qml:832-920` |
| 10.5 | Редактирование своего профиля (имя, username, био) | `ProfileScreen.kt:641`, `TdProfiles.kt:379` | `ProfileEdit.cpp`, `TelegramRepository.cpp:406-433`, `SettingsPage.qml:229` | PARTIAL | PARTIAL | PARTIAL | MISSING | Пустые username/bio не отправляются (нельзя очистить); поля не предзаполнены; нет проверки формата | `TelegramRepository.cpp:413` |
| 10.6 | Смена и удаление аватара | `TdProfiles.kt:366,396` | `ProfileEdit.cpp` (`deleteProfilePhotoRequest`) | PARTIAL | MISSING | MISSING | COMPLETE | Билдер есть, `setProfilePhoto` не смоделирован | — |
| 10.7 | Галерея фото профиля и музыка профиля | `ProfilePhotoGallery.kt`, `ProfileMusic.kt`, `TdProfilePhotos.kt`, `TdProfileAudios.kt` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 10.8 | Онлайн-статус и last seen | `TdProfiles.kt:583`, `ChatPresenceTest`, `LastSeenSettingsStore.kt` | `PresenceRequests.*`, `UserStatusModel.*` | STUB | MISSING | MISSING | COMPLETE | `updateUserStatus` явно выбрасывается в `TelegramUpdateScheduler.cpp:67-74` | `TelegramUpdateScheduler.cpp:71` |
| 10.9 | Блокировка пользователей и блок-лист | `TdPrivacy.kt:184-201`, `BlockedSendersScreen.kt` | `ChatBlocking.*`, `PrivacyRequests.cpp:177-193` | COMPLETE | MISSING | MISSING | COMPLETE | Дублирующие билдеры, ни один не вызывается | — |
| 10.10 | Список участников группы/канала | `GroupMembersScreen.kt`, `ChatMembersScreen.kt`, `TdGroupAdministration.kt:69` | `ChatMembers.cpp`, `ChatMemberListModel.*` | COMPLETE | MISSING | MISSING | COMPLETE | Списка участников в UI нет | — |
| 10.11 | Администрирование: promote/demote/ban/restrict, заявки, инвайты | `GroupManagementScreen.kt`, `GroupInvitesScreen.kt`, `GroupJoinRequestsScreen.kt`, `TdGroupAdministration.kt:154-319` | `ChatPermissions.*`, `ChatMemberPermissionsModel.*`, `ChatInviteLinkModel.*` | STUB | MISSING | MISSING | COMPLETE | Только парсеры; запросов управления правами нет | — |
| 10.12 | Создание группы и канала | `TdLibTelegramRepository.kt:909,980` | — | MISSING | MISSING | MISSING | MISSING | — | — |

### 11. Звонки

**Android в двух словах:** Реального VoIP нет: единственная возможность — read-only вкладка «Звонки» с историей из сообщений messageCall (searchCallMessages) и отрисовка звонков в чате как событий с направлением, статусом и длительностью. Ни одного call-control запроса в Android нет.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 11.1 | Вкладка «Звонки» и история звонков | `CallsScreen.kt`, `TdCallHistory.kt:20` (`SearchCallMessages`) | `CallHistory.*`, `CallHistoryModel.*` | STUB | MISSING | MISSING | COMPLETE | Запрос никогда не отправляется, страницы нет, строка истории не рисуется | `CallHistory.cpp:98` |
| 11.2 | Строка истории: направление, статус, длительность, аватар | `CallsScreen.kt:154-222` | `CallHistoryModel` (только `missed`/`withUser`) | STUB | MISSING | MISSING | COMPLETE | Нет форматирования, локализации и UI | — |
| 11.3 | Пузырь-событие звонка в чате | `ChatScreen.kt:1817,3073` (`CallMessageChip`), `TdServiceMessageMapper.kt:62` | `TelegramTypes.cpp:287,358` | PARTIAL | COMPLETE | PARTIAL | MISSING | Рисуется обычный текстовый пузырь с литералом «Call», без направления и длительности | `TelegramTypes.cpp:287` |
| 11.4 | Локализованный текст и формат длительности | `TdServiceMessageMapper.kt:42-71`, `CallEventDurationTest.kt` | — | MISSING | MISSING | MISSING | MISSING | Строк `service_call_*` нет | — |

Android VoIP не имеет: grep по `CreateCall|DiscardCall|JoinGroupCall|callId|Voip` даёт 0 совпадений. Поэтому отсутствие VoIP на Windows — не регресс, а паритет.

### 12. Уведомления

**Android в двух словах:** Полная система на TDLib notification API: группы уведомлений, per-chat mute/preview/mentions, foreground service для фонового получения, inline-reply с outbox и токенами, trigger-words и отдельный фид упоминаний.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 12.1 | Доставка уведомления о новом сообщении | `AndroidNotificationRenderer.kt:80`, `TdNotifications.kt:54` | `NotificationFormatter.cpp:129` → `TelegramRepository.cpp:1304` → `AppController.cpp:648` | COMPLETE | PARTIAL | COMPLETE | COMPLETE | Своя эвристика вместо TDLib notification groups; нет mention-групп и восстановления после реконнекта | `AppController.cpp:662` |
| 12.2 | Клик по уведомлению открывает чат | `NotificationIntents.kt:78` | — | MISSING | MISSING | MISSING | MISSING | `messageClicked` не подключён | grep — 0 |
| 12.3 | Inline-reply из уведомления | `AndroidNotificationRenderer.kt:114-158`, `ReplyOutbox.kt`, `NotificationReplyToken.kt` | — | MISSING | MISSING | MISSING | MISSING | Нет ни action, ни очереди, ни токена | — |
| 12.4 | Per-chat mute в доставке | `TdLibTelegramRepository.kt:2645`, `ChatMuteState.kt` | `TelegramRepository.cpp:375,1264`, `NotificationFormatter.cpp:148` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | Mute читается как `mute_for>0`; scope и `use_default_mute_for` игнорируются | `ChatsPage.qml:1082` |
| 12.5 | Per-chat звук, превью и упоминания | `ChatSettingsSheet.kt:104-137`, `TdLibTelegramRepository.kt:713` | `ChatNotificationController.*`, `ChatNotificationRequests.cpp:96`, `ChatNotificationSettings.qml` | COMPLETE | MISSING | MISSING | COMPLETE | Класс и QML не инстанцируются; `setChatMuted` затирает per-chat значения в «inherit» | — |
| 12.6 | Глобальные переключатели и их хранение | `NotificationPreferences.kt:15` | `NotificationSettingsStore.cpp:122`, `AppController.cpp:99` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | Хранятся 4 поля; `sound` и `notifyMutedChats` не читаются и не выведены | `SettingsPage.qml:136-150` |
| 12.7 | Подавление превью и приватность | `NotificationPreviewPolicy.kt:9-19`, `AndroidNotificationRenderer.kt:86-108` | `AppController.cpp:662`, `NotificationFormatter.cpp:144` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | Только глобальное скрытие текста; per-chat `show_preview` не учитывается; secret-чаты отсекаются | `TelegramRepository.cpp:1300` |
| 12.8 | Группировка и сводка уведомлений | `TdNotifications.kt:69-83` (группы, MessagingStyle) | `NotificationGrouping.*` | COMPLETE | MISSING | MISSING | COMPLETE | Модель не вызывается из реального пути | — |
| 12.9 | Роутер решения (mute/mention/sound/preview) и звук | Android-каналы + `notification_sound_id` | `NotificationRouter.*`, `NotificationSoundModel.*` | COMPLETE | MISSING | MISSING | COMPLETE | Два параллельных мёртвых слоя; `QSoundEffect` отсутствует | — |
| 12.10 | Badge и суммарный счётчик непрочитанного | launcher badge | `NotificationBadgeController.*`, `UnreadCounter.*` | COMPLETE | MISSING | MISSING | COMPLETE | Нет taskbar overlay и трей-бейджа | grep — 0 |
| 12.11 | Упоминания | группы `NotificationGroupTypeMentions` | `ChatNotificationRequests.h:39` (только поле) | STUB | MISSING | MISSING | PARTIAL | `updateNewMessage` не отличает упоминание | — |
| 12.12 | Trigger words и keyword alerts | `TriggerWordsStore.kt:118`, `TriggerAlerts.kt:25`, `TriggerWordsScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | Ни правил, ни матчера, ни фида | — |
| 12.13 | Фоновая доставка при закрытом окне | `TelegramNotificationService.kt:56` (FGS) | только пока процесс жив | MISSING | MISSING | MISSING | MISSING | Нет `setQuitOnLastWindowClosed(false)` и иконки в трее при старте; при закрытии окна процесс завершается | `main.cpp:102-117` |

### 13. Premium, Stars, подарки и платежи

**Android в двух словах:** Premium (статус, фичи, покупка через внешнюю ссылку, gift-code), Stars (баланс, история, пополнение), подарки (каталог, получение, отправка), платный контент и инвойсы, платные реакции и бусты. Реальные деньги никогда не проводятся внутри приложения — только внешние HTTPS-страницы.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 13.1 | Premium: статус и список возможностей | `TdPayments.kt:66` (GetPremiumState+Features), `PremiumScreen.kt` | `PremiumRequests.*`, `PremiumStatusModel.*` | PARTIAL | MISSING | MISSING | COMPLETE | Билдеры ссылаются на методы, удалённые в TDLib 1.8.69; ничего не отправляется | `PremiumRequests.h:33` |
| 13.2 | Покупка Premium и gift-code | `TdPayments.kt:108-238` | — | MISSING | MISSING | MISSING | MISSING | Ни ссылки, ни применения кода | — |
| 13.3 | Stars: баланс и история транзакций | `TdPayments.kt:323-554`, `StarsScreen.kt` | `StarBalanceModel.*`, `StarTransactionModel.*`, `PaymentRequests.cpp:158-175` | PARTIAL | MISSING | MISSING | COMPLETE | Модели не наполняются; ответ не парсится | — |
| 13.4 | Stars: пополнение | `TdPayments.kt:247` (GetStarPaymentOptions→GetPaymentForm) | `StarPurchaseModel.*` (выбор пакета) | STUB | MISSING | MISSING | COMPLETE | Нет `StarPaymentOption` и формы | — |
| 13.5 | Подарки: каталог, полученные, отправка | `TdPayments.kt:331-404`, `GiftsScreen.kt` | `GiftRequests.*`, `GiftCatalogModel.*`, `GiftSendController.cpp:130` | PARTIAL | MISSING | MISSING | COMPLETE | `CanSendGift`/`GetGiftCollections` отсутствуют; контроллер не инстанцируется | — |
| 13.6 | Платный контент: paid media и инвойсы | `TdPayments.kt:496-534` (реальный SendPaymentForm), `PaidContentViews.kt` | `PaymentCheckoutModel.*`, `PaymentRequests.*` | PARTIAL | MISSING | MISSING | COMPLETE | `messageInvoice`/`messagePaidMedia` не парсятся — сообщение отрисуется пустым | `TelegramTypes.cpp:243-292` |
| 13.7 | Платные реакции | `TdPaidReactions.kt:23-37`, `PaidReactionDialog.kt` | — | MISSING | MISSING | MISSING | MISSING | Только отрисовка счётчика | — |
| 13.8 | Checkout, методы оплаты, карта | Android: только внешний HTTPS-URL (`Monetization.kt:322`) | `PaymentCheckoutModel.*`, `PaymentMethodModel.*`, `PaymentCardModel.*` | STUB | MISSING | MISSING | COMPLETE | `sendPaymentFormRequest` возвращает `{}` при providerToken; карта вводится в память, но отправить некуда | `PaymentRequests.cpp:139` |
| 13.9 | Внешние платёжные ссылки и их валидация | `Monetization.kt:322`, `PaymentExternalLink.kt:21` | — | MISSING | MISSING | MISSING | MISSING | Нет валидации платёжных URL | — |
| 13.10 | Бусты чата | Android: только метка фичи Premium | `BoostModel.*` | STUB | MISSING | MISSING | COMPLETE | Ни `getChatBoosts`, ни `boostChat` | `BoostModel.h:26` |
| 13.11 | Бейдж Premium у пользователя | `UserProfile.isPremium` → UI | `UserProfile.cpp:162` парсит флаг | COMPLETE | PARTIAL | MISSING | COMPLETE | Флаг нигде не читается в UI | grep — 0 |

### 14. Опросы и геопозиция

**Android в двух словах:** Создание обычного опроса и квиза (2–10 вариантов, анонимность, несколько ответов, правильные ответы, пояснение); голосование, смена и отзыв голоса, живые результаты, список проголосовавших, остановка и добавление варианта; геолокация и живая геолокация, venue, контакт, dice и открытие карты.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 14.1 | Отправка опроса (вопрос, варианты, анонимность) | `ComposerExtras.kt:65-190`, `TdConversationActions.kt:332-337` | `PollSend.*`, `PollComposerModel.*`, `AppController.cpp:1230`, `ChatsPage.qml:1022-1029` | PARTIAL | COMPLETE | COMPLETE | COMPLETE | Нет гейта по `canSendPolls`, нет reply для опроса | `PollSend.cpp:26-45` |
| 14.2 | Quiz: правильный ответ и пояснение | `ComposerExtras.kt:97-190` (несколько правильных, блокировка анонимности) | `PollSend.cpp:35-45`, `PollComposerDialog.qml:95-109` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | Только один правильный ответ; anonymous не блокируется; индекс правильного ответа может съехать при пустом варианте | `PollComposerDialog.qml:38-48` |
| 14.3 | Голосование: несколько ответов, смена и отзыв | `TdConversationActions.kt:365-370`, `SpecialMessageViews.kt:211-243` | `PollInfo.cpp:93-105`, `TelegramRepository.cpp:1014` | PARTIAL | PARTIAL | PARTIAL | COMPLETE | Всегда `setPollAnswer` с одним индексом; отзыв голоса невозможен | `PollInfo.cpp:93` |
| 14.4 | Результаты: проценты, объяснение, правильный ответ, voters | `SpecialMessageViews.kt:166-292,476-557` | `PollBubble.qml`, `TelegramTypes.cpp:476-491` | PARTIAL | PARTIAL | PARTIAL | COMPLETE | Нет `vote_percentage`, `is_being_chosen`, explanation и списка голосующих; вопрос дублируется | `PollBubble.qml:51-63` |
| 14.5 | Живое обновление результатов | `TdLibTelegramRepository.kt:2672-2685` | `MessageListModel.cpp:200-221`, `TelegramRepository.cpp:1355` | MISSING | PARTIAL | MISSING | MISSING | `updateContent` меняет только text/contentType/textHtml; poll-роли не пересчитываются | `MessageListModel.cpp:200` |
| 14.6 | Остановка опроса | `TdConversationActions.kt:372-375`, `SpecialMessageViews.kt:263` | `PollInfo.cpp:107`, `PollAdmin.cpp:58`, `PollResults.*` | STUB | MISSING | MISSING | COMPLETE | Билдеры не вызываются, Q_INVOKABLE нет | — |
| 14.7 | Добавление варианта и список проголосовавших | `TdConversationActions.kt:381-396`, `SpecialMessageViews.kt:294-465` | `PollAdmin.*`, `PollVoteModel.*` | STUB | MISSING | MISSING | COMPLETE | `PollAdmin` орфанный целиком | `tst_poll_admin` |
| 14.8 | Отправка текущей и живой геолокации | `ComposerExtraPickers.kt:68-95`, `LocationSharing.kt:20-77` | — | MISSING | MISSING | MISSING | MISSING | `messageLocation` сводится к тексту «Location» | `TelegramTypes.cpp:277` |
| 14.9 | Venue, контакт и dice | `SpecialMessageViews.kt:625-665`, `DiceMessageView.kt`, `TdConversationActions.kt:358,419` | `TelegramTypes.cpp:274-286` (текст-заглушка) | MISSING | MISSING | MISSING | MISSING | Ни карточек, ни отправки, ни анимации | — |
| 14.10 | Открытие локации на карте | `SpecialMessageViews.kt:724-738` (geo: intent) | — | MISSING | MISSING | MISSING | MISSING | `openLink` карты не умеет | — |

### 15. Боты, мини-приложения, deep links и share

**Android в двух словах:** Меню команд бота, reply/inline-клавиатуры с callback, инлайн-режим, мини-приложения в отдельном WebView с JS-мостом; приём внешних ссылок tg:// и t.me, приём файлов через ACTION_SEND, экран выбора чата и Direct Share ярлыки.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 15.1 | Меню команд бота | `TdBotFeatures.kt:49-60` (GetUserFullInfo.botInfo), `BotCommandMenu.kt` | `BotCommands.cpp:10`, `BotCommandModel.*` | COMPLETE | MISSING | MISSING | COMPLETE | `GetUserFullInfo` для команд не отправляется; меню нет | grep — 0 |
| 15.2 | Reply/inline-клавиатуры и callback-кнопки | `TdBotKeyboards.kt:28-39` (GetCallbackQueryAnswer), `BotButtons.kt` | `BotMarkup.cpp:105,133` | COMPLETE | MISSING | MISSING | COMPLETE | `reply_markup` вообще не парсится при маппинге сообщения; callback не отправляется | `TelegramTypes.cpp` |
| 15.3 | Кнопка «Запустить бота» (/start) | `BotStartBar.kt:29-36` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 15.4 | Inline-запросы (`@bot query`) | `TdBotFeatures.kt:62-120` (GetInlineQueryResults, SendInlineQueryResultMessage) | `InlineQueryController.cpp:49-144` (только UI-состояние) | STUB | MISSING | MISSING | COMPLETE | Контроллер не строит запросы; `getInlineQueryResults` в `src/` отсутствует | — |
| 15.5 | Мини-приложения (Web App) | `TdBotFeatures.kt:271-338`, `BotWebAppScreen.kt:76`, `TelegramWebAppBridge.kt` | `WebAppInfo.cpp:193-239`, `WebAppSessionController.*` | PARTIAL | MISSING | MISSING | COMPLETE | URL-валидация есть; Qt WebEngine не линкуется | `CMakeLists.txt:33` |
| 15.6 | Превью ссылок в сообщении | `TdLinkPreview.kt:16-52`, `LinkPreviewCard.kt` | `LinkPreview.cpp:141`, `TelegramTypes.cpp:467`, `MessageBubble.qml:290` | PARTIAL | COMPLETE | PARTIAL | COMPLETE | Картинка превью отключена жёстко (`thumbnailReady: false`) | `MessageBubble.qml:298` |
| 15.7 | Deep links `tg://` и `t.me` | `AndroidManifest.xml:93-105`, `MainActivity.kt:414`, `TelegramDeepLink.kt:337` | `DeepLinks.cpp:665-718`, `DeepLinkRouter.cpp:103` | COMPLETE | MISSING | MISSING | COMPLETE | `main.cpp` не читает argv, роутер не создаётся; `tg:` уходит в `QDesktopServices`; схема не зарегистрирована | `AppController.cpp:1268` |
| 15.8 | Вступление по инвайт-ссылке | `TelegramDeepLink.kt:121,228` | `DeepLinks.cpp:696` (builder) | PARTIAL | MISSING | MISSING | COMPLETE | `joinChatByInviteLink` не отправляется | — |
| 15.9 | Приём share из других приложений и drag-and-drop | `AndroidManifest.xml:106-115`, `IncomingShare.kt`, `ShareSession.kt`, `ShareTargetScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | Нет `DropArea`, обработки аргументов и файловых ассоциаций | grep — 0 |
| 15.10 | Direct Share ярлыки чатов | `DirectShareShortcuts.kt:34-107` | — | MISSING | MISSING | MISSING | MISSING | Аналога jump list нет | — |

### 16. Секретные чаты и безопасность

**Android в двух словах:** Полный цикл секретных чатов (создание, состояние, TTL, закрытие, сверка ключа emoji/identicon, FLAG_SECURE); редактор правил приватности по девяти настройкам; список заблокированных; сквозной stealth-режим (online, typing, read receipts) с расписанием отправки.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 16.1 | Создание секретного чата | `TdSecretChats.kt:28-42`, `TdLibTelegramRepository.kt:1098` | — | MISSING | MISSING | MISSING | MISSING | `createNewSecretChat` в Windows отсутствует | — |
| 16.2 | Состояние секретного чата, TTL и закрытие | `TdSecretChats.kt:44-84` | `TelegramRepository.cpp:40,55,748,1300`, `SecretChatSafetyController.*` | PARTIAL | PARTIAL | PARTIAL | PARTIAL | `updateSecretChat` не обрабатывается: нет pending/ready/closed; TTL только клампится в локальном поле | `SecretChatSafetyController.cpp:36` |
| 16.3 | Сверка ключа (emoji, identicon, hex) | `SecretChatKeyEmoji.kt`, `SecretChatSettings.kt:236-268` | — | MISSING | MISSING | MISSING | MISSING | `key_hash` даже не парсится | — |
| 16.4 | Защита от скриншотов и view-once | `SecretChatPrivacy.kt:19-58` (FLAG_SECURE), `ViewOnceStealthGate.kt` | `SecretChatSafetyController.h:55-62`, `MediaSend.cpp:46` | STUB | MISSING | MISSING | PARTIAL | Только геттеры; `self_destruct_type` жёстко null; оконного флага нет | — |
| 16.5 | Правила приватности (9 настроек) | `TdPrivacy.kt:20-30,165-182`, `PrivacySettingsScreen.kt` | `PrivacyRequests.cpp:148-164`, `PrivacyRules.cpp:112-147`, `PrivacySettingsModel.*` | COMPLETE | MISSING | MISSING | COMPLETE | Билдеры и модели не вызываются; экрана нет | grep — 0 |
| 16.6 | Stealth: online, typing, read receipts | `TdConversationEffects.kt:48-70`, `TdLibTelegramRepository.kt:1541,1724`, `StealthSettingsStore.kt` | `PresenceRequests.cpp:9` (builder `setOption`), `TypingStatus.*` | STUB | MISSING | MISSING | COMPLETE | `setOnlineRequest` не вызывается; `sendTyping` и `viewMessages` уходят безусловно | `TelegramRepository.cpp:988,1125` |
| 16.7 | Stealth-расписание отправки | `TdStealthScheduler.kt:27-128`, `StealthPresets.kt`, `StealthSettingsScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | — | — |

### 17. Настройки и персонализация

**Android в двух словах:** Редактор оформления из 16 полей: пресеты Warm/Graphite/Material You, режимы FollowSystem/Light/Dark/AMOLED, пять дизайн-стилей, произвольные акцент и цвета пузырей, обои (глобально и на чат), плотность списков, анимации и motion, размещение навигации; отдельные экраны языка, хранилища, автозагрузки, видеосообщений и экспериментов.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 17.1 | Хранение настроек оформления (16 полей, per-account) | `AppearanceSettingsStore.kt:48-95`, `AppearanceConfig.kt:4-37` | `AppearanceStore.*` (7 полей, глобально) | PARTIAL | PARTIAL | MISSING | COMPLETE | `appearance.json` общий на все аккаунты; 5 из 7 полей никем не применяются | `AppearanceStore.cpp:261`; `main.cpp:54` |
| 17.2 | Пресеты палитры и яркость, включая AMOLED | `ThemePreset.kt`, `ThemeMode.kt`, `ui/theme/Theme.kt` | `ThemeController.cpp:110-176`, `SettingsPage.qml:56-99` | DIFFERENT | COMPLETE | PARTIAL | COMPLETE | Material You сознательно отклонён; Warm/Graphite × Light/Dark/AMOLED работают | `ThemeController.cpp:115` |
| 17.3 | Пять дизайн-стилей (Default, M2, M3, Liquid Glass, Signature) | `DesignStyle.kt`, `ui/design/*`, `AppearanceStyleScreen.kt` | `AppearanceStore.cpp:46-56` (только список строк) | STUB | MISSING | MISSING | MISSING | Стиль сохраняется, но `ThemeController` его не читает | — |
| 17.4 | Акцент, цвета пузырей и текста, радиусы, размер текста, интерлиньяж | `AppearanceConfig.kt:9-22`, `AppearanceColorsScreen.kt`, `AppearanceMessagesScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | Акцент задан пресетом; радиусы пузыря захардкожены | `MessageBubble.qml:118-121` |
| 17.5 | Обои: пресеты, интенсивность, глобально и на чат | `ChatWallpaper.kt`, `WallpaperPresets.kt`, `ChatOverridesStore.kt` | `AppearanceStore` (2 поля), `ChatBackgroundController.*`, `WallpaperPicker.qml` | STUB | MISSING | MISSING | COMPLETE | Пикер на 16 пресетов не инстанцируется; на чат обои не задаются | `qml/components/WallpaperPicker.qml` |
| 17.6 | Плотность списков | `ListDensity.kt`, `ChatListDensityTest.kt` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 17.7 | Анимации сообщений, motion и reduced motion | `ui/motion/*`, `MessageAnimation.kt`, `ChatAnimationSettingsStore.kt` | `AppearanceStore::reduceMotion` | STUB | MISSING | MISSING | COMPLETE | Флаг сохраняется, но QML-анимации безусловные | — |
| 17.8 | Размещение навигации и настройка вкладок | `NavigationPlacement.kt`, `NavigationSettingsScreen.kt`, `NavigationDock.kt` | — | MISSING | MISSING | MISSING | MISSING | Фиксированный рейл 268 px | `Shell.qml:26-28` |
| 17.9 | Язык интерфейса, перевод сообщений, замена текста | `AppLocale.kt`, `LanguageScreen.kt`, `TranslationSettingsStore.kt` | `LanguagePackModel.*`, `TranslationController.*`, `TextReplacementModel.*` | STUB | MISSING | MISSING | COMPLETE | Нет ни `.ts`, ни `QTranslator`; UI жёстко на английском | grep `.ts` — 0 |
| 17.10 | Клавиатурные сокращения | — (touch UI) | литералы `Shortcut{}` в QML, `ShortcutModel.*` | STUB | DIFFERENT | PARTIAL | COMPLETE | Alt+1..6, Ctrl+D, Ctrl+R работают; переназначения нет | `Shell.qml:12-17` |
| 17.11 | Доступность: контраст, имена, масштаб текста | `ui/accessibility/*`, `AccessibilitySupport.kt` | `ColorContrast.*` (живой), `AccessibilityModel.*` (мёртв), `Accessible.name` в 12 компонентах | PARTIAL | PARTIAL | PARTIAL | COMPLETE | WCAG-контраст реально используется; `textScale` 85–150 никем не применяется | `ColorContrast.cpp:34`; `ThemeController.cpp:146` |
| 17.12 | Экраны настроек: уведомления, хранилище, автозагрузка, видео, эксперименты | `SettingsScreen.kt`, `StorageSettingsScreen.kt`, `AutoDownloadSettingsScreen.kt`, `VideoMessageSettingsScreen.kt`, `ExperimentsScreen.kt` | только `SettingsPage.qml` (тема, профиль, 2 тумблера, About) | PARTIAL | PARTIAL | PARTIAL | COMPLETE | Экранов хранилища, автозагрузки, видео и экспериментов нет | `SettingsPage.qml:126-151` |

### 18. Сеть и прокси

**Android в двух словах:** Два экрана прокси (список сохранённых в TDLib и мастер добавления MTProto/SOCKS5/HTTP), замер TCP-задержки, подписка на серверы через встроенное ядро Mihomo (loopback SOCKS), автовыбор узла по состоянию сети, восстановление прокси после перезапуска, сетевая диагностика и per-network автодозагрузка.

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 18.1 | Список прокси, включение, выключение, удаление | `ProxyListScreen.kt:123-292`, `TdLibTelegramRepository.kt:257-280` | — | MISSING | MISSING | MISSING | MISSING | Нет `getProxies`/`enableProxy`/`disableProxy`/`removeProxy` | grep — 0 |
| 18.2 | Добавление прокси вручную (MTProto/SOCKS5/HTTP) | `ProxyConnectScreen.kt:135` | `ProxyConfigModel.*` (только форма) | STUB | MISSING | MISSING | COMPLETE | Модель не инстанцируется; `addProxy` вызывается один раз из конфига при старте | `TelegramSession.cpp:483` |
| 18.3 | Пинг и латентность прокси | `TdLibTelegramRepository.kt:297-307`, `ProxyListScreen.kt:100` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 18.4 | Подписка и ядро Mihomo (loopback SOCKS) | `VpnBackend.kt`, `SubscriptionClient.kt`, `MihomoCoreBridge.kt`, `MihomoProxyService.kt` | — | MISSING | MISSING | MISSING | MISSING | `third_party` содержит только bg-media; grep mihomo — 0 | — |
| 18.5 | Автовыбор узла и восстановление прокси | `TelegramAutoSelector.kt:29-75`, `ProxyRestoreDialog.kt` | — | MISSING | MISSING | MISSING | MISSING | После перезапуска без env прокси теряется | — |
| 18.6 | Состояние соединения в UI | `repository.connectionState`, `ChatsConnectionStatusTest` | `TelegramSession.cpp:50-61`, `AppController.cpp:519`, `Shell.qml:118` | COMPLETE | COMPLETE | COMPLETE | MISSING | Текст статуса есть в 4 местах; тестов нет | `connectionTitle` |
| 18.7 | Ретраи и оффлайн-политика | `ProxyRetry.kt`, retry в `VpnBackend.kt:164-205` | `ConnectionStatusController.*` (мёртв) | STUB | MISSING | MISSING | COMPLETE | `retry_after` из TDLib не читается | — |
| 18.8 | Сетевая диагностика (TCP до DC, публичный IP) | `ConnectionChecks.kt:31-155`, `ConnectionDiagnosticsScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 18.9 | Per-network автодокачка | `AutoDownloadNetwork.kt:19-126` | `AutoDownloadPolicy.*` (мёртв) | STUB | MISSING | MISSING | COMPLETE | Нет детекции сети, политика неисполнима | — |
| 18.10 | Прокси второго аккаунта (общий Mihomo) | `SecondaryProxySettingsScreen.kt:93` | — | MISSING | MISSING | MISSING | MISSING | Мультиаккаунта нет | — |

### 19. Диагностика, аналитика, обновления, экспорт и прочее

**Android в двух словах:** Структурный журнал событий с двухэшелонной редакцией; bug-report в бота с nonce и поллингом статуса; analytics opt-in; APK-апдейтер stable/dev; экспорт чата в JSON/HTML (+ZIP с медиа); локальный архив сообщений Chat Memory на SQLite; клиентская отложенная отправка с outbox и статусами; экран «Ресурсы».

| # | Функция | Android: поведение и файлы | Windows: файлы | L | I | U | T | Отличия и чего нет | Доказательства |
|---|---------|----------------------------|----------------|---|---|---|---|--------------------|----------------|
| 19.1 | Логирование с редакцией секретов | `StructuredEventLog.kt`, `ConnectionEventLog.kt`, `DiagnosticsRedactor.kt` | `core/Logging.*` (ring 200, 3×5 MiB), `core/Redaction.*` | COMPLETE | COMPLETE | COMPLETE | COMPLETE | Windows-редакция сильнее: tg://login, Bearer, ключи TDLib, hex api_hash | `Logging.cpp:67-100`; `tst_redaction` |
| 19.2 | «Copy diagnostics» и просмотр состояния | `ConnectionDiagnostics.kt:38`, `ConnectionChecks.kt` | `AppController.cpp:1354`, `EnvironmentPage.qml:63-90`, `LogsPage.qml` | PARTIAL | COMPLETE | COMPLETE | MISSING | Нет сети/VPN/ABI/батареи/мультиаккаунта; тестов нет | `AppController.cpp:1354` |
| 19.3 | Экспорт логов в файл или «поделиться» | `ConnectionDiagnostics.kt:63-74` (FileProvider) | `openLogFolder()` | PARTIAL | COMPLETE | PARTIAL | MISSING | Только открыть каталог и скопировать в буфер | `LogsPage.qml:44` |
| 19.4 | Bug-report в бота (форма, nonce, поллинг) | `DiagnosticsReportCoordinator.kt` (27 КБ), `ReportProblemScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | Нет HTTP-клиента вообще (`Qt6::Network` не линкуется) | grep — 0 |
| 19.5 | Analytics opt-in | `AnalyticsSync.kt:26`, `AnalyticsStore/API/Payload` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 19.6 | Обновление приложения | `StableUpdateRepository.kt:84-311`, `DevUpdateRepository.kt`, `StableUpdatesScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | Только строка версии в настройках | — |
| 19.7 | Экспорт чата (JSON/HTML, медиа в ZIP) | `ChatExportStreamer.kt` (27 КБ), `TdChatExporter.kt`, `ChatExportDialog.kt` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 19.8 | Chat Memory (локальный архив сообщений) | `ChatMemoryStore.kt` (21 КБ), `ChatMemoryRecorder.kt`, `ChatMemoryViews.kt` | — | MISSING | MISSING | MISSING | MISSING | — | — |
| 19.9 | Отложенная отправка (outbox со статусами) | `DelayedSendQueue.kt:44-230`, `DelayedSendStrip.kt`, `DelayedSendSettingsStore.kt` | `ScheduleSendController.*`, `MessageSendOptions.*` | STUB | MISSING | MISSING | COMPLETE | Мёртвые заготовки; `sendText` идёт без options | — |
| 19.10 | Экран «Хранилище» и очистка | `StorageSettingsScreen.kt:82`, `TdStorage.kt:30,170`, `StorageCleanupRules.kt:39` | `StorageStatsModel.*`, `CacheCleanupController.*` | STUB | MISSING | MISSING | COMPLETE | `getStorageStatistics` и `optimizeStorage` не отправляются | — |
| 19.11 | Экран «Ресурсы» (CPU/память) | `diagnostics/resources/*`, `ResourcesScreen.kt` | — | MISSING | MISSING | MISSING | MISSING | — | — |

### 20. Windows-only

Эти возможности отсутствуют на Android (или реализованы принципиально иначе). Они **не входят** в расчёт покрытия Android→Windows.

| # | Функция | Windows: реализация | L | I | U | T | Комментарий |
|---|---------|---------------------|---|---|---|---|-------------|
| W.1 | Регистрация нового аккаунта | `AuthPage.qml:483-551`, `AuthRequests.cpp:73`, `TelegramSession.cpp:320` | COMPLETE | COMPLETE | COMPLETE | COMPLETE | Android `AuthState` не знает такого состояния |
| W.2 | Email-адрес и email-код при входе | `AuthPage.qml:415-481`, `AuthRequests.cpp:57-71` | COMPLETE | COMPLETE | COMPLETE | PARTIAL | `setEmailAddressRequest` без теста |
| W.3 | Ввод api_id/api_hash и их DPAPI-хранение | `AuthPage.qml:150-238`, `CredentialStore.cpp:54-77` | COMPLETE | COMPLETE | COMPLETE | COMPLETE | Источники: environment, build, DPAPI |
| W.4 | Шифрование TDLib-БД и lock-файл аккаунта | `DatabaseKeyStore.cpp:57`, `SessionLock.cpp:14`, `TelegramSession.cpp:28` | COMPLETE | COMPLETE | COMPLETE | COMPLETE | Android использует пустой ключ БД |
| W.5 | Выбор страны, формат телефона, проверка username | `CountryCodeModel.*`, `PhoneNumberModel.*`, `UsernameCheckController.*` | COMPLETE | MISSING | MISSING | COMPLETE | Три модели-сироты, в UI не выведены |
| W.6 | Прокси из окружения или `proxy.txt` | `TelegramConfig.cpp:20-106`, `EnvironmentPage.qml:135-141` | COMPLETE | COMPLETE | PARTIAL | COMPLETE | Единственный способ задать прокси; только при старте |
| W.7 | История поисковых запросов (MRU) | `SearchHistoryStore.*` | COMPLETE | MISSING | MISSING | COMPLETE | На Android отсутствует; не персистится |
| W.8 | Локальный FSM звонка и модель группового звонка | `CallController.*`, `GroupCallModel.*` | PARTIAL | MISSING | MISSING | COMPLETE | Android VoIP не имеет; код недостижим |
| W.9 | Модель бустов чата | `BoostModel.*` | STUB | MISSING | MISSING | COMPLETE | Android имеет только метку Premium-фичи |
| W.10 | Поля банковской карты в памяти | `PaymentCardModel.*` | STUB | MISSING | MISSING | COMPLETE | Android никогда не вводит карту в приложении |
| W.11 | Сохранение геометрии окна | `WindowStateStore.*` | COMPLETE | COMPLETE | COMPLETE | COMPLETE | Десктопная возможность |
| W.12 | WCAG-контраст и `Accessible`-имена | `ColorContrast.*`, `Accessible.name` в 12 QML | COMPLETE | COMPLETE | COMPLETE | COMPLETE | Единственная осознанная a11y-база |

---

## Critical Parity Gaps

Отсортировано по влиянию на пользовательский сценарий, а не по объёму кода.

1. **Логика есть, проводки нет — и это главное.** 50 из 175 telegram-заголовков не включаются продакшн-кодом (до появления фасадов было 101). Это не отдельная функция, а системная причина, по которой стикеры, истории, платежи, приватность, сессии, контакты и группы «есть в коде, но не в приложении». Пока фасады не форвардят сигналы в репозиторий, любой новый домен останется мёртвым.
2. **Музыка отсутствует полностью.** Нет библиотеки, плейлистов, `.bgmusic`, очереди, эквалайзера, BG Mix/Beat и музыки профиля. Это самая большая продуктовая дыра (на Android — ~10 тыс. строк Compose + 160 КБ `PlaybackEngine`).
3. **Стикеры, GIF и эмодзи недоступны.** Нет панели, каталога, поиска, избранного и отправки; входящие стикеры рисуются статичной миниатюрой. Для мессенджера это ежедневный сценарий.
4. **Секретные чаты и приватность не существуют как функции.** Нет создания, TTL, ключевых emoji, `FLAG_SECURE`-аналога, правил приватности и stealth-режима; `updateSecretChat` не обрабатывается.
5. **Платежи, Premium, Stars и подарки — каркас.** Ни один запрос не отправляется, инвойсы и paid media не парсятся, платные реакции не ставятся.
6. **Контакты, группы и администрирование.** Нет экрана контактов, добавления/импорта, участников, promote/ban, инвайт-ссылок, заявок, создания групп и каналов, публичного поиска.
7. **Истории не отображаются и не публикуются.** Есть 4 билдера, парсер, две модели состояния и `StoriesFacade`, но `StoryViewer.qml` не смонтирован, ни один QML не читает `BeyGramApp.stories`, а `updateChatActiveStories` не обрабатывается.
8. **Уведомления недоделаны как система.** Нет клика по уведомлению, inline-reply, группировки, звука, badge, упоминаний, trigger-words и фоновой доставки при закрытом окне. Per-chat настройки затираются при mute.
9. **Форматирование текста не отправляется.** `entities` всегда пустой; жирный/курсив/код/ссылки из composer не поддерживаются, хотя приходящие entities рендерятся.
10. **Медиа-конвейер минимален.** Нет альбомов, сжатия, лимитов, камеры, видеозаметок, голосовой записи, прогресса загрузки, автодокачки, хранилища и shared media.
11. **Реакции не обновляются и не удаляются.** Нельзя поставить первую реакцию (`canReact: false`), снять свою, выбрать из каталога, поставить платную; входящие `updateMessage*Reactions` игнорируются.
12. **Поиск урезан.** Глобальный поиск — плоский список сообщений без секций и фильтров; переход к сообщению теряет `messageId`; подсветки нет; поиск по чатам локальный.
13. **Архив, папки и список чатов только читаются.** Нельзя закрепить, отметить непрочитанным, удалить/покинуть чат, управлять папками, догрузить список; pinned не отображается.
14. **Прокси и сеть.** Пользователь не может увидеть, добавить, выключить или удалить прокси; нет пинга, подписок, Mihomo, автовыбора и восстановления.
15. **Нет экспорта чата, Chat Memory, отложенной отправки, аналитики, bug-report и обновлений.** При этом в репозитории лежат мёртвые модели, создающие видимость реализации.

---

## Architecture Differences

| Аспект | Android | Windows | Влияние на перенос |
|---|---|---|---|
| Точка сборки UI | `MainActivity` + `BeyGramApp.kt` (~194 КБ) с ручным `AppRoute` и ~50 маршрутами | `Main.qml`, `Shell.qml` + 67 QML-файлов; 6 разделов, без стека экранов | Перенос экранов требует сначала ввести навигационный стек/роутер; сейчас новые экраны некуда монтировать |
| Контракт с UI | `domain/repository/TelegramRepository.kt` (23 КБ), расширяемый несколькими контрактами | Один `AppController` (113 Q_PROPERTY + 61 Q_INVOKABLE) как единственный мост QML→C++ | Любая новая функция требует правки `AppController`; это узкое место и причина «островов» |
| Реализация домена | `TdLibTelegramRepository.kt` (227 КБ) + 40+ `Td*.kt` хелперов | `TelegramRepository.cpp` (58 КБ) + 175 заголовков-компонентов | Android-хелперы вызываются из репозитория; Windows-компоненты в основном не вызываются никем |
| Модель сообщения | `ChatMessage.kt` (10 КБ): альбомы, forward, боты, special, реакции, paid, сервисные | `MessageData` (36 полей): текст, медиа, ответ, реакции, ссылка, опрос | `MessageData` физически не вмещает стикеры/альбомы/инвойсы/локации/read-state — расширение модели обязательно для большинства фич |
| Обновления TDLib | `TdAccountUpdateRouting` + точечные подписки; `UpdateMessageContent` перемапливает `special` | `TelegramUpdateScheduler` (батчинг/коалесинг) + один `processUpdate`; часть типов явно игнорируется | Игнорируются `updateFile`, `updateUserStatus`, `updateChatOnlineMemberCount`, `updateMessageContentOpened`; `updateChatReadOutbox`, `updateMessageReactions`, `updateSecretChat` не обрабатываются |
| Потоки | Kotlin coroutines/Flow, один репозиторий на аккаунт | GUI-поток + `TelegramParseWorker` в отдельном `QThread`; TDLib-клиент в своём цикле | Парсинг вынесен, но модели и QML живут в GUI-потоке; при больших апдейтах возможны пики |
| Состояние | DataStore/SharedPreferences per-account + TDLib | JSON-файлы (`appearance.json`, `notification-settings.json`, `drafts.json`) + TDLib | Настройки Windows глобальные, а не per-account; часть полей не применяется |
| Мультиаккаунт | Отдельный TDLib-клиент и БД на аккаунт, реестр и маршрутизация | Один `TelegramSession`, `AccountRegistry` — заготовка | Добавление второго аккаунта потребует переработать владение сессией и все синглтоны UI |
| Медиа | Media3/ExoPlayer, MediaCodec, CameraX, собственный importer | Qt Multimedia (`QMediaPlayer`), `QImageReader`, локальные файлы | Нет progressive streaming, камеры и записи; плеер намеренно не ходит в сеть |
| Платформа | FGS, notification channels, VpnService (loopback), Keystore, biometrics | Трей, DPAPI, `QLockFile`, без фонового процесса и VPN | Часть функций нужно перепроектировать, а не портировать |
| Тесты | 416 файлов, Robolectric/Compose, скриншот-голдены | 214 файлов, из них 182 собрано в exe, 3 217 кейсов; чистая логика | Windows-тесты не покрывают QML и сеть; зелёный прогон не доказывает UI |

---

## Stubs and Dead Code

### Масштаб

- **50 из 175** заголовков `src/telegram` не включаются ни одним другим файлом `src/` (проверено грепом `#include "…/X.h"` по всему `src/`; в начале аудита было 101). В списке: `StoryRequests`, `StickerSetModel`, `PaymentRequests`, `PremiumRequests`, `PrivacyRequests`, `ChatBlocking`, `ChatInviteLinkModel`, `ContactRequests`, `PollAdmin`, `PollResults`, `SearchFilters`, `MessageSendOptions`, `ScheduleSendController`, `NotificationRouter`, `NotificationGrouping`, `UnreadCounter`, `DraftSync`, `MediaDownloadQueue`, `MessageGroupingModel` и др. Часть доменов (сессии, приватность, сторис, платежи) теперь включена через `UiFacades`, но их сигналы не подключены к репозиторию.
- **36 QML-файлов** не инстанцируются ни одним другим QML: `SelectionToolbar.qml`, `AlbumGrid.qml`, `StickerPicker.qml`, `StickerPanel.qml`, `WallpaperPicker.qml`, `ProfileEditDialog.qml`, `ChatNotificationSettings.qml`, `ArchivePage.qml`, `StoryViewer.qml`, `PremiumCard.qml`, `PaymentSheet.qml`, `GiftPicker.qml`, `ContactsPanel.qml`, `SessionsPanel.qml`, `PrivacyPanel.qml`, `AppearancePanel.qml`, `CallOverlay.qml`, `VoiceRecorder.qml` и др. Большинство из них — свежие незакоммиченные панели, подготовленные под будущий монтаж.
- **Новый слой фасадов** (`src/ui/UiFacades` + 30 `*Facade`) на момент снимка **выставлен в `AppController`** (Q_PROPERTY `stickerPanel`, `sessions`, `stories`, `payments`, …), но остаётся инертным: `m_facades` используется только в геттерах (`AppController.cpp:954-983`), **ни один сигнал фасада не подключён** к репозиторию, и **ни один QML-файл не читает** эти свойства. Это подготовка UI-контракта, а не работающая функциональность.

### Что это значит

Большая часть «мёртвого» кода — не мусор, а качественно сделанные заготовки: контрактные комментарии сверены со схемой TDLib 1.8.69, есть валидация, клампы, тесты на граничные случаи. Проблема не в качестве, а в **отсутствии точки сборки**: нет слоя, который превращает builder в отправленный запрос и обновляет модель.

Типичные примеры:

| Компонент | Что делает | Почему мёртв |
|---|---|---|
| `StoryRequests` + `StoryViewerController` + `StoryProgressModel` | 4 билдера, парсер, state-машина, таймер | Нет экрана, нет вызовов, модели не связаны |
| `StickerCatalog` + `StickerSend` + `StickerPickerController` | каталог, установка, отправка, поиск | Нет панели и нет вызова из репозитория |
| `PaymentRequests`/`PremiumRequests`/`GiftRequests` | полные билдеры платежей | Ни один не отправляется; checkout возвращает `{}` |
| `PrivacyRules`/`PrivacyRequests` | парсер и билдеры правил | Нет экрана и нет вызова |
| `SessionListModel`/`SessionRequests`/`SessionTerminationController` | сессии и завершение | Есть фасад `sessions` в `AppController`, но нет форвардинга запросов и нет QML |
| `NotificationRouter`/`Grouping`/`Badge`/`Sound` | богаче реального пути | Реальный путь — отдельный `NotificationFormatter` |
| `CallController` | FSM звонка | TDLib не трогает, не инстанцируется |
| `MessageSendOptions`/`ScheduleSendController` | отложенная отправка | `sendText` идёт мимо options |
| `TextReplacementModel` | замена текста | Иная семантика, чем trigger words, и не вызывается |
| `AccessibilityModel` | масштаб текста 85–150 | `main.cpp` не вызывает `setFont` |

### Опасность для оценки зрелости

CI зелёный (3 217 кейсов), и это создаёт впечатление готовности. Но тесты проверяют **чистые функции**, а не достижимость. Например, `tst_reaction_catalog` (16 кейсов) и `tst_reaction_picker_model` (31 кейс) относятся к пикеру, которого пользователь не увидит. `tst_media_playlist_model` проверяет очередь, которую никто не создаёт. Это главный источник завышенных ожиданий от кодовой базы.

---

## Integration Risks

1. **Владение сессией и моделями.** `AppController` владеет `TelegramSession`, `TelegramRepository`, моделями и контроллерами. Любое подключение нового компонента требует решить, кто им владеет и как он переживает `rebuildSession()` (logout/retry создаёт новую сессию, `AppController.cpp:236`). Сейчас `rebuildSession` не пересоздаёт компоненты-«острова», поэтому наивная проводка приведёт к висячим указателям на старую сессию.
2. **Один аккаунт как неявный синглтон.** `AccountRegistry` не используется для `setActive`, а все модели глобальны. Добавление второго аккаунта без переработки владения приведёт к утечке данных между аккаунтами (чаты, черновики, настройки).
3. **Игнорируемые обновления.** `TelegramUpdateScheduler::isIgnored` отбрасывает `updateFile` — прогресс загрузки файлов не доходит до моделей; `updateUserStatus` — присутствие невозможно; `updateMessageContentOpened` — просмотры. `updateChatReadOutbox` коалесится, но не обрабатывается — галочек прочтения не будет даже после проводки. `updateMessageReactions`/`updateMessageInteractionInfo` не обрабатываются — реакции и опросы не обновляются живьём.
4. **Неполная модель данных.** `MessageData` не содержит `grouped_id`, read-state, forward-info, bot markup, invoice/paid media, location/contact/dice, sticker animation, album id. Подключение соответствующих компонентов без расширения модели приведёт к «работающей логике без отображения».
5. **Асинхронность и таймауты.** `TelegramRepository::sendRequest` использует таймауты и `jobId`; часть контроллеров (Story, Search) не имеют доступа к этому механизму и не умеют отменять запросы. При подключении необходим единый request-хэндл, иначе появятся гонки и «вечные» состояния загрузки.
6. **Коалесинг и порядок.** `TelegramUpdateScheduler` заменяет повторяющиеся `updateChatPosition`/`updateChatLastMessage` по ключу, но не сохраняет порядок между типами. При включении папок/архива это может дать неверный порядок чатов.
7. **Медиа-файлы.** `MediaFileStore` трекает только превью; полный файл грузится по клику. При подключении плеера/галереи нужно аккуратно совместить `trackFileId` и `updateFile` (который сейчас отброшен), иначе не будет прогресса и статуса «готово».
8. **Секреты в UI.** `diagnosticsText()` включает account id и полные пути; при появлении bug-report это нужно пропустить через редакцию (сейчас `Redaction` в логах сильнее, чем в diagnostics).
9. **Стили и темы.** `AppearanceStore` хранит `designStyle`/`bubbleStyle`, но `ThemeController` их не читает. Если подключить стили без миграции QML-констант (радиусы, цвета в `MessageBubble.qml`), получится «настройка, которая ничего не меняет».
10. **Платёжные данные.** `PaymentCardModel` держит номер карты и CVC в памяти. Пока нет отправки, это мёртвый груз; при подключении нельзя логировать и сохранять их в JSON.

---

## Recommended Implementation Order

Приоритеты учитывают зависимости: сначала точка сборки и модель данных, затем ежедневные сценарии, затем домены.

### P0 — без этого основные сценарии неполноценны

1. **Слой проводки и навигации.** Ввести QML-стек экранов и фасады, зарегистрированные в `AppController`; подключить уже существующие `*Facade` и `ChatListControls.qml`. Без этого остальное некуда монтировать. Зависимостей нет.
2. **Расширить `MessageData` и `TelegramTypes`.** Добавить read-state, forward-info, `grouped_id`, sticker/animation, invoice/paid media, location/contact/dice, service-типы. Это разблокирует 3.11, 3.12, 5.10, 7.11, 13.6, 14.8-14.10.
3. **Обработка недостающих обновлений.** `updateFile`, `updateChatReadOutbox`, `updateMessageReactions`, `updateMessageInteractionInfo`, `updateSecretChat`, `updateChatPhoto`, `updateUserStatus`. Разблокирует прогресс медиа, галочки, реакции, секретные чаты, аватары, присутствие.
4. **Действия списка чатов и папок.** Pin/unread/delete/leave, CRUD папок, догрузка списка, корректные folder-позиции и pinned-секция. Зависит от 1; использует готовые `ChatListActions`, `FolderEdit`.
5. **Полноценные уведомления.** Клик → чат, группировка, badge, звук, per-chat настройки, упоминания, фоновая работа (трей + `setQuitOnLastWindowClosed(false)`). Зависит от 1 и 3.

### P1 — существенные функциональные пробелы

6. **Стикеры, GIF и эмодзи.** Панель, каталог, установка, поиск, отправка, статичная отрисовка; TGS/WebM — отдельным решением. Зависит от 1.
7. **Форматирование текста и rich composer.** Генерация entities, markdown-хоткеи, предпросмотр. Зависит от 3.2 (сейчас только приём).
8. **Медиа-конвейер.** Альбомы, лимиты, сжатие/импорт, прогресс, авто-докачка, хранилище, shared media. Зависит от 2, 3, 4.
9. **Голосовые сообщения.** Запись через Qt Multimedia/Media Foundation, волна, отправка. Зависит от 2.
10. **Профили, контакты и группы.** Экран контактов, добавление/импорт, участники, promote/ban, инвайты, заявки, создание групп/каналов, публичный поиск. Зависит от 1 и 3.
11. **Поиск.** Секции global search, фильтры, подсветка, переход к сообщению (`MessageJumpController`), серверный поиск чатов. Зависит от 1 и 2.
12. **Реакции до конца.** Пикер из TDLib, удаление своей, первая реакция, живое обновление, лимит. Зависит от 3.
13. **Приватность и сессии.** Экран правил, блок-лист, активные сессии, stealth. Зависит от 1 и 3.
14. **Секретные чаты.** Создание, состояние, TTL, ключевые emoji, защита превью/скриншотов. Зависит от 3 и 13.

### P2 — дополнительные пользовательские возможности

15. **Истории.** Бар, вьюер, прогресс, просмотр, публикация, архив, приватность. Зависит от 1, 2, 8.
16. **Музыка.** Библиотека/плейлисты/`.bgmusic`, затем плеер и очередь, затем эквалайзер и BG Mix/Beat. Зависит от 2 и медиа-слоя; это отдельный крупный проект.
17. **Premium, Stars, подарки, платежи.** Только внешние HTTPS-ссылки; инвойсы и paid media после 2. Зависит от 1 и 2.
18. **Боты и мини-приложения.** Команды, клавиатуры, callback, inline, Web App (нужен WebEngine). Зависит от 2 и 1.
19. **Deep links, share и drag-and-drop.** Регистрация схемы, argv, single-instance, приём файлов. Зависит от 1.
20. **Прокси-менеджмент.** CRUD, пинг, восстановление; Mihomo/VPN — отдельное продуктовое решение. Зависит от 1.
21. **Экспорт чата, Chat Memory, отложенная отправка.** Зависит от 2 и 3.

### P3 — улучшения и необязательные отличия

22. Темы: пять дизайн-стилей, обои, плотность, motion, кастомизация цветов.
23. Локализация (`.ts`, `QTranslator`) и перевод сообщений.
24. Аналитика, bug-report, автообновление.
25. Экран ресурсов, расширенная диагностика.
26. Клавиатурные сокращения с переназначением, доступность (масштаб текста).

---

## Verification Plan

**Уже выполнено:** статический разбор обоих репозиториев; прогон 182 тестовых исполняемых файлов (3 217 кейсов, все прошли). Не выполнялось: запуск приложения, вход в аккаунт, сетевые сценарии, QML-тесты, Android-тесты.

### Обязательные runtime-проверки (P0)

| Сценарий | Что подтвердить | Критерий |
|---|---|---|
| Вход по телефону/коду/паролю | Полный цикл и восстановление после ошибок | Чат-лист появляется, сессия переживает перезапуск |
| QR-вход | Подтверждение с телефона, обновление QR, истечение токена | `authorizationStateReady`, нет зависшего BusyIndicator |
| Перезапуск и logout | `rebuildSession`, сохранность БД и ключа | После logout можно войти снова; данные не теряются |
| Открытие чата и история | Пагинация, отсутствие дублей, read receipts | Нет дублей; счётчик уменьшается; галочки (после доработки) |
| Отправка текста и файла | Медленная сеть, ошибка, повтор | Пузырь pending → sent; ошибка видна и повторяема |
| Уведомления | Свёрнутое окно, закрытое окно, mute, превью | Уведомление приходит; клик открывает чат (после доработки); mute соблюдается |
| Медиа | Превью, полный файл, видео/аудио, сохранение | Файл открывается, плеер играет, Save as работает |
| Архив/папки | Архивация, возврат, порядок и pin | Списки совпадают с Android |
| Реакции/опросы | Отправка, голос, обновление | Значения совпадают с сервером после перезапуска |

### Автоматические проверки, которые нужно добавить

1. **QML smoke-тесты** (Qt Quick Test): загрузка `Shell.qml` и каждого экрана, проверка свойств и сигналов. Сейчас UI не покрыт вообще.
2. **Интеграционные тесты с фейковым TDLib-транспортом**: подменить `TdJsonTransport` и проверить, что каждый домен отправляет ожидаемый `@type` и корректно применяет ответ/ошибку. Это поймает «острова» автоматически.
3. **Тест достижимости**: статический тест/скрипт, который проверяет, что каждый зарегистрированный компонент имеет хотя бы одного потребителя вне тестов (или явно помечен как заготовка). Сейчас 50 заголовков без потребителя и 36 несмонтированных QML.
4. **Тест обновлений**: для каждого обрабатываемого `update*` — кейс «пришло обновление → модель изменилась»; отдельно для `updateFile`, `updateChatReadOutbox`, `updateMessageReactions`.
5. **Тест модели данных**: сериализация/десериализация `MessageData` для всех поддерживаемых content type, включая новые поля.
6. **Контрактные тесты TDLib-схемы**: сверка `@type` и обязательных полей с pinned-версией (частично уже есть в комментариях, но не в тестах).
7. **Android-эталон**: прогнать 416 Android unit-тестов и зафиксировать поведение домена, которое Windows должен повторить (особенно merge истории, mute scope, poll updates, notification suppression).

### Ручные проверки перед релизом

- Сценарии с двумя устройствами (Android + Windows) на одном аккаунте: порядок, прочтение, черновики, mute, реакции.
- Проверка приватности: secret chat, protected content, скрытие превью, screenshot-политика.
- Проверка доступности: клавиатурная навигация, контраст, масштаб 125/150 %, экранный диктор.
- Проверка устойчивости: потеря сети, смена сети, спящий режим, второй экземпляр приложения (lock-файл).
- Проверка утечек: логи, `diagnosticsText`, платёжные поля, пути к файлам.

---

## Bottom line

Windows-клиент — это **не «почти готовый порт», а крепкий фундамент с большим количеством несмонтированных деталей**. Инфраструктура (TDLib JSON, DPAPI, revision-safe история, редакция логов, 3 217 зелёных тестов) сильнее, чем у большинства ранних портов. Но продуктовая поверхность — примерно пятая часть Android, а главная причина — отсутствие слоя интеграции, а не отсутствие кода.

Если подключить существующие компоненты, не написав ни одной новой бизнес-функции, можно закрыть значительную часть разрыва (стикеры, истории, сессии, приватность, платежи, звонки-история, поиск). Это делает следующий шаг одновременно дешёвым и рискованным: наивная проводка без расширения `MessageData`, обработки обновлений и владения сессией создаст иллюзию функциональности, которая рассыпется в runtime.

