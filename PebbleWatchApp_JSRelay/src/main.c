#include <pebble.h>

typedef enum { STATE_CHATS, STATE_MESSAGES } UiState;

static Window *s_window;
static MenuLayer *s_menu;
static UiState s_state = STATE_CHATS;

// Chats
#define MAX_CHATS 10
static char s_chat_titles[MAX_CHATS][32];
static int s_chat_ids[MAX_CHATS];
static int s_chat_count = 0;
static int s_selected_chat = -1;

// Messages
#define MAX_MSGS 20
static char s_msgs[MAX_MSGS][96];
static bool s_msg_out[MAX_MSGS];
static int s_msg_count = 0;

// Helpers
static void request_chats(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_cstring(out, 0, "get_chats");
  app_message_outbox_send();
}

static void request_messages(int chat_id) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_cstring(out, 0, "get_messages");
  dict_write_int(out, 1, &chat_id, sizeof(int), true);
  app_message_outbox_send();
}

// Menu callbacks
static uint16_t menu_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *ctx) {
  if (s_state == STATE_CHATS) {
    return s_chat_count == 0 ? 1 : s_chat_count;
  } else {
    return s_msg_count == 0 ? 1 : s_msg_count;
  }
}

static void draw_bubble(GContext *ctx, GRect b, const char *text, bool right) {
  int pad = 4;
  int bubble_w = b.size.w - 24;
  int x = right ? (b.size.w - bubble_w - pad) : pad;
  GRect bubble = GRect(x, 2, bubble_w, b.size.h - 4);
  graphics_context_set_fill_color(ctx, right ? GColorPictonBlue : GColorLightGray);
  graphics_fill_rect(ctx, bubble, 4, GCornersAll);
  graphics_context_set_text_color(ctx, right ? GColorWhite : GColorBlack);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(bubble.origin.x + 6, bubble.origin.y + 4, bubble.size.w - 12, bubble.size.h - 8),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void menu_draw(MenuLayer *menu_layer, GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *ctx2) {
  GRect b = layer_get_bounds(cell_layer);
  if (s_state == STATE_CHATS) {
    if (s_chat_count == 0) {
      graphics_draw_text(ctx, "Loading chats...", fonts_get_system_font(FONT_KEY_GOTHIC_18),
        GRect(4, 6, b.size.w-8, b.size.h-12), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      return;
    }
    graphics_draw_text(ctx, s_chat_titles[idx->row], fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(4, 6, b.size.w-8, 24), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else {
    if (s_msg_count == 0) {
      graphics_draw_text(ctx, "Loading messages...", fonts_get_system_font(FONT_KEY_GOTHIC_18),
        GRect(4, 6, b.size.w-8, b.size.h-12), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      return;
    }
    bool right = s_msg_out[idx->row];
    draw_bubble(ctx, b, s_msgs[idx->row], right);
  }
}

static void menu_select(MenuLayer *menu_layer, MenuIndex *idx, void *ctx) {
  if (s_state == STATE_CHATS) {
    s_selected_chat = idx->row;
    s_state = STATE_MESSAGES;
    s_msg_count = 0;
    layer_mark_dirty(menu_layer_get_layer(s_menu));
    request_messages(s_chat_ids[s_selected_chat]);
  } else {
    // In messages view, SELECT sends a canned reply "OK"
    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) == APP_MSG_OK) {
      dict_write_cstring(out, 0, "send");
      dict_write_int(out, 1, &s_chat_ids[s_selected_chat], sizeof(int), true);
      dict_write_cstring(out, 2, "OK");
      app_message_outbox_send();
      vibes_short_pulse();
    }
  }
}

static void back_click(ClickRecognizerRef rec, void *ctx) {
  if (s_state == STATE_MESSAGES) {
    s_state = STATE_CHATS;
    layer_mark_dirty(menu_layer_get_layer(s_menu));
  } else {
    window_stack_pop(true);
  }
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
}

static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t_type = dict_find(iter, 0); // type string
  if (!t_type) return;
  char *type = t_type->value->cstring;
  if (strcmp(type, "chat_item") == 0) {
    Tuple *t_index = dict_find(iter, 1);
    Tuple *t_title = dict_find(iter, 2);
    Tuple *t_id = dict_find(iter, 3);
    if (t_index && t_title && t_id) {
      int i = (int)t_index->value->int32;
      if (i >= 0 && i < MAX_CHATS) {
        strncpy(s_chat_titles[i], t_title->value->cstring, sizeof(s_chat_titles[i])-1);
        s_chat_ids[i] = t_id->value->int32;
        if (i + 1 > s_chat_count) s_chat_count = i + 1;
        layer_mark_dirty(menu_layer_get_layer(s_menu));
      }
    }
  } else if (strcmp(type, "chats_done") == 0) {
    layer_mark_dirty(menu_layer_get_layer(s_menu));
  } else if (strcmp(type, "message_item") == 0) {
    Tuple *t_index = dict_find(iter, 1);
    Tuple *t_text = dict_find(iter, 2);
    Tuple *t_out = dict_find(iter, 3);
    if (t_index && t_text && t_out) {
      int i = (int)t_index->value->int32;
      if (i >= 0 && i < MAX_MSGS) {
        strncpy(s_msgs[i], t_text->value->cstring, sizeof(s_msgs[i])-1);
        s_msg_out[i] = t_out->value->int32 != 0;
        if (i + 1 > s_msg_count) s_msg_count = i + 1;
        layer_mark_dirty(menu_layer_get_layer(s_menu));
      }
    }
  } else if (strcmp(type, "messages_done") == 0) {
    layer_mark_dirty(menu_layer_get_layer(s_menu));
  }
}

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  s_menu = menu_layer_create(b);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = menu_num_rows,
    .draw_row = menu_draw,
    .select_click = menu_select,
  });
  menu_layer_set_click_config_onto_window(s_menu, w);
  layer_add_child(root, menu_layer_get_layer(s_menu));
  request_chats();
}

static void window_unload(Window *w) {
  menu_layer_destroy(s_menu);
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load, .unload = window_unload
  });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_open(512, 512);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) { init(); app_event_loop(); deinit(); }