# Дизайн-система BeyGram для Windows

Визуальный язык Windows-клиента: числа — из `qml/ds/design.js`, цвета — из `BeyGramTheme`
(`src/ui/ThemeController.h`), роли — из `qml/ds/DsTheme.qml`. Экран называет назначение, а не пиксель.

```qml
import "../ds" as Ds

Item {
    Ds.DsTheme { id: ds }
    Rectangle {
        color: ds.color.surface; radius: ds.radius.lg
        border.width: 1; border.color: ds.color.line
    }
}
```

`DsTheme` — обычный `QtObject`, не синглтон: один экземпляр на корень экрана, делегаты используют его `id`.

## 1. Принципы

**Спокойные поверхности.** `bg` — canvas, `surface` — карточки, `surface2` — вложенные и приподнятые
блоки. Иерархия строится шагом поверхности и линией `line`, а не набором разных цветов. На экране
не больше трёх уровней поверхностей.

**Один акцент.** `accent` — главное действие, выбранное состояние, непрочитанное и ссылки. Если на
экране три акцентные кнопки, акцента нет ни у одной. `accentSoft` — тональная подложка, `accentInk` —
текст на ней.

**Сетка 4 px.** Любой отступ, размер контрола и радиус кратны 4. Шаг 2 px допустим только для
оптической правки иконки или hairline. Вертикальный ритм задают `space.s2`/`s3`/`s4`, а не 7, 13 или 18.

**Радиусы 12–16.** Контейнеры — 12–16 px, крупные панели — до 20. Малые 6–10 остаются у компактных
контролов. Один тип поверхности имеет один радиус на всём экране.

**Без тяжёлых теней.** Глубина — шаг поверхности плюс граница `line`; `DropShadow` с blur > 12 и «парящие»
карточки запрещены. Для диалога достаточно `ds.alpha("#000000", ds.opacity.scrim)`.

## 2. Цвет

`DsTheme.color` повторяет `BeyGramTokens` из `Color.kt` и `ThemeController.h`; имена совпадают,
поэтому перенос с Android не требует переименований.

| Android (`BeyGramTokens`) | Windows (`BeyGramTheme` / `DsTheme`) | Назначение |
| --- | --- | --- |
| `bg` | `bg` / `color.bg` | фон окна, canvas |
| `surface` | `surface` / `color.surface` | карточки, панели, входящие пузыри |
| `surface2` | `surface2` / `color.surface2` | вложенные блоки, hover, поля ввода |
| `line` | `line` / `color.line` | hairline 1 px, границы, разделители |
| `text` | `text` / `color.text` | основной текст, заголовки, иконки |
| `muted` | `muted` / `color.muted` | вторичный текст, подписи, время |
| `accent` | `accent` / `color.accent` | primary-кнопка, selection, ссылки, unread |
| `accentSoft` | `accentSoft` / `color.accentSoft` | тональная подложка выбранного, чипы |
| `accentInk` | `accentInk` / `color.accentInk` | текст и иконки на `accentSoft` |
| `onAccent` | `onAccent` / `color.onAccent` | текст и иконки на `accent` |
| `bubbleOut` | `bubbleOut` / `color.bubbleOut` | исходящие сообщения |
| `ok` / `okInk` | `ok` / `okInk` (`color.ok` / `color.okInk`) | фон успеха / текст на нём |
| `warn` / `warnInk` | `warn` / `warnInk` (`color.warn` / `color.warnInk`) | фон предупреждения / текст на нём |
| `danger` | `danger` / `color.danger` | ошибки, удаление, destructive |

Правила:

- `danger` — и цвет текста/иконки, и заливка destructive-кнопки. Мягкая подложка —
  `ds.alpha(ds.color.danger, 0.16)`.
- Любая полупрозрачность — через `ds.alpha()`: так она работает в Light, Dark и AMOLED.
- `accent` на `bg` — не ниже 3:1; `text` на `surface` — не ниже 4.5:1.
- В AMOLED `bg` может быть чёрным: не «исправляйте» его серой подложкой. `line` не используется для
  текста, `bubbleOut` — вне чата, `ok`/`warn` — как текст.

