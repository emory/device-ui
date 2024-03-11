#ifdef VIEW_320x240

#include "TFTView_320x240.h"
#include "Arduino.h"
#include "ViewController.h"
#include "ui.h" // this is the ui generated by lvgl / squareline editor
#include <algorithm>
#include <cstdio>
#include <functional>

#ifdef USE_X11
#include "X11Driver.h"
#elif defined(LGFX_DRIVER_INC)
#include "LGFXDriver.h"
#include LGFX_DRIVER_INC
#else
#error "Unknown device for view 320x240"
#endif

#define CR_REPLACEMENT 0x0C // dummy to record several lines in a one line textarea

// children index of nodepanel lv objects (see addNode)
enum NodePanelIdx { node_img_idx, node_btn_idx, node_lbl_idx, node_lbs_idx, node_bat_idx, node_lh_idx, node_sig_idx };

TFTView_320x240 *TFTView_320x240::gui = nullptr;

TFTView_320x240 *TFTView_320x240::instance(void)
{
    if (!gui)
        gui = new TFTView_320x240;
    return gui;
}

#ifdef USE_X11
TFTView_320x240::TFTView_320x240() : MeshtasticView(&X11Driver::create(), new ViewController) {}
#else
TFTView_320x240::TFTView_320x240() : MeshtasticView(new LGFXDriver<LGFX_DRIVER>(screenWidth, screenHeight), new ViewController) {}
#endif

void TFTView_320x240::init(IClientBase *client)
{
    Serial.println("TFTView init...");
    displaydriver->init();
    MeshtasticView::init(client);

    activeMsgContainer = ui_MessagesContainer;
    channel = {// TODO: channel is intended to store all channel data, not just the name(label)
               ui_ChannelLabel0, ui_ChannelLabel1, ui_ChannelLabel2, ui_ChannelLabel3,
               ui_ChannelLabel4, ui_ChannelLabel5, ui_ChannelLabel6, ui_ChannelLabel7};
    channelGroup = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    ui_set_active(ui_HomeButton, ui_HomePanel, ui_TopPanel);
    ui_events_init();

    // keyboard init
    lv_keyboard_set_textarea(ui_Keyboard, ui_MessageInputArea);
}

/**
 * @brief set active button, panel and top panel
 *
 * @param b button to set active
 * @param p main panel to set active
 * @param tp top panel to set active
 */
