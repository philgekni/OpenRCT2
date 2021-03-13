/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Viewport.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Game.h>
#include <openrct2/actions/LargeSceneryRemoveAction.h>
#include <openrct2/actions/SignSetNameAction.h>
#include <openrct2/actions/SignSetStyleAction.h>
#include <openrct2/actions/WallRemoveAction.h>
#include <openrct2/config/Config.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/localisation/StringIds.h>
#include <openrct2/sprites.h>
#include <openrct2/world/Banner.h>
#include <openrct2/world/LargeScenery.h>
#include <openrct2/world/Scenery.h>
#include <openrct2/world/Wall.h>

static constexpr const rct_string_id WINDOW_TITLE = STR_SIGN;
static constexpr const int32_t WW = 113;
static constexpr const int32_t WH = 96;

// clang-format off
enum WINDOW_SIGN_WIDGET_IDX {
    WIDX_BACKGROUND,
    WIDX_TITLE,
    WIDX_CLOSE,
    WIDX_VIEWPORT,
    WIDX_SIGN_TEXT,
    WIDX_SIGN_DEMOLISH,
    WIDX_MAIN_COLOUR,
    WIDX_TEXT_COLOUR
};

// 0x9AEE00
static rct_widget window_sign_widgets[] = {
    WINDOW_SHIM(WINDOW_TITLE, WW, WH),
    MakeWidget({      3,      17}, {85, 60}, WindowWidgetType::Viewport,  WindowColour::Secondary, STR_VIEWPORT                                 ), // Viewport
    MakeWidget({WW - 25,      19}, {24, 24}, WindowWidgetType::FlatBtn,   WindowColour::Secondary, SPR_RENAME,   STR_CHANGE_SIGN_TEXT_TIP       ), // change sign button
    MakeWidget({WW - 25,      67}, {24, 24}, WindowWidgetType::FlatBtn,   WindowColour::Secondary, SPR_DEMOLISH, STR_DEMOLISH_SIGN_TIP          ), // demolish button
    MakeWidget({      5, WH - 16}, {12, 12}, WindowWidgetType::ColourBtn, WindowColour::Secondary, 0xFFFFFFFF,   STR_SELECT_MAIN_SIGN_COLOUR_TIP), // Main colour
    MakeWidget({     17, WH - 16}, {12, 12}, WindowWidgetType::ColourBtn, WindowColour::Secondary, 0xFFFFFFFF,   STR_SELECT_TEXT_COLOUR_TIP     ), // Text colour
    { WIDGETS_END },
};

static void window_sign_mouseup(rct_window *w, rct_widgetindex widgetIndex);
static void window_sign_mousedown(rct_window *w, rct_widgetindex widgetIndex, rct_widget* widget);
static void window_sign_dropdown(rct_window *w, rct_widgetindex widgetIndex, int32_t dropdownIndex);
static void window_sign_textinput(rct_window *w, rct_widgetindex widgetIndex, char *text);
static void window_sign_viewport_rotate(rct_window *w);
static void window_sign_invalidate(rct_window *w);
static void window_sign_paint(rct_window *w, rct_drawpixelinfo *dpi);


// 0x98E44C
static rct_window_event_list window_sign_events([](auto& events)
{
    events.mouse_up = &window_sign_mouseup;
    events.mouse_down = &window_sign_mousedown;
    events.dropdown = &window_sign_dropdown;
    events.text_input = &window_sign_textinput;
    events.viewport_rotate = &window_sign_viewport_rotate;
    events.invalidate = &window_sign_invalidate;
    events.paint = &window_sign_paint;
});

static void window_sign_small_mouseup(rct_window *w, rct_widgetindex widgetIndex);
static void window_sign_small_dropdown(rct_window *w, rct_widgetindex widgetIndex, int32_t dropdownIndex);
static void window_sign_small_invalidate(rct_window *w);