```qml
color: ds.surfaceFor(1)                    // 0 canvas, 1 card, 2 raised, 3 overlay
color: ds.textFor(1)                       // 0 primary, 1 secondary, 2 disabled
color: ds.alpha(ds.color.accent, ds.opacity.hover)
```

`surfaceFor(3)` — нейтральный overlay: в тёмной теме светлее `surface2`, в светлой — темнее. Для меню
и диалогов предпочтительнее `surface` + `line`.

## 3. Типографика

| Токен | px | Межстрочный | Насыщенность | Назначение |
| --- | --- | --- | --- | --- |
| `font.caption` | 11 | `lineNormal` | Medium | overline, бейджи, время |
| `font.small` | 12 | `lineNormal` | Medium | метаданные, подписи |
| `font.body` | 14 | `lineNormal` | Regular | основной текст, сообщения |
| `font.bodyLarge` | 15 | `lineNormal` | Medium | акцентный body, крупная кнопка |
| `font.title` | 17 | `lineNormal` | DemiBold | заголовок карточки или секции |
| `font.headline` | 22 | `lineTight` | DemiBold | заголовок страницы |
| `font.display` | 28 | `lineTight` | Bold | hero, пустое состояние |

- Не более трёх размеров на экране; иерархия — размером и весом, не цветом. `caption` — только метки
  и время, не длинный текст; для него допустим `letterSpacing: 0.4`.
- Не фиксируйте высоту текстового блока: `implicitHeight` и множитель строки, иначе при масштабе
  текст обрежется. Заголовки не центрируются без причины; body выравнивается по левому краю.

## 4. Отступы и размеры

| Токен | px | Применение |
| --- | --- | --- |
| `space.s0` | 0 | сброс, вплотную |
| `space.s1` | 4 | иконка к тексту, hairline-зазор |
| `space.s2` | 8 | между контролами в строке |
| `space.s3` | 12 | padding компактной карточки |
| `space.s4` | 16 | стандартный padding, край экрана |
| `space.s5` | 20 | padding крупной карточки |
| `space.s6` | 24 | между секциями |
| `space.s7` | 28 | крупные блоки, производный шаг |
| `space.s8` | 32 | отступ пустого состояния |
| `space.s9` | 36 | крупные блоки, производный шаг |
| `space.s10` | 40 | максимум десктопного layout |

`design.js` не содержит шагов 7 и 9: в компонентах они не нужны. В `DsTheme` они выведены из сетки,
но предпочитайте `s4`, `s6`, `s8`. Контролы: `heightSmall` 32, `height` 40, `heightLarge` 48.
Минимальная цель — 32×32, комфортная — 40×40. Иконки 18/22, аватары 32/44/56/72.

## 5. Радиусы

| Токен | px | Применение |
| --- | --- | --- |
| `radius.xs` | 6 | мелкие бейджи, вложенные чипы |
| `radius.sm` | 10 | кнопки, поля ввода, компактные контролы |
| `radius.md` | 12 | небольшие панели, поповеры |
| `radius.lg` | 16 | карточки, основные контейнеры |
| `radius.xl` | 20 | шторки, диалоги, composer |
| `radius.pill` | 999 | чипы, переключатели, круглые кнопки |

Карточки — 16; существующие 18 — унаследованное исключение. Пузырь — 16 с хвостом 4, composer — 20.

## 6. Анатомия компонентов

**Кнопка.** Высота 40 (small 32, large 48), padding 16, радиус 10, иконка 18, зазор 8.
Primary — `accent` + `onAccent`, DemiBold. Secondary — прозрачный фон, граница `line`, hover
`ds.alpha(ds.color.surface2, 0.75)`, pressed `surface2`. Destructive — `danger` + `onAccent`; primary одна на экран.

**Карточка.** `surface`, граница 1 px `line`, радиус 16, padding 16–20, без тени. Заголовок 17
DemiBold, body 14, метаданные 12 `muted`. Вложенная карточка — `surface2`, не больше одного уровня.

**Поле ввода.** Высота 40, радиус 10, фон `surface2` или `surface`, граница `line`, placeholder
`muted`. Фокус — граница `accent` 2 px и кольцо `ds.alpha(ds.color.accent, 0.24)`. Ошибка — `danger`
плюс текст 12 px; только цветом ошибку не показывают.