void TFTView_320x240::ui_set_active(lv_obj_t *b, lv_obj_t *p, lv_obj_t *tp)
{
    if (activeButton) {
        lv_obj_set_style_border_width(activeButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_recolor_opa(activeButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_set_style_border_width(b, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_img_recolor(b, lv_color_hex(0x67EA94), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(b, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (activePanel) {
        _ui_flag_modify(activePanel, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        if (activePanel == ui_MessagesPanel) {
            unreadMessages = 0; // TODO: not all messages are actually read
            updateUnreadMessages();
        }
    }

    _ui_flag_modify(p, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);

    if (tp) {
        if (activeTopPanel) {
            _ui_flag_modify(activeTopPanel, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        }
        _ui_flag_modify(tp, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        activeTopPanel = tp;
    }

    activeButton = b;
    activePanel = p;

    _ui_flag_modify(ui_Keyboard, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_MsgPopupPanel, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
}

void TFTView_320x240::ui_events_init(void)
{
    // just a test to implement callback via non-static lambda function
    auto ui_event_HomeButton = [](lv_event_t *e) {
        lv_event_code_t event_code = lv_event_get_code(e);
        if (event_code == LV_EVENT_CLICKED) {
            TFTView_320x240 &view = *static_cast<TFTView_320x240 *>(e->user_data);
            view.ui_set_active(ui_HomeButton, ui_HomePanel, ui_TopPanel);
        }
    };

    // main button events
    lv_obj_add_event_cb(ui_HomeButton, ui_event_HomeButton, LV_EVENT_ALL, this); // uses lambda above
    lv_obj_add_event_cb(ui_NodesButton, this->ui_event_NodesButton, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_GroupsButton, this->ui_event_GroupsButton, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_MessagesButton, this->ui_event_MessagesButton, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_MapButton, this->ui_event_MapButton, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_SettingsButton, this->ui_event_SettingsButton, LV_EVENT_ALL, NULL);

    // node and channel buttons
    lv_obj_add_event_cb(ui_NodeButton, ui_event_NodeButtonClicked, LV_EVENT_ALL, (void *)ownNode);

    // 8 channel buttons
    lv_obj_add_event_cb(ui_ChannelButton0, ui_event_ChannelButtonClicked, LV_EVENT_ALL, (void *)0);
    lv_obj_add_event_cb(ui_ChannelButton1, ui_event_ChannelButtonClicked, LV_EVENT_ALL, (void *)1);
    lv_obj_add_event_cb(ui_ChannelButton2, ui_event_ChannelButtonClicked, LV_EVENT_ALL, (void *)2);
    lv_obj_add_event_cb(ui_ChannelButton3, ui_event_ChannelButtonClicked, LV_EVENT_ALL, (void *)3);
    lv_obj_add_event_cb(ui_ChannelButton4, ui_event_ChannelButtonClicked, LV_EVENT_ALL, (void *)4);
    lv_obj_add_event_cb(ui_ChannelButton5, ui_event_ChannelButtonClicked, LV_EVENT_ALL, (void *)5);
    lv_obj_add_event_cb(ui_ChannelButton6, ui_event_ChannelButtonClicked, LV_EVENT_ALL, (void *)6);
    lv_obj_add_event_cb(ui_ChannelButton7, ui_event_ChannelButtonClicked, LV_EVENT_ALL, (void *)7);

    // new message popup
    lv_obj_add_event_cb(ui_MsgPopupButton, this->ui_event_MsgPopupButton, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_MsgPopupPanel, this->ui_event_MsgPopupButton, LV_EVENT_ALL, NULL);

    // keyboard
    lv_obj_add_event_cb(ui_Keyboard, ui_event_Keyboard, LV_EVENT_ALL, this);
}

#if 0 // defined above as lambda function for tests
void TDeckGUI::ui_event_HomeButton(lv_event_t * e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_CLICKED) {
        TDeckGUI::instance()->ui_set_active(ui_HomeButton, ui_HomePanel, ui_TopHomePanel);
    }
}
#endif

void TFTView_320x240::ui_event_NodesButton(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_CLICKED) {
        TFTView_320x240::instance()->ui_set_active(ui_NodesButton, ui_NodesPanel, ui_TopNodesPanel);
    }
}

void TFTView_320x240::ui_event_NodeButtonClicked(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_CLICKED) {
        //  set color and text of clicked node
        uint32_t nodeNum = (unsigned long)e->user_data;
        TFTView_320x240::instance()->showMessages(nodeNum);
    }
}

void TFTView_320x240::ui_event_GroupsButton(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_CLICKED) {
        TFTView_320x240::instance()->ui_set_active(ui_GroupsButton, ui_GroupsPanel, ui_TopGroupsPanel);
    }
}

void TFTView_320x240::ui_event_ChannelButtonClicked(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_CLICKED) {
        // set color and text of clicked group channel
        uint8_t ch = (uint8_t)(unsigned long)e->user_data;
        if (TFTView_320x240::instance()->channelGroup[ch]) {
            TFTView_320x240::instance()->showMessages(ch);
        } else {
            // TODO: click on unset channel should popup config screen
        }
    }
}

void TFTView_320x240::ui_event_MessagesButton(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_CLICKED) {
        // determine if last chat panel was node or group message panel
        // TODO: there is no easy way so far, so search through channelGroup
        uint8_t i;
        for (i = 0; i < c_max_channels; i++) {
            if (TFTView_320x240::instance()->channelGroup[i] == TFTView_320x240::instance()->activeMsgContainer)
                break;
        }
        if (i == c_max_channels)
            TFTView_320x240::instance()->ui_set_active(ui_MessagesButton, ui_MessagesPanel, ui_TopMessagePanel);
        else
            TFTView_320x240::instance()->ui_set_active(ui_MessagesButton, ui_MessagesPanel, ui_TopGroupChatPanel);
    }
}

void TFTView_320x240::ui_event_MapButton(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_CLICKED) {
        TFTView_320x240::instance()->ui_set_active(ui_MapButton, ui_MapPanel, ui_TopMapPanel);
    }
}

void TFTView_320x240::ui_event_SettingsButton(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_CLICKED) {
        TFTView_320x240::instance()->ui_set_active(ui_SettingsButton, ui_SettingsPanel, ui_TopSettingsPanel);
    }
}

/**
 * @brief hide msgPopupPanel on touch; goto message on button press
 *
 */
void TFTView_320x240::ui_event_MsgPopupButton(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    if (target == ui_MsgPopupPanel) {
        if (event_code == LV_EVENT_PRESSED) {
            TFTView_320x240::instance()->hideMessagePopup();
        }
    } else { // msg button was clicked
        if (event_code == LV_EVENT_CLICKED) {
            uint32_t channelOrNode = (unsigned long)ui_MsgPopupButton->user_data;
            if (channelOrNode < c_max_channels) {
                uint8_t ch = (uint8_t)channelOrNode;
                TFTView_320x240::instance()->showMessages(ch);
            } else {
                uint32_t nodeNum = channelOrNode;
                TFTView_320x240::instance()->showMessages(nodeNum);
            }
        }
    }
}

void TFTView_320x240::ui_event_Keyboard(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_CLICKED) {
        lv_obj_t *kb = lv_event_get_target(e);
        uint32_t btn_id = lv_keyboard_get_selected_btn(kb);

        switch (btn_id) {
        case 22: { // enter (filtered out by one-liner text input area, so we
                   // replace it)
            lv_obj_t *ta = lv_keyboard_get_textarea(kb);
            lv_textarea_add_char(ta, ' ');
            lv_textarea_add_char(ta, CR_REPLACEMENT);
            break;
        }
        case 35: { // keyboard
            lv_keyboard_set_popovers(ui_Keyboard, !lv_btnmatrix_get_popovers(kb));
            break;
        }
        case 36: { // left
            break;
        }
        case 38: { // right
            break;
        }
        case 39: { // checkmark
            lv_obj_t *ta = lv_keyboard_get_textarea(kb);
            char *txt = (char *)lv_textarea_get_text(ta);
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            if (ta == ui_MessageInputArea) {
                Serial.println(txt);
                TFTView_320x240::instance()->handleAddMessage(txt);
            }
            lv_textarea_set_text(ta, "");
            break;
        }
        default: {
            const char *txt = lv_keyboard_get_btn_text(kb, btn_id);
            Serial.print("btn id: ");
            Serial.print(btn_id);
            Serial.print(", btn: ");
            Serial.println(txt);
        }
        }
    }
}

void TFTView_320x240::handleAddMessage(char *msg)
{
    // retrieve nodeNum + channel from activeMsgContainer
    uint8_t ch;
    uint32_t to = UINT32_MAX;
    uint32_t channelOrNode = (unsigned long)activeMsgContainer->user_data;
    if (channelOrNode < c_max_channels) {
        ch = (uint8_t)channelOrNode;
    } else {
        ch = (uint8_t)(unsigned long)nodes[channelOrNode]->user_data;
        to = channelOrNode;
    }

    controller->sendText(to, ch, msg);
    addMessage(msg);
}

void TFTView_320x240::addMessage(char *msg)
{
    lv_obj_t *hiddenPanel = lv_obj_create(activeMsgContainer);
    lv_obj_set_width(hiddenPanel, lv_pct(100));
    lv_obj_set_height(hiddenPanel, LV_SIZE_CONTENT);
    lv_obj_set_align(hiddenPanel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(hiddenPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(hiddenPanel, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(hiddenPanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *textLabel = lv_label_create(hiddenPanel);
    // calculate expected size of text bubble, to make it look nicer
    lv_coord_t width = lv_txt_get_width(msg, strlen(msg), &lv_font_montserrat_12, 0, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(textLabel, max(min(width + 20, 200), 40));
    lv_obj_set_height(textLabel, LV_SIZE_CONTENT);
    lv_obj_set_y(textLabel, 0);
    lv_obj_set_align(textLabel, LV_ALIGN_RIGHT_MID);

    // tweak to allow multiple lines in single line text area
    for (int i = 0; i < strlen(msg); i++)
        if (msg[i] == CR_REPLACEMENT)
            msg[i] = '\n';
    lv_label_set_text(textLabel, msg);
    lv_obj_set_style_text_color(textLabel, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_text_opa(textLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(textLabel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(textLabel, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(textLabel, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(textLabel, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(textLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(textLabel, lv_color_hex(0x67EA94), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(textLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(textLabel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(textLabel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(textLabel, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(textLabel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(textLabel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_scroll_to_view(hiddenPanel, LV_ANIM_ON);
}

void TFTView_320x240::addNode(uint32_t nodeNum, uint8_t ch, const char *userShort, const char *userLong, uint32_t lastHeard,
                              eRole role)
{
    // lv_obj nodePanel children
    // [0]: img
    // [1]: btn
    // [2]: lbl user long
    // [3]: lbl user short
    // [4]: lbl battery
    // [5]: lbl lastHeard
    // [6]: lbl signal
    // user_data: ch

    lv_obj_t *p = lv_obj_create(ui_NodesPanel);
    p->user_data = (void *)(uint32_t)ch;
    nodes[nodeNum] = p;
    nodeCount++;

    lv_obj_set_height(p, 50);
    lv_obj_set_width(p, lv_pct(100));
    lv_obj_set_align(p, LV_ALIGN_CENTER);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                             LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE |
                             LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM); /// Flags
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(p, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(p, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(p, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(p, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(p, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *img = lv_img_create(p);
    setNodeImage(nodeNum, role, img);

    lv_obj_set_width(img, LV_SIZE_CONTENT);
    lv_obj_set_height(img, LV_SIZE_CONTENT);
    lv_obj_set_x(img, -5);
    lv_obj_set_y(img, -10);
    // lv_obj_set_align( img, LV_ALIGN_LEFT_MID );
    lv_obj_add_flag(img, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ui_NodeButton = lv_btn_create(p);
    lv_obj_set_height(ui_NodeButton, 50);
    lv_obj_set_width(ui_NodeButton, lv_pct(50));
    lv_obj_set_x(ui_NodeButton, -13);
    lv_obj_set_y(ui_NodeButton, 0);
    lv_obj_set_align(ui_NodeButton, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_NodeButton, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_style_bg_color(ui_NodeButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_NodeButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_NodeButton, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_NodeButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_NodeButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_NodeButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui_NodeButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ln_lbl = lv_label_create(p);
    lv_obj_set_width(ln_lbl, LV_SIZE_CONTENT);
    lv_obj_set_height(ln_lbl, LV_SIZE_CONTENT);
    lv_obj_set_x(ln_lbl, -6);
    lv_obj_set_y(ln_lbl, 12);
    lv_obj_set_align(ln_lbl, LV_ALIGN_BOTTOM_LEFT);
    lv_label_set_text(ln_lbl, userLong);
    lv_label_set_long_mode(ln_lbl, LV_LABEL_LONG_DOT);
    ln_lbl->user_data = (void *)strlen(userLong);
    lv_obj_set_style_text_color(ln_lbl, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ln_lbl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ln_lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *sn_lbl = lv_label_create(p);
    lv_obj_set_width(sn_lbl, LV_SIZE_CONTENT);
    lv_obj_set_height(sn_lbl, LV_SIZE_CONTENT);
    lv_obj_set_x(sn_lbl, 25);
    lv_obj_set_y(sn_lbl, -3);
    lv_obj_set_align(sn_lbl, LV_ALIGN_LEFT_MID);
    lv_label_set_text(sn_lbl, userShort);
    lv_obj_set_style_text_color(sn_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(sn_lbl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ui_BatteryLabel = lv_label_create(p);
    lv_obj_set_width(ui_BatteryLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_BatteryLabel, LV_SIZE_CONTENT);
    lv_obj_set_y(ui_BatteryLabel, -15);
    lv_obj_set_x(ui_BatteryLabel, 8);
    lv_obj_set_align(ui_BatteryLabel, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_BatteryLabel, "");
    lv_obj_set_style_text_color(ui_BatteryLabel, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_BatteryLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_BatteryLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_BatteryLabel, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ui_lastHeardLabel = lv_label_create(p);
    lv_obj_set_width(ui_lastHeardLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lastHeardLabel, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lastHeardLabel, LV_ALIGN_RIGHT_MID);
    lv_obj_set_x(ui_lastHeardLabel, 8);
    lv_obj_set_y(ui_lastHeardLabel, 0);

    // TODO: devices without actual time will report all nodes as lastseen = now
    if (lastHeard) {
        time_t curtime;
        time(&curtime);
        lastHeard = min(curtime, (time_t)lastHeard); // adapt values too large

        char buf[12];
        bool isOnline = lastHeartToString(lastHeard, buf);
        lv_label_set_text(ui_lastHeardLabel, buf);
        if (isOnline) {
            nodesOnline++;
        }
    } else {
        lv_label_set_text(ui_lastHeardLabel, "");
    }

    lv_obj_set_style_text_color(ui_lastHeardLabel, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lastHeardLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lastHeardLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lastHeardLabel, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_lastHeardLabel->user_data = (void *)lastHeard;

    lv_obj_t *ui_SignalLabel = lv_label_create(p);
    lv_obj_set_width(ui_SignalLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_SignalLabel, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_SignalLabel, 8);
    lv_obj_set_y(ui_SignalLabel, 12);
    lv_obj_set_align(ui_SignalLabel, LV_ALIGN_BOTTOM_RIGHT);
    lv_label_set_text(ui_SignalLabel, "");
    lv_obj_set_style_text_color(ui_SignalLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_SignalLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_SignalLabel, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_NodeButton, ui_event_NodeButtonClicked, LV_EVENT_ALL, (void *)nodeNum);

    // move node into new position within nodePanal
    if (lastHeard) {
        lv_obj_t **children = ui_NodesPanel->spec_attr->children;
        int i = ui_NodesPanel->spec_attr->child_cnt - 1;
        while (i > 1) {
            if (lastHeard <= (time_t)(children[i - 1]->LV_OBJ_IDX(node_lh_idx)->user_data))
                break;
            i--;
        }
        if (i >= 1 && i < ui_NodesPanel->spec_attr->child_cnt - 1) {
            lv_obj_move_to_index(p, i);
        }
    }

    updateNodesOnline("%d of %d nodes online");
}

void TFTView_320x240::setMyInfo(uint32_t nodeNum)
{
    ownNode = nodeNum;
    nodes[ownNode] = ui_NodePanel;
}

void TFTView_320x240::setDeviceMetaData(int hw_model, const char *version, bool has_bluetooth, bool has_wifi, bool has_eth,
                                        bool can_shutdown)
{
}

void TFTView_320x240::addOrUpdateNode(uint32_t nodeNum, uint8_t ch, const char *userShort, const char *userLong,
                                      uint32_t lastHeard, eRole role)
{
    if (nodes.find(nodeNum) == nodes.end()) {
        addNode(nodeNum, ch, userShort, userLong, lastHeard, role);
    } else {
        updateNode(nodeNum, ch, userShort, userLong, lastHeard, role);
    }
}

/**
 * @brief update node userName and image
 *
 * @param nodeNum
 * @param ch
 * @param userShort
 * @param userLong
 * @param lastHeard
 * @param role
 */
void TFTView_320x240::updateNode(uint32_t nodeNum, uint8_t ch, const char *userShort, const char *userLong, uint32_t lastHeard,
                                 eRole role)
{
    auto it = nodes.find(nodeNum);
    if (it != nodes.end()) {
        lv_label_set_text(it->second->LV_OBJ_IDX(node_lbl_idx), userLong);
        it->second->LV_OBJ_IDX(node_lbl_idx)->user_data = (void *)strlen(userLong);
        lv_label_set_text(it->second->LV_OBJ_IDX(node_lbs_idx), userShort);
        setNodeImage(nodeNum, role, it->second->LV_OBJ_IDX(node_img_idx));
    }
}

void TFTView_320x240::updatePosition(uint32_t nodeNum, int32_t lat, int32_t lon, int32_t alt, uint32_t precision)
{
    auto it = nodes.find(nodeNum);
    if (it != nodes.end()) {
        // TODO
    }
}

void TFTView_320x240::updateMetrics(uint32_t nodeNum, uint32_t bat_level, float voltage, float chUtil, float airUtil)
{
    auto it = nodes.find(nodeNum);
    if (it != nodes.end()) {
        char buf[32];
        if (it->first == ownNode) {
            sprintf(buf, "Util %0.1f%% %0.1f%%", chUtil, airUtil);
            lv_label_set_text(it->second->LV_OBJ_IDX(node_sig_idx), buf);
        }

        if (bat_level != 0 || voltage != 0) {
            bat_level = min(bat_level, (uint32_t)100);
            sprintf(buf, "%d%% %0.2fV", bat_level, voltage);
            lv_label_set_text(it->second->LV_OBJ_IDX(node_bat_idx), buf);
        }
    }
}

void TFTView_320x240::updateSignalStrength(uint32_t nodeNum, int32_t rssi, float snr)
{
    if (nodeNum != ownNode) {
        auto it = nodes.find(nodeNum);
        if (it != nodes.end()) {
            char buf[30];
            if (rssi == 0.0 && snr == 0.0) {
                buf[0] = '\0';
            } else {
                // if userNameLong is too long, skip printing rssi/snr
                if ((size_t)it->second->LV_OBJ_IDX(node_lbl_idx)->user_data <= 20) {
                    sprintf(buf, "rssi: %d snr: %.1f", rssi, snr);
                } else {
                    sprintf(buf, "snr: %.1f", snr);
                }
            }
            lv_label_set_text(it->second->LV_OBJ_IDX(node_sig_idx), buf);
        }
    }
}

void TFTView_320x240::packetReceived(uint32_t from, const meshtastic_MeshPacket &p)
{
    MeshtasticView::packetReceived(from, p);
}

void TFTView_320x240::updateChannelConfig(uint32_t index, const char *name, const uint8_t *psk, uint32_t psk_size, uint8_t role)
{
    if (strlen(name)) {
        lv_label_set_text(channel[index], name);
        newMessageContainer(0, UINT32_MAX, index);
    }
}

/**
 * @brief Create a new container for a node or group channel
 *
 * @param from
 * @param to: UINT32_MAX for broadcast, ownNode (= us) otherwise
 * @param channel
 */
lv_obj_t *TFTView_320x240::newMessageContainer(uint32_t from, uint32_t to, uint8_t ch)
{
    // create container for new messages
    lv_obj_t *container = lv_obj_create(ui_MessagesPanel);
    lv_obj_remove_style_all(container);
    lv_obj_set_width(container, lv_pct(110));
    lv_obj_set_height(container, lv_pct(100));
    lv_obj_set_x(container, 0);
    lv_obj_set_y(container, -10);
    lv_obj_set_align(container, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(container,
                      LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE |
                          LV_OBJ_FLAG_SCROLL_ELASTIC); /// Flags
    lv_obj_set_style_pad_row(container, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // store new message container
    if (to == UINT32_MAX || from == 0) {
        channelGroup[ch] = container;
    } else {
        messages[from] = container;
    }

    return container;
}

/**
 * @brief insert a mew message that arrived into a <channel group> or <from node> container
 *
 * @param nodeNum
 * @param ch
 * @param msg
 */
void TFTView_320x240::newMessage(uint32_t from, uint32_t to, uint8_t ch, const char *msg)
{
    // if there's a message from a node we don't know (yet), create it with defaults
    auto it = nodes.find(from);
    if (it == nodes.end()) {
        MeshtasticView::addOrUpdateNode(from, ch, 0, eRole::client);
        updateLastHeard(from);
    }

    char buf[280];
    char *message = (char *)msg;
    lv_obj_t *container = nullptr;
    if (to == UINT32_MAX) { // message for group, prepend short name to msg
        sprintf(buf, "%s:\n%s", lv_label_get_text(nodes[from]->LV_OBJ_IDX(node_lbs_idx)), msg);
        message = buf;
        container = channelGroup[ch];
    } else { // message for us
        container = messages[from];
    }

    // if it's the first message we need a container
    if (!container) {
        container = newMessageContainer(from, to, ch);
    }

    // place new message into container
    newMessage(from, container, ch, message);

    // display msg popup if not already viewing the messages
    if (container != activeMsgContainer || activePanel != ui_MessagesPanel) {
        unreadMessages++;
        updateUnreadMessages();
        if (activePanel != ui_MessagesPanel) {
            showMessagePopup(from, to, ch, lv_label_get_text(nodes[from]->LV_OBJ_IDX(node_lbl_idx)));
        }
        _ui_flag_modify(container, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
}

/**
 * @brief Display message bubble in related message container
 *
 * @param nodeNum
 * @param container
 * @param ch
 * @param msg
 */
void TFTView_320x240::newMessage(uint32_t nodeNum, lv_obj_t *container, uint8_t ch, const char *msg)
{
    lv_obj_t *hiddenPanel = lv_obj_create(container);
    lv_obj_set_width(hiddenPanel, lv_pct(100));
    lv_obj_set_height(hiddenPanel, LV_SIZE_CONTENT); /// 50
    lv_obj_set_align(hiddenPanel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(hiddenPanel, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_radius(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(hiddenPanel, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(hiddenPanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(hiddenPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *msgLabel = lv_label_create(hiddenPanel);
    // calculate expected size of text bubble, to make it look nicer
    lv_coord_t width = lv_txt_get_width(msg, strlen(msg), &lv_font_montserrat_12, 0, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(msgLabel, max(min(width + 20, 200), 40));
    lv_obj_set_height(msgLabel, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(msgLabel, LV_ALIGN_LEFT_MID);
    lv_label_set_text(msgLabel, msg);
    lv_obj_set_style_text_color(msgLabel, lv_color_hex(0xD0D0D0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(msgLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(msgLabel, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(msgLabel, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(msgLabel, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(msgLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(msgLabel, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(msgLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(msgLabel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(msgLabel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(msgLabel, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(msgLabel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(msgLabel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_scroll_to_view(hiddenPanel, LV_ANIM_ON);
}

/**
 * @brief display new message popup panel
 *
 * @param from sender (NULL for removing popup)
 * @param to individual or group message
 * @param ch received channel
 */
void TFTView_320x240::showMessagePopup(uint32_t from, uint32_t to, uint8_t ch, const char *name)
{
    if (name) {
        static char buf[64];
        sprintf(buf, "New message from \n%s", name);
        buf[38] = '\0'; // cut too long userName
        lv_label_set_text(ui_MsgPopupLabel, buf);
        if (to == UINT32_MAX)
            ui_MsgPopupButton->user_data = (void *)(uint32_t)ch; // store the channel in the button's data
        else
            ui_MsgPopupButton->user_data = (void *)from; // store the node in the button's data
        _ui_flag_modify(ui_MsgPopupPanel, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
}

void TFTView_320x240::hideMessagePopup(void)
{
    _ui_flag_modify(ui_MsgPopupPanel, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
}

/**
 * @brief Display messages of a group channel
 *
 * @param ch
 */
void TFTView_320x240::showMessages(uint8_t ch)
{
    _ui_flag_modify(activeMsgContainer, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    activeMsgContainer = channelGroup[ch];
    activeMsgContainer->user_data = (void *)(uint32_t)ch;
    _ui_flag_modify(activeMsgContainer, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    lv_label_set_text(ui_TopGroupChatLabel, lv_label_get_text(channel[ch]));
    ui_set_active(ui_MessagesButton, ui_MessagesPanel, ui_TopGroupChatPanel);
}

/**
 * @brief Display messages from a node
 *
 * @param nodeNum
 */
void TFTView_320x240::showMessages(uint32_t nodeNum)
{
    _ui_flag_modify(activeMsgContainer, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    activeMsgContainer = messages[nodeNum];
    if (!activeMsgContainer) {
        activeMsgContainer = newMessageContainer(nodeNum, 0, 0);
    }
    activeMsgContainer->user_data = (void *)(uint32_t)nodeNum;
    _ui_flag_modify(activeMsgContainer, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    lv_obj_t *p = nodes[nodeNum];
    if (p) {
        lv_label_set_text(ui_TopNodeLabel, lv_label_get_text(p->LV_OBJ_IDX(node_lbl_idx)));
        ui_set_active(ui_MessagesButton, ui_MessagesPanel, ui_TopMessagePanel);
    } else {
        // TODO: log error
    }
}

// -------- helpers --------

void TFTView_320x240::removeNode(uint32_t nodeNum)
{
    auto it = nodes.find(nodeNum);
    if (it != nodes.end()) {
    }
}

void TFTView_320x240::setNodeImage(uint32_t nodeNum, eRole role, lv_obj_t *img)
{
    uint32_t color = nodeColor(nodeNum);
    switch (role) {
    case client:
    case client_mute:
    case client_hidden:
    case tak: {
        lv_img_set_src(img, &ui_img_2104440450);
        break;
    }
    case router_client: {
        lv_img_set_src(img, &ui_img_2095618903);
        break;
    }
    case repeater:
    case router: {
        lv_img_set_src(img, &ui_img_1003866492);
        break;
    }
    case tracker:
    case sensor:
    case lost_and_found:
    case tak_tracker: {
        lv_img_set_src(img, &ui_img_519712240);
        break;
    }
    default:
        lv_img_set_src(img, &ui_img_2104440450);
        break;
    }

    lv_obj_set_style_img_recolor(img, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(img, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void TFTView_320x240::updateNodesOnline(const char *str)
{
    char buf[30];
    sprintf(buf, str, nodesOnline, nodeCount);
    lv_label_set_text(ui_TopNodesOnlineLabel, buf);
    lv_label_set_text(ui_HomeNodesLabel, buf);
}

/**
 * @brief Update last heard display/user_data/counter to current time
 *
 * @param nodeNum
 */
void TFTView_320x240::updateLastHeard(uint32_t nodeNum)
{
    time_t curtime;
    time(&curtime);
    auto it = nodes.find(nodeNum);
    if (it != nodes.end()) {
        time_t lastHeard = (time_t)it->second->LV_OBJ_IDX(node_lh_idx)->user_data;
        it->second->LV_OBJ_IDX(node_lh_idx)->user_data = (void *)curtime;
        lv_label_set_text(it->second->LV_OBJ_IDX(node_lh_idx), "now");
        if (curtime - lastHeard >= 900) {
            nodesOnline++;
            updateNodesOnline("%d of %d nodes online");
        }
        // move to top position
        if (it->first != ownNode) {
            lv_obj_move_to_index(it->second, 1);
        }
    }
}

/**
 * @brief update last heard display for all nodes; also update nodes online
 *
 */
void TFTView_320x240::updateAllLastHeard(void)
{
    uint16_t online = 0;
    time_t lastHeard;
    for (auto &it : nodes) {
        char buf[20];
        if (it.first == ownNode) { // own node is always now, so do update
            time_t curtime;
            time(&curtime);
            lastHeard = curtime;
            it.second->LV_OBJ_IDX(node_lh_idx)->user_data = (void *)lastHeard;
        } else {
            lastHeard = (time_t)it.second->LV_OBJ_IDX(node_lh_idx)->user_data;
        }
        if (lastHeard) {
            bool isOnline = lastHeartToString(lastHeard, buf);
            lv_label_set_text(it.second->LV_OBJ_IDX(node_lh_idx), buf);
            if (isOnline)
                online++;
        }
    }
    nodesOnline = online;
    updateNodesOnline("%d of %d nodes online");
}

void TFTView_320x240::updateUnreadMessages(void)
{
    char buf[20];
    if (unreadMessages > 0) {
        sprintf(buf, unreadMessages == 1 ? "%d new message" : "%d new messages", unreadMessages);
        lv_obj_set_style_bg_img_src(ui_HomeMailButton, &ui_img_2090176585, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        strcpy(buf, "no new messages");
        lv_obj_set_style_bg_img_src(ui_HomeMailButton, &ui_img_1442270332, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_label_set_text(ui_HomeMailLabel, buf);
}

void TFTView_320x240::task_handler(void)
{
    MeshtasticView::task_handler();

    // call every 60s
    time_t curtime;
    time(&curtime);
    if (curtime - lastrun >= 60) {
        lastrun = curtime;
        updateAllLastHeard();
    }
}

#endif