// 0x9A410C
static rct_window_event_list window_sign_small_events([](auto& events)
{
    events.mouse_up = &window_sign_small_mouseup;
    events.mouse_down = &window_sign_mousedown;
    events.dropdown = &window_sign_small_dropdown;
    events.text_input = &window_sign_textinput;
    events.viewport_rotate = &window_sign_viewport_rotate;
    events.invalidate = &window_sign_small_invalidate;
    events.paint = &window_sign_paint;
});
// clang-format on

static void window_sign_show_text_input(rct_window* w);

/**
 *
 *  rct2: 0x006BA305
 */
rct_window* window_sign_open(rct_windownumber number)
{
    rct_window* w;
    rct_widget* viewportWidget;

    // Check if window is already open
    w = window_bring_to_front_by_number(WC_BANNER, number);
    if (w != nullptr)
        return w;

    w = WindowCreateAutoPos(WW, WH, &window_sign_events, WC_BANNER, WF_NO_SCROLLING);
    w->widgets = window_sign_widgets;
    w->enabled_widgets = (1 << WIDX_CLOSE) | (1 << WIDX_SIGN_TEXT) | (1 << WIDX_SIGN_DEMOLISH) | (1 << WIDX_MAIN_COLOUR)
        | (1 << WIDX_TEXT_COLOUR);

    w->number = number;
    WindowInitScrollWidgets(w);

    auto banner = GetBanner(w->number);
    if (banner == nullptr)
        return nullptr;

    auto signViewPos = banner->position.ToCoordsXY().ToTileCentre();
    TileElement* tile_element = map_get_first_element_at(signViewPos);
    if (tile_element == nullptr)
        return nullptr;

    while (1)
    {
        if (tile_element->GetType() == TILE_ELEMENT_TYPE_LARGE_SCENERY)
        {
            rct_scenery_entry* scenery_entry = tile_element->AsLargeScenery()->GetEntry();
            if (scenery_entry != nullptr && scenery_entry->large_scenery.scrolling_mode != SCROLLING_MODE_NONE)
            {
                auto bannerIndex = tile_element->AsLargeScenery()->GetBannerIndex();

                if (bannerIndex == w->number)
                    break;
            }
        }
        tile_element++;
        if (tile_element >= &gTileElements[std::size(gTileElements)])
        {
            return nullptr;
        }
    }

    int32_t view_z = tile_element->GetBaseZ();
    w->frame_no = view_z;

    w->list_information_type = tile_element->AsLargeScenery()->GetPrimaryColour();
    w->var_492 = tile_element->AsLargeScenery()->GetSecondaryColour();
    w->SceneryEntry = tile_element->AsLargeScenery()->GetEntryIndex();

    // Create viewport
    viewportWidget = &window_sign_widgets[WIDX_VIEWPORT];
    viewport_create(
        w, w->windowPos + ScreenCoordsXY{ viewportWidget->left + 1, viewportWidget->top + 1 }, viewportWidget->width() - 1,
        viewportWidget->height() - 1, 0, { signViewPos, view_z }, 0, SPRITE_INDEX_NULL);

    w->viewport->flags = gConfigGeneral.always_show_gridlines ? VIEWPORT_FLAG_GRIDLINES : 0;
    w->Invalidate();

    return w;
}

/**
 *
 *  rct2: 0x6B9765
 */
static void window_sign_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    switch (widgetIndex)
    {
        case WIDX_CLOSE:
            window_close(w);
            break;
        case WIDX_SIGN_DEMOLISH:
        {
            auto banner = GetBanner(w->number);
            auto bannerCoords = banner->position.ToCoordsXY();
            auto tile_element = map_get_first_element_at(bannerCoords);
            if (tile_element == nullptr)
                return;
            while (1)
            {
                if (tile_element->GetType() == TILE_ELEMENT_TYPE_LARGE_SCENERY)
                {
                    rct_scenery_entry* scenery_entry = tile_element->AsLargeScenery()->GetEntry();
                    if (scenery_entry->large_scenery.scrolling_mode != SCROLLING_MODE_NONE)
                    {
                        auto bannerIndex = tile_element->AsLargeScenery()->GetBannerIndex();
                        if (bannerIndex == w->number)
                            break;
                    }
                }
                tile_element++;
                if (tile_element >= &gTileElements[std::size(gTileElements)])
                {
                    return;
                }
            }

            auto sceneryRemoveAction = LargeSceneryRemoveAction(
                { bannerCoords, tile_element->GetBaseZ(), tile_element->GetDirection() },
                tile_element->AsLargeScenery()->GetSequenceIndex());
            GameActions::Execute(&sceneryRemoveAction);
            break;
        }
        case WIDX_SIGN_TEXT:
            window_sign_show_text_input(w);
            break;
    }
}