**Строка списка.** Высота 72 (`chat.rowHeight`), padding 16, аватар 44–56, зазор 12. Заголовок 14–15
Medium, вторая строка 12–14 `muted`, время 11. Разделитель — `ds.alpha(ds.color.line, 0.6)` с отступом
под текст. Hover — `ds.alpha(ds.color.surface2, 0.6)`, selected — `accentSoft`; размер строки не меняется.

**Пузырь сообщения.** Входящий — `surface`, исходящий — `bubbleOut`, радиус 16, хвост 4, максимум 72%
(`chat.maxWidthRatio`), padding 12/10, текст 14/1.35, время 11 `muted`. Акцент — только галочки
прочтения и ссылки. Без тени и градиента.

**Диалог и шторка.** `surface`, радиус 20, padding 24, ширина до 480, затемнение
`ds.alpha("#000000", ds.opacity.scrim)`. Кнопки справа, зазор 8; destructive отделена. Esc и клик вне окна закрывают.

**Бейдж и пилюля.** Высота 20–22, радиус `pill`, текст 11 DemiBold, `letterSpacing: 0.4`. Тона:
`accentSoft`/`accentInk`, `ok`/`okInk`, `warn`/`warnInk`, `ds.alpha(ds.color.danger, 0.16)`/`danger`. Максимум «99+».

## 7. Состояния

| Состояние | Визуал | Длительность |
| --- | --- | --- |
| Hover | `alpha(surface2, 0.6–0.8)` или `alpha(text, 0.06)` | `motion.fast` 120 |
| Pressed | `surface2` полностью или темнее на 10–15% | 80–120 |
| Focus | кольцо `accent` 2 px с отступом 2 px | 120 |
| Disabled | `opacity.disabled` 0.45, без hover и нажатия | — |
| Selected | `accentSoft` + `accentInk`, без сдвига layout | 120 |
| Error | граница и текст `danger` + иконка/подпись | 120 |
| Loading | спиннер 16 px, ширина кнопки не меняется | — |

Focus-кольцо показывается при клавиатурном фокусе (`activeFocus` или `visualFocus` у Controls), а не при каждом
клике. Не заменяйте его сменой цвета текста: кольцо должно быть видно на любом фоне.

## 8. Движение

- `motion.fast` 120 мс — hover, pressed, focus, смена цвета.
- `motion.normal` 180 мс — появление панели, раскрытие секции, смена страницы.
- `motion.slow` 260 мс — шторки и диалоги.
- Кривые: `motion.easeOut` (OutCubic) — основная, `easeEmphasized` (OutQuint) — акцентная,
  `easeInOut` — обратимая.
- Анимируйте `opacity`, `color`, `scale`, `x`/`y`; не анимируйте высоту списка и layout без нужды.
- При системном уменьшении движения длительности ставьте в 0, оставляя мгновенную смену состояния.

## 9. Доступность

- Контраст body — не ниже 4.5:1, крупного текста и иконок — 3:1, focus-кольца — 3:1 к соседнему фону.
  `muted` — для вторичного текста, не для основного body на `surface2`.
- Минимальная цель — 32×32, для основных действий — 40×40; между целями не меньше 8 px.
- Focus-кольцо не отключается. Tab совпадает с визуальным порядком; Esc закрывает диалог, Enter
  активирует основное действие, Space переключает чекбокс.
- Цвет не единственный носитель смысла: ошибка — текст/иконка, выбранное — форма или галочка,
  непрочитанное — точка или счётчик.
- Заполняйте `Accessible.role`, `Accessible.name`, при необходимости `Accessible.description`;
  декоративные иконки не попадают в дерево доступности.
- Проверяйте 125% и 150%: текст не обрезается, кнопки растут по `implicitHeight`.

## 10. Соответствие Android → Windows

Цвета переносятся один в один (раздел 2); геометрия и типографика — семантически: Windows-контролы компактнее.

| Android | Windows | Примечание |
| --- | --- | --- |
| `DesignSpacing.xs` 4 | `space.s1` 4 | точное совпадение |
| `sm` 8 / `md` 12 | `space.s2` / `space.s3` | точное совпадение |
| `lg` 16 / `xl` 20 | `space.s4` / `space.s5` | точное совпадение |
| `xxl` 24 | `space.s6` 24 | точное совпадение |
| `screenPadding` 16 | `space.s4` | края окна 16 или 24 |
| `sectionGap` 18 | `space.s4`/`s5` | 16 или 20, не 18 |
| `DesignShapes.button` 16 | `radius.sm` 10 | Windows-кнопки компактнее |
| `card` 20 | `radius.lg` 16 | унаследованные 18 допустимы |
| `sheet`/`dialog` 28 | `radius.xl` 20 | крупные панели |
| `chip` 12 / `search` 24 | `radius.pill` | чипы и поиск полностью скруглены |
| `composer` 20 | `chat.composerRadius` 20 | точное совпадение |
| `bubbles.cornerRadius` 16 | `chat.bubbleRadius` 16 | хвост 4 |
| `listRowMinHeight` 64 | `chat.rowHeight` 72 | десктопная строка выше |
| `avatarSize` 54 | `control.avatarLarge` 56 | ближайший токен |
| иконка 24 | `control.iconLarge` 22 | иконки Windows мельче |
| `titleMedium` 17 | `font.title` 17 | точное совпадение |
| `bodyMedium` 14 | `font.body` 14 | точное совпадение |
| `bodySmall` 12 | `font.small` 12 | точное совпадение |
| `labelSmall` 11 | `font.caption` 11 | точное совпадение |
| `headlineMedium` 24 | `font.headline` 22 | шкала Windows плотнее |
| `displaySmall` 36 | `font.display` 28 | десктопный hero меньше |

## 11. Примеры: так и не так

- **Цвет.** Да: `ds.color.accent`, `ds.color.bubbleOut`, `ds.alpha(ds.color.danger, 0.16)`.
  Нет: `"#6A4FDC"`, `"#B42318"`, `Qt.rgba(...)` с придуманными каналами.
- **Отступы.** Да: `spacing: ds.space.s2`, `padding: ds.space.s4`. Нет: `spacing: 7`, `padding: 15`.
- **Радиусы.** Да: `radius: ds.radius.lg` для карточки, `ds.radius.sm` для кнопки.
  Нет: `radius: 18` у каждой второй поверхности и `radius: 4` у кнопки.
- **Текст.** Да: `color: ds.textFor(1)`, `font.pixelSize: ds.font.body`.
  Нет: `color: ds.color.text; opacity: 0.6` — непрозрачность ломает контраст и наследуется детьми.
- **Глубина.** Да: `surface` + `border.color: ds.color.line` + `surface2` для вложенного блока.
  Нет: `DropShadow { radius: 24; samples: 48 }` под каждой карточкой.
- **Состояния.** Да: hover — `alpha(surface2, 0.6)`, pressed — `surface2`, focus — кольцо `accent`.
  Нет: hover, меняющий акцент на другой оттенок, и «выбранная» кнопка без формы или галочки.
- **Движение.** Да: `Behavior on color { ColorAnimation { duration: ds.motion.fast } }`.
  Нет: `duration: 600` на каждом hover и анимация ширины списка.

## 12. Чек-лист перед сдачей экрана

- [ ] Все цвета из `DsTheme`, ни одного нового hex.
- [ ] Отступы кратны 4 px и используют `space.*`.
- [ ] Радиусы соответствуют типу поверхности, а не настроению.
- [ ] На экране один акцент и одна primary-кнопка.
- [ ] Нет теней тяжелее hairline + шага поверхности.
- [ ] Hover, pressed, focus и disabled видны и не двигают layout.
- [ ] Focus-кольцо работает с клавиатуры, Tab-порядок логичен.
- [ ] Цели нажатия не меньше 32×32, между ними не меньше 8 px.
- [ ] Текст не обрезается при 125% и 150%.
- [ ] Ошибки и статусы не передаются только цветом.
- [ ] Анимации используют `motion.*` и отключаются при reduced motion; `Accessible.name` заполнен.