/**
 *
 *  rct2: 0x6B9784
  & 0x6E6164 */
static void window_sign_mousedown(rct_window* w, rct_widgetindex widgetIndex, rct_widget* widget)
{
    switch (widgetIndex)
    {
        case WIDX_MAIN_COLOUR:
            WindowDropdownShowColour(w, widget, TRANSLUCENT(w->colours[1]), static_cast<uint8_t>(w->list_information_type));
            break;
        case WIDX_TEXT_COLOUR:
            WindowDropdownShowColour(w, widget, TRANSLUCENT(w->colours[1]), static_cast<uint8_t>(w->var_492));
            break;
    }
}

/**
 *
 *  rct2: 0x6B979C
 */
static void window_sign_dropdown(rct_window* w, rct_widgetindex widgetIndex, int32_t dropdownIndex)
{
    switch (widgetIndex)
    {
        case WIDX_MAIN_COLOUR:
        {
            if (dropdownIndex == -1)
                return;
            w->list_information_type = dropdownIndex;
            auto signSetStyleAction = SignSetStyleAction(w->number, dropdownIndex, w->var_492, true);
            GameActions::Execute(&signSetStyleAction);
            break;
        }
        case WIDX_TEXT_COLOUR:
        {
            if (dropdownIndex == -1)
                return;
            w->var_492 = dropdownIndex;
            auto signSetStyleAction = SignSetStyleAction(w->number, w->list_information_type, dropdownIndex, true);
            GameActions::Execute(&signSetStyleAction);
            break;
        }
        default:
            return;
    }

    w->Invalidate();
}

/**
 *
 *  rct2: 0x6B9791, 0x6E6171
 */
static void window_sign_textinput(rct_window* w, rct_widgetindex widgetIndex, char* text)
{
    if (widgetIndex == WIDX_SIGN_TEXT && text != nullptr)
    {
        auto signSetNameAction = SignSetNameAction(w->number, text);
        GameActions::Execute(&signSetNameAction);
    }
}

/**
 *
 *  rct2: 0x006B96F5
 */
static void window_sign_invalidate(rct_window* w)
{
    rct_widget* main_colour_btn = &window_sign_widgets[WIDX_MAIN_COLOUR];
    rct_widget* text_colour_btn = &window_sign_widgets[WIDX_TEXT_COLOUR];

    rct_scenery_entry* scenery_entry = get_large_scenery_entry(w->SceneryEntry);

    main_colour_btn->type = WindowWidgetType::Empty;
    text_colour_btn->type = WindowWidgetType::Empty;

    if (scenery_entry->large_scenery.flags & LARGE_SCENERY_FLAG_HAS_PRIMARY_COLOUR)
    {
        main_colour_btn->type = WindowWidgetType::ColourBtn;
    }
    if (scenery_entry->large_scenery.flags & LARGE_SCENERY_FLAG_HAS_SECONDARY_COLOUR)
    {
        text_colour_btn->type = WindowWidgetType::ColourBtn;
    }

    main_colour_btn->image = SPRITE_ID_PALETTE_COLOUR_1(w->list_information_type) | IMAGE_TYPE_TRANSPARENT | SPR_PALETTE_BTN;
    text_colour_btn->image = SPRITE_ID_PALETTE_COLOUR_1(w->var_492) | IMAGE_TYPE_TRANSPARENT | SPR_PALETTE_BTN;
}

/**
 *
 *  rct2: 0x006B9754, 0x006E6134
 */
static void window_sign_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    WindowDrawWidgets(w, dpi);

    // Draw viewport
    if (w->viewport != nullptr)
    {
        window_draw_viewport(dpi, w);
    }
}

/**
 *
 *  rct2: 0x6B9A6C, 0x6E6424
 */
static void window_sign_viewport_rotate(rct_window* w)
{
    w->RemoveViewport();

    auto banner = GetBanner(w->number);

    auto signViewPos = CoordsXYZ{ banner->position.ToCoordsXY().ToTileCentre(), w->frame_no };

    // Create viewport
    rct_widget* viewportWidget = &window_sign_widgets[WIDX_VIEWPORT];
    viewport_create(
        w, w->windowPos + ScreenCoordsXY{ viewportWidget->left + 1, viewportWidget->top + 1 }, viewportWidget->width() - 1,
        viewportWidget->height() - 1, 0, signViewPos, 0, SPRITE_INDEX_NULL);
    if (w->viewport != nullptr)
        w->viewport->flags = gConfigGeneral.always_show_gridlines ? VIEWPORT_FLAG_GRIDLINES : 0;
    w->Invalidate();
}

/**
 *
 *  rct2: 0x6E5F52
 */
rct_window* window_sign_small_open(rct_windownumber number)
{
    rct_window* w;
    rct_widget* viewportWidget;

    // Check if window is already open
    w = window_bring_to_front_by_number(WC_BANNER, number);
    if (w != nullptr)
        return w;

    w = WindowCreateAutoPos(WW, WH, &window_sign_small_events, WC_BANNER, 0);
    w->widgets = window_sign_widgets;
    w->enabled_widgets = (1 << WIDX_CLOSE) | (1 << WIDX_SIGN_TEXT) | (1 << WIDX_SIGN_DEMOLISH) | (1 << WIDX_MAIN_COLOUR)
        | (1 << WIDX_TEXT_COLOUR);

    w->number = number;
    WindowInitScrollWidgets(w);
    w->colours[0] = COLOUR_DARK_BROWN;
    w->colours[1] = COLOUR_DARK_BROWN;
    w->colours[2] = COLOUR_DARK_BROWN;

    auto banner = GetBanner(w->number);
    auto signViewPos = banner->position.ToCoordsXY().ToTileCentre();

    TileElement* tile_element = map_get_first_element_at(signViewPos);
    if (tile_element == nullptr)
        return nullptr;

    while (1)
    {
        if (tile_element->GetType() == TILE_ELEMENT_TYPE_WALL)
        {
            rct_scenery_entry* scenery_entry = tile_element->AsWall()->GetEntry();
            if (scenery_entry->wall.scrolling_mode != SCROLLING_MODE_NONE)
            {
                if (tile_element->AsWall()->GetBannerIndex() == w->number)
                    break;
            }
        }
        tile_element++;
    }

    int32_t view_z = tile_element->GetBaseZ();
    w->frame_no = view_z;

    w->list_information_type = tile_element->AsWall()->GetPrimaryColour();
    w->var_492 = tile_element->AsWall()->GetSecondaryColour();
    w->SceneryEntry = tile_element->AsWall()->GetEntryIndex();

    // Create viewport
    viewportWidget = &window_sign_widgets[WIDX_VIEWPORT];
    viewport_create(
        w, w->windowPos + ScreenCoordsXY{ viewportWidget->left + 1, viewportWidget->top + 1 }, viewportWidget->width() - 1,
        viewportWidget->height() - 1, 0, { signViewPos, view_z }, 0, SPRITE_INDEX_NULL);

    w->viewport->flags = gConfigGeneral.always_show_gridlines ? VIEWPORT_FLAG_GRIDLINES : 0;
    w->flags |= WF_NO_SCROLLING;
    w->Invalidate();

    return w;
}

/**
 *
 *  rct2: 0x6E6145
 */
static void window_sign_small_mouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    switch (widgetIndex)
    {
        case WIDX_CLOSE:
            window_close(w);
            break;
        case WIDX_SIGN_DEMOLISH:
        {
            auto banner = GetBanner(w->number);
            auto bannerCoords = banner->position.ToCoordsXY();
            auto tile_element = map_get_first_element_at(bannerCoords);
            if (tile_element == nullptr)
                return;
            while (true)
            {
                if (tile_element->GetType() == TILE_ELEMENT_TYPE_WALL)
                {
                    rct_scenery_entry* scenery_entry = tile_element->AsWall()->GetEntry();
                    if (scenery_entry->wall.scrolling_mode != SCROLLING_MODE_NONE)
                    {
                        if (tile_element->AsWall()->GetBannerIndex() == w->number)
                            break;
                    }
                }
                tile_element++;
            }
            CoordsXYZD wallLocation = { bannerCoords, tile_element->GetBaseZ(), tile_element->GetDirection() };
            auto wallRemoveAction = WallRemoveAction(wallLocation);
            GameActions::Execute(&wallRemoveAction);
            break;
        }
        case WIDX_SIGN_TEXT:
            window_sign_show_text_input(w);
            break;
    }
}

/**
 *
 *  rct2: 0x6E617C
 */
static void window_sign_small_dropdown(rct_window* w, rct_widgetindex widgetIndex, int32_t dropdownIndex)
{
    switch (widgetIndex)
    {
        case WIDX_MAIN_COLOUR:
        {
            if (dropdownIndex == -1)
                return;
            w->list_information_type = dropdownIndex;
            auto signSetStyleAction = SignSetStyleAction(w->number, dropdownIndex, w->var_492, false);
            GameActions::Execute(&signSetStyleAction);
            break;
        }
        case WIDX_TEXT_COLOUR:
        {
            if (dropdownIndex == -1)
                return;
            w->var_492 = dropdownIndex;
            auto signSetStyleAction = SignSetStyleAction(w->number, w->list_information_type, dropdownIndex, false);
            GameActions::Execute(&signSetStyleAction);
            break;
        }
        default:
            return;
    }

    w->Invalidate();
}

/**
 *
 *  rct2: 0x006E60D5
 */
static void window_sign_small_invalidate(rct_window* w)
{
    rct_widget* main_colour_btn = &window_sign_widgets[WIDX_MAIN_COLOUR];
    rct_widget* text_colour_btn = &window_sign_widgets[WIDX_TEXT_COLOUR];

    rct_scenery_entry* scenery_entry = get_wall_entry(w->SceneryEntry);

    main_colour_btn->type = WindowWidgetType::Empty;
    text_colour_btn->type = WindowWidgetType::Empty;

    if (scenery_entry->wall.flags & WALL_SCENERY_HAS_PRIMARY_COLOUR)
    {
        main_colour_btn->type = WindowWidgetType::ColourBtn;
    }
    if (scenery_entry->wall.flags & WALL_SCENERY_HAS_SECONDARY_COLOUR)
    {
        text_colour_btn->type = WindowWidgetType::ColourBtn;
    }

    main_colour_btn->image = SPRITE_ID_PALETTE_COLOUR_1(w->list_information_type) | IMAGE_TYPE_TRANSPARENT | SPR_PALETTE_BTN;
    text_colour_btn->image = SPRITE_ID_PALETTE_COLOUR_1(w->var_492) | IMAGE_TYPE_TRANSPARENT | SPR_PALETTE_BTN;
}

static void window_sign_show_text_input(rct_window* w)
{
    auto banner = GetBanner(w->number);
    if (banner != nullptr)
    {
        auto bannerText = banner->GetText();
        window_text_input_raw_open(w, WIDX_SIGN_TEXT, STR_SIGN_TEXT_TITLE, STR_SIGN_TEXT_PROMPT, bannerText.c_str(), 32);
    }
}
