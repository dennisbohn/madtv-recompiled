/* Prototypen aller übersetzten Spielfunktionen, Kommentar = Adresse im Original (re/notes/sigs). */
#ifndef MADTV_GAME_H
#define MADTV_GAME_H

#include <stdint.h>

#include "dgroup.h"
#include "rtlib.h"
#include "sound.h"
#include "globals.h"


/* ---- Segment 1054 ---- */
void game_main(int argc, char **argv); /* 1054:0005 */
extern int16_t g_opponent_stars; /* DS:0580, Sternchen im 2. Argument */
/* PORT: Anzahl der Filme, Serien und Shows (Einträge 1..n von 201/202/203.HES; im Original fest
 * 200, 50 und 50, seg_2a62.c) */
extern int16_t g_film_count, g_series_count, g_show_count;
/* Programme mit Sonderrolle (Nummern der Original-Datenbanken; Mods müssen sie beibehalten) */
enum {
    FILM_NFL = 0x1d,           /* "Football-Übertragung": Live-Rechte für 3 NFL-Spiele mit Terminen */
    FILM_BETTY_CULTURE = 0x3a, /* "Kultur Heute": Bettys Sendung, stärkt beim Senden den Image-Balken */
    FILM_OLYMPICS = 0xbb,      /* "Olympische Sommerspiele": Rechte für 5 Disziplinen mit Terminen */
    SHOW_VIEWER_POLL = 0x18,   /* "Zuschauer-Befragung": immer im Studio-Angebot */
    SERIES_START_FIRST = 0x12, /* Startserien 18..20 (Dokumentationen), je Spieler eine */
    SERIES_START_COUNT = 3,
};
void hotkey_handle(int16_t key); /* 1054:076c, med */
void init_tables(void); /* 1054:0927 */
void config_read(void *dst, uint16_t n); /* 1054:0bd3 */
void room_reload(void); /* 1054:0bfc */
void room_enter(int16_t room); /* 1054:0d49 */
void room_load(int16_t room); /* 1054:0e4c */
void room_free_resources(void); /* 1054:1bb9 */
void room_load_background(int16_t hes_no); /* 1054:1ce0 */
void day_begin(void); /* 1054:1d57 */
void clock_update(int16_t force_redraw); /* 1054:1ea5 */
void stub_21bf(void); /* 1054:21bf */
void archive_find_selected(void); /* 1054:21c4 */
void schedule_remove_hour(int16_t hour); /* 1054:2276 */
void schedule_clear_programme(int16_t player, int16_t hour); /* 1054:2298 */
int16_t schedule_hover_editable(void); /* 1054:22de, med */
void info_board_build(void); /* 1054:2386, med */
void room_leave(void); /* 1054:2490 */
void day_end_walk_home(void); /* 1054:2e1c, med */
void player_walk_to_exit(void); /* 1054:2e60, med */
void player_walk_to_boss(void); /* 1054:2efb, med */
void screen_transition(int16_t dir, int16_t delay, int16_t effect); /* 1054:2fa3 */
void screen_wipe_in(Bitmap *src, int16_t x, int16_t y, int16_t w, int16_t h, int16_t dir, int16_t delay); /* 1054:2fd9 */
void status_show_start(void); /* 1054:320b, med */
void tv_show_current_programme(void); /* 1054:3240, med */
void tv_show_picture(int16_t pic_id); /* 1054:3357, med */
void tv_anim_setup(int16_t force); /* 1054:34c2, low */
void tv_anim_stop(void); /* 1054:3a1c, med */
void savegame_names_alloc(void); /* 1054:3a29, med */
void savegame_names_load(void); /* 1054:3a53, med */
void savegame_names_save(void); /* 1054:3ab1, med */
SpriteBank *sprite_pool_load(int16_t hes_no); /* 1054:3b34, med */
void schedule_room_refresh(void); /* 1054:3bb6, med */
void movie_agency_reload_texts(void); /* 1054:3c59, med */
void language_select(void); /* 1054:3ca4 */
void language_poll(void); /* PORT: Sprachwechsel aus Fenstermenü/F10 */
void game_menu_poll(void); /* PORT: Menü "Spiel" und Schalter des Control-Centers */

/* ---- Segment 1433 ---- */
void text_cache_news(int16_t player); /* 1433:0002, med */
void text_cache_load_news(int16_t slot, int16_t text_id); /* 1433:0120, med */
void text_cache_news_free(void); /* 1433:01de */
void text_cache_titles(int16_t room, int16_t player); /* 1433:021b */
int16_t text_cache_has_film(int16_t film_id); /* 1433:0855 */
void text_cache_load_film(int16_t film_id, int16_t slot); /* 1433:0889 */
void text_cache_titles_free(int16_t fixed); /* 1433:09a2 */
void text_cache_film_free(int16_t slot); /* 1433:09df */
void text_cache_descr(int16_t room, int16_t player); /* 1433:0a45, med */
int16_t text_cache_has_series(int16_t series_id); /* 1433:0feb */
void text_cache_load_series(int16_t series_id, int16_t slot); /* 1433:101f */
void text_cache_descr_free(int16_t fixed); /* 1433:1134 */
void text_cache_series_free_slot(int16_t slot); /* 1433:1171 */
void text_cache_series(int16_t room, int16_t player); /* 1433:11cf, med */
void text_cache_load_show(int16_t text_id, int16_t slot); /* 1433:1373 */
void text_cache_series_free(void); /* 1433:148c */
void text_cache_show_free_slot(int16_t slot); /* 1433:14a0 */

/* ---- Segment 1581 ---- */
void pause_begin(void);       /* PORT: Pause (aus room_click) */
void pause_end(void);
int16_t pause_allowed(void);
void options_speed_set(int16_t speed); /* PORT: Knöpfe Spieltempo im Optionen-Monitor */
void options_switch_set(int16_t which, int16_t on); /* PORT: Schalter Fernsehfamilie/-bild/Musik */
int16_t options_switch_on(int16_t which);
void room_click(void); /* 1581:0009 */
void room_click_auction(void); /* 1581:0345, med */
void room_click_movie_agency(void); /* 1581:088c */
void room_click_schedule(void); /* 1581:0c78 */
void room_click_building(void); /* 1581:19f0 */
void room_click_info_board(void); /* 1581:1bbd, med */
void room_click_elevator_board(void); /* 1581:201c, med */
void room_click_office(void); /* 1581:2220, med */
void room_click_news_studio(void); /* 1581:23ca, med */
void room_click_station_map(void); /* 1581:29fc, med */
void room_click_ad_agency(void); /* 1581:3156 */
void room_click_screenplay_agency(void); /* 1581:353d, med */
void room_click_real_estate(void); /* 1581:37d8, med */
void room_click_studio(void); /* 1581:3aaf, med */
void save_menu_refresh(void); /* 1581:4112, med */
void room_click_save_menu(void); /* 1581:4135, med */
void room_click_porter(void); /* 1581:454e, low */

/* ---- Segment 19e6 ---- */
void room_click_boss(void); /* 19e6:0004, med */
void room_click_new_game_setup(void); /* 19e6:09f7 */
void room_click_supermarket(void); /* 19e6:0c29 */
void room_click_supermarket_shelves(void); /* 19e6:0f78, med */
void supermarket_present_take(void); /* 19e6:1b54, med */
void betty_present_give_close(void); /* 19e6:1b7f, low */
void supermarket_checkout(void); /* 19e6:1baa, med */
void room_click_betty(void); /* 19e6:1be9, med */
void shelf_item_draw(void); /* 19e6:1e4f, low */
void shoplist_redraw(void); /* 19e6:1eb8, med */
void news_item_draw(void *item, int16_t x, int16_t y); /* 19e6:1ec3, med */
int16_t hotspot_at_mouse(int16_t any_owner); /* 19e6:1f8c */
int16_t station_next_valid(int16_t idx); /* 19e6:2022 */
int16_t station_prev_valid(int16_t idx); /* 19e6:207c */
void programme_sell_selected(void); /* 19e6:20d6, med */
void adcontract_offers_commit(void); /* 19e6:2174, med */
void screenplay_offers_commit(void); /* 19e6:21f1, med */
void dealer_offer_return(void); /* 19e6:2246, med */
void archive_add_selected(void); /* 19e6:2277, med */
void offer_list_add_selected(void); /* 19e6:22f7, low */
void auction_withdraw_item(void); /* 19e6:2333, low */
void adcontract_offer_reserve(void); /* 19e6:241e, low */
void adcontract_sign(void); /* 19e6:246d, med */
void screenplay_buy_selected(void); /* 19e6:24f3, med */
void screenplay_add_to_bag(void); /* 19e6:2591, med */
void studio_screenplay_to_bag(void); /* 19e6:2602, low */
void studio_screenplay_insert(void); /* 19e6:2667, med */
void studio_screenplay_remove(void); /* 19e6:26b9, med */
void studio_screenplay_swap(void); /* 19e6:2760, med */
void panel_slide_toggle(int16_t *pos, int16_t xcol, int16_t y); /* 19e6:2806 */
void tv_refresh(void); /* 19e6:287c, med */
void station_select_next(void); /* 19e6:289f, med */

/* ---- Segment 1c74 ---- */
void hes_init_file_list(void); /* 1c74:0008 */
int16_t disk_detect(void); /* 1c74:0039, med */
void timer_pause(void); /* 1c74:0051 */
void timer_resume(void); /* 1c74:0067 */
void drive_io_begin(void); /* 1c74:007d, med */
void drive_io_end(void); /* 1c74:008c, med */
void debug_nop(void); /* 1c74:00a0 */
void *hes_load_raw(int16_t hes_no); /* 1c74:00a1 */
void hes_read_into(int16_t hes_no, void *buf); /* 1c74:00f4 */
SpriteBank *hes_load_unpack0(int16_t hes_no); /* 1c74:0111 */
SpriteBank *hes_load_unpack(int16_t hes_no); /* 1c74:018e */
int16_t rand_range(int16_t n, int16_t seed); /* 1c74:020d */
void clock_reset(void); /* 1c74:023d */
void clock_suspend(void); /* 1c74:0260 */
void clock_resume(void); /* 1c74:0291 */
void clock_get_cached(int16_t *out_seconds); /* 1c74:02b5 */
void clock_get_seconds(int16_t *out_seconds); /* 1c74:02c3 */
void spritebank_fix_pointers(SpriteBank *bank); /* 1c74:0384 */
void far_free_ptr(ptr32 *pp); /* 1c74:03e4 */
int32_t figure_frame_bytes(int16_t w, int16_t with_extra); /* 1c74:0408, med */
int16_t hes_size_or_ask_disk(int16_t hes_no, int16_t *unpacked_size, int16_t mode); /* 1c74:0457 */
void font_load(int16_t hes_no, int16_t slot, int16_t mode); /* 1c74:04d1 */
SpriteBank *font_get(int16_t slot, int16_t mode); /* 1c74:0517 */
void figure_sprites_load_all(void); /* 1c74:0570 */
void figure_sprites_load(int16_t hes_no, int16_t slot); /* 1c74:0610 */
void *far_calloc(uint16_t n, uint16_t size); /* 1c74:06a3 */
void far_free(void *p); /* 1c74:06c9 */
void clock_stop(void); /* 1c74:06db */
void clock_start(void); /* 1c74:0712 */
char *text_get_copy(int16_t text_id); /* 1c74:0733 */

/* ---- Segment 1cec ---- */
void text_draw(int16_t x, int16_t y, const char *str); /* 1cec:000b */
void text_draw_right_to_free(int16_t x, int16_t y, char *str, Bitmap *dst); /* 1cec:0033 */
void text_draw_free(int16_t x, int16_t y, char *str); /* 1cec:0076 */
void text_draw_to(int16_t x, int16_t y, const char *str, Bitmap *dst); /* 1cec:00ab */
void text_box_wrap(int16_t x, int16_t y, const char *str, int16_t width, uint16_t flags); /* 1cec:00d1 */
void text_draw_to_free(int16_t x, int16_t y, char *str, Bitmap *dst); /* 1cec:00fb */
void text_draw_centered_to_free(int16_t x, int16_t y, char *str, Bitmap *dst); /* 1cec:012e */
void text_draw_centered(int16_t x, int16_t y, const char *str); /* 1cec:0177 */
void text_draw_centered_to(int16_t x, int16_t y, const char *str, Bitmap *dst); /* 1cec:01b5 */
void text_box_centered(int16_t x, int16_t y, const char *str, int16_t width, uint16_t flags, Bitmap *dst, int16_t max_lines); /* 1cec:01f1 */
void text_box_to_free(int16_t x, int16_t y, char *str, int16_t width, uint16_t flags, Bitmap *dst); /* 1cec:022e */
void text_box_free(int16_t x, int16_t y, char *str, int16_t width, uint16_t flags, Bitmap *dst, int16_t max_lines); /* 1cec:0263 */
void text_box_wrap_free(int16_t x, int16_t y, char *str, int16_t width, uint16_t flags); /* 1cec:0297 */
void text_box(int16_t x, int16_t y, const char *str, int16_t width, uint16_t flags, Bitmap *dst, int16_t max_lines); /* 1cec:02ce */
int16_t text_line_count(const char *str, int16_t width, uint16_t flags); /* 1cec:046d */

/* ---- Segment 1d41 ---- */
char *fmt(const char *format, ...); /* 1d41:0003 */
int16_t fmt_count_digits(const char *s); /* 1d41:049b */
void fmt_int(char *dst, int16_t *value, uint16_t *width, const char *pad); /* 1d41:04c7 */
void fmt_long(char *dst, int32_t *value, uint16_t *width, const char *pad); /* 1d41:05c2 */
int32_t str_to_long(const char *s); /* 1d41:06d7, med */
void str_overwrite_prefix(char *dst, const char *src, int16_t n, const char *suffix); /* 1d41:072c, low */
void far_memset(void *dst, uint16_t n); /* 1d41:07ff */
void far_memcpy(void *dst, const void *src, uint16_t n); /* 1d41:081a */
int16_t str_length(const char *s); /* 1d41:083d */
void str_fill(char *dst, const char *ch, int16_t n); /* 1d41:0857 */
int16_t text_fit_width(const char *s, int16_t width_cols); /* 1d41:0874 */
int16_t text_width(const char *s); /* 1d41:08d7 */
int16_t str_is_prefix(const char *prefix, const char *s); /* 1d41:0927, low */

/* ---- Segment 1dd5 ---- */
int16_t game_tick(void); /* 1dd5:000f */
void screen_update(void); /* 1dd5:0884 */
void lower_screen_present(void); /* 1dd5:0a44, med */
void cursor_reset(void); /* 1dd5:0b74 */
int16_t text_input(int16_t x, int16_t y, int16_t max_cols, char *buf); /* 1dd5:0b8d */
void set_cursor_shape(int16_t shape); /* 1dd5:0d83 */
void tetris_tick(int16_t drop); /* 1dd5:0dcb, med */
void draw_tile_box(int16_t xcol, int16_t y, int16_t wcols, int16_t h, int16_t style); /* 1dd5:169a */
void draw_tile_box_to(int16_t xcol, int16_t y, int16_t wcols, int16_t h, int16_t style, Bitmap *dst); /* 1dd5:16bd */

/* ---- Segment 1f51 ---- */
void mouse_show(void); /* 1f51:0003 */
void mouse_hide(void); /* 1f51:019e */
void ease_step(int16_t *delta, int16_t *value); /* 1f51:01f3 */
void draw_string(int16_t x, int16_t y, const char *str, Bitmap *dst, int16_t fixed_digits); /* 1f51:0250 */
void draw_frame_cells(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Bitmap *dst, int16_t style); /* 1f51:036f */
void draw_frame(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Bitmap *dst, int16_t style); /* 1f51:03c1 */
void frame_piece_draw(int16_t piece_ofs, int16_t x, int16_t y); /* 1f51:04cb */
void text_window_draw(int16_t x0, int16_t y0, int16_t x1, int16_t y1, char *text, int16_t visible); /* 1f51:0516, med */
int16_t window_open(int16_t x0, int16_t y0, int16_t x1, int16_t y1, char *text, int16_t mode); /* 1f51:057c, med */
void popup_box_show(int16_t x0, int16_t y0, int16_t x1, int16_t y1, char *text); /* 1f51:0686, med */
void window_close(int16_t slot); /* 1f51:06d5 */
void window_restore(int16_t slot); /* 1f51:0762 */
void room_frame_draw(void); /* 1f51:07c3, med */
void room_frame_draw_alt(void); /* 1f51:08d9, med */
int16_t sprite_ofs(int16_t idx); /* 1f51:0a15 */
int16_t figure_sprite_ofs(int16_t fig, int16_t frame); /* 1f51:0a26 */
void drag_image_draw(Bitmap *src); /* 1f51:0a50, med */
void bmp_draw_masked(Bitmap *src, Bitmap *dst, int16_t x, int16_t y); /* 1f51:0a7f */
void bmp_draw(Bitmap *src, Bitmap *dst, int16_t x, int16_t y); /* 1f51:0ab0 */
void draw_bitmap(Bitmap *src, int16_t x, int16_t y); /* 1f51:0ae1 */
void screen_grab(Bitmap *dst, int16_t x, int16_t y); /* 1f51:0b14 */
void bank_draw_opaque(int16_t ofs, int16_t x, int16_t y); /* 1f51:0b47 */
void bank_draw(int16_t ofs, int16_t x, int16_t y); /* 1f51:0b8e */
void target_draw_sprite(Bitmap *src, int16_t x, int16_t y); /* 1f51:0bd5 */
void bank_draw_fixed_y(int16_t ofs, int16_t x); /* 1f51:0c08 */
void bank_draw_fixed_x(int16_t ofs, int16_t y); /* 1f51:0c50 */
void bank_draw_current(int16_t x, int16_t y); /* 1f51:0c98 */
void draw_sprite(Bitmap *src, int16_t x, int16_t y); /* 1f51:0ce1 */
void draw_bitmap_fast(Bitmap *src, int16_t x, int16_t y); /* 1f51:0d14 */
void blit_sprite_masked(Bitmap *src, Bitmap *dst, int16_t dx, int16_t dy, int16_t sx, int16_t sy, int16_t w, int16_t h); /* 1f51:0d4f */
void bmp_copy_at(Bitmap *src, Bitmap *dst, int16_t x, int16_t y); /* 1f51:0e6f */
void bmp_copy(Bitmap *src, Bitmap *dst); /* 1f51:0ea9 */
void blit_rect(Bitmap *src, Bitmap *dst, int16_t dx, int16_t dy, int16_t sx, int16_t sy, int16_t w, int16_t h); /* 1f51:0ec4 */
void elevator_request_add(int16_t floor); /* 1f51:0fd8, med */
void elevator_request_remove(int16_t floor); /* 1f51:100e, med */
void floor_door_prev(int16_t floor, int16_t *slot, int16_t exclude); /* 1f51:105c, med */
int16_t floor_door_count(int16_t floor); /* 1f51:10cf, med */
void clip_set_small(void); /* 1f51:110f, med */
void clip_reset(void); /* 1f51:112e */
void clip_set_upper(void); /* 1f51:114d */
int16_t mouse_x(void); /* 1f51:1158 */
int16_t mouse_y(void); /* 1f51:1186 */
void building_walls_draw(int16_t variant); /* 1f51:11ef, low */
void door_draw(int16_t floor, int16_t slot, int16_t state); /* 1f51:12ca, med */
void panel_build(void); /* 1f51:1419, med */
void figure_draw(int16_t fig, int16_t frame); /* 1f51:1503, med */
int16_t input_poll(void); /* 1f51:1628 */
void mouse_update(void); /* 1f51:16dd */
void room_animate(int16_t mode); /* 1f51:1703, med */
void anim_step_every(int16_t period, int16_t *frame); /* 1f51:2c36 */
void room_region_restore(int16_t region); /* 1f51:2c53, med */
void popup_box_create(int16_t x0, int16_t y0, int16_t x1, int16_t y1); /* 1f51:2e46, med */
void popup_box_present(int16_t xcol, int16_t yrow); /* 1f51:3060, med */
void tv_picture_update(void); /* 1f51:3099, med */
Bitmap *tv_picture_compose(void); /* 1f51:3105, med */
void tv_picture_bg_draw(void); /* 1f51:38b1, med */
void tv_layer0_draw(void); /* 1f51:3946, low */
void tv_layer1_draw(void); /* 1f51:3a78, low */
void tv_layer2_draw(void); /* 1f51:3b02, low */
void tv_layer3_draw(void); /* 1f51:3bb7, low */
void tv_layer4_draw(void); /* 1f51:3c2a, low */
void tv_picture_tick(void); /* 1f51:3cc6, med */
void room_items_draw(void); /* 1f51:3cd2, low */
void room_stack_overlay_draw(void); /* 1f51:3d24, low */
void room_owner_marker_draw(void); /* 1f51:3e0f, low */
void room_variant_overlay_draw(void); /* 1f51:3e66, low */
void room_crowd_draw(void); /* 1f51:3eb9, low */
int16_t mouse_overlaps_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1); /* 1f51:4095, med */
void anim_blink_draw(void *anim, int16_t state, int16_t sprite, int16_t dx, int16_t y); /* 1f51:410c, med */
int16_t mouse_in_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1); /* 1f51:416f */
void game_end_sequence(void); /* 1f51:41a1, med */

/* ---- Segment 239a ---- */
void room_hover(void); /* 239a:000a */
void hover_dispatch(int16_t id); /* 239a:1568, med */
void channel_info_show(void); /* 239a:3132, med */
void money_tooltip_show(void); /* 239a:31b2, med */
void day_tooltip_show(void); /* 239a:31f5, med */
void channel_info_card_draw(Bitmap *dst); /* 239a:3228, med */
void channel_programme_title_load(void); /* 239a:3849, med */
void ad_requirements_text_build(int16_t ad); /* 239a:3a2f, low */
void ad_targets_text_build(int16_t ad, int16_t sign); /* 239a:3af8, low */
void tooltip_show_text(const char *text); /* 239a:3cb1 */
void card_band_fill(int16_t y, int16_t h, int16_t style); /* 239a:3cdb, med */
void layout_box(const char *spec); /* 239a:3cff, med */
void tooltip_present(void); /* 239a:3dff */
void info_card_film_small(int16_t film); /* 239a:3e0f, med */

/* ---- Segment 27bb ---- */
void building_draw(int16_t scroll_y); /* 27bb:000b, med */
void building_view_present(void); /* 27bb:0718 */
void building_elevator_draw(int16_t floor); /* 27bb:0756, med */
void building_sides_draw(void); /* 27bb:0a3b, med */
void building_animate(void); /* 27bb:0b9e */
void figures_update(void); /* 27bb:0ce1, med */
void npc_update(void); /* 27bb:11fb, med */
void elevator_update(void); /* 27bb:198f, med */
void building_hover_draw(void); /* 27bb:1be9, med */
char *door_label_text(int16_t slot, int16_t floor); /* 27bb:21dd, med */
void player_eliminated_sequence(void); /* 27bb:231e, med */
void facade_draw(void); /* 27bb:2951, med */

/* ---- Segment 2a62 ---- */
void db_load_all(void); /* 2a62:000c */
void db_texts_load(void); /* PORT: Textteil von db_load_all */
void text_cache_reload(void); /* PORT: Text-Caches in neuer Sprache */
void text_pool_take(int16_t len, ptr32 *out); /* 2a62:029e, med */
void episodes_init_from_first(void); /* 2a62:02eb, med */
void money_add(int16_t category, int32_t amount); /* 2a62:036d */
void news_update(int16_t elapsed, int16_t min_new); /* 2a62:0387, med */
int16_t news_count_type1(void); /* 2a62:0405, med */
void news_generate(int16_t chain); /* 2a62:0430, med */
void news_add_special(int16_t topic, int16_t param1, int16_t param2); /* 2a62:0627 */
int16_t news_chain_step(int16_t max_time); /* 2a62:06c8, med */
void news_chains_refill(void); /* 2a62:07ed, med */
int16_t news_variant_in_use(int16_t topic, int16_t variant); /* 2a62:0a1c, med */
void newscast_rate(int16_t player); /* 2a62:0ab6, med */
void auction_assign_unbid(void); /* 2a62:0afa, low */
void message_slot_alloc(int16_t player); /* 2a62:0b46, med */
void programmes_on_air_update(void); /* 2a62:0b96, med */
void schedule_day_rotate(void); /* 2a62:0cfa */
void news_distribute(void); /* 2a62:0df2, med */
void news_values_decay(int16_t pct_archive, int16_t pct_newscast); /* 2a62:0ea8, med */
int16_t series_next_episode(int16_t player, int16_t series, int16_t hour); /* 2a62:0f7c, med */
int32_t money_available(int16_t player); /* 2a62:1041 */
void programme_remove_from_archives(int16_t prog); /* 2a62:111a */
void player_news_drop_oldest(int16_t player); /* 2a62:1299 */
void player_news_remove(int16_t player, int16_t idx); /* 2a62:1302 */
void list5_remove(int16_t player, int16_t idx); /* 2a62:1368, low */
void production_remove(int16_t player, int16_t idx); /* 2a62:13d1, med */
void list_remove(int16_t player, int16_t idx); /* 2a62:1437, med */
void series_episode_wear(int16_t series, int16_t episode); /* 2a62:152c, med */
void film_wear(int16_t film); /* 2a62:158e, med */
int32_t auction_bid_increment(int16_t lot); /* 2a62:15db */

/* ---- Segment 2bd1 ---- */
int16_t shows_free_count(void); /* 2bd1:0005, med */
void programmes_mark_scheduled(void); /* 2bd1:004b, med */
int16_t newscast_quality_sum(void); /* 2bd1:012f, med */
int16_t channel_value_sum(void); /* 2bd1:016c, low */
int16_t show_episode_count(int16_t show); /* 2bd1:01ac, med */
void programmes_topicality_recover(void); /* 2bd1:01f6, med */
void hour_start(void); /* 2bd1:0301, med */
int16_t archive_capacity_check(int16_t player, int16_t prog, int16_t genre); /* 2bd1:0668, low */
void programme_genre_get(int16_t *prog, int16_t *genre); /* 2bd1:07c2 */
int16_t ad_spots_remaining(int16_t player, int16_t hour); /* 2bd1:07fb, med */
void ad_contracts_expire(void); /* 2bd1:08f0, med */
void film_betty_culture_check(void); /* 2bd1:0a80, low */
void auction_lot_set(int16_t lot, int16_t prog, int16_t kind); /* 2bd1:0bc8, med */
void player_message_push(int16_t msg_id, int16_t player, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5); /* 2bd1:0d6e */
void loan_interest_pay(void); /* 2bd1:0e2f, med */
void daily_costs_pay(void); /* 2bd1:0e8b, med */
void market_daily_cleanup(void); /* 2bd1:10d6, low */
void licenses_expire(void); /* 2bd1:11e3, med */
void actors_market_shuffle(void); /* 2bd1:12cc, low */
void start_programmes_assign(void); /* 2bd1:13b0, med */
int32_t station_reach_at(int16_t x, int16_t y); /* 2bd1:1637, med */
int16_t map_cell_value(int16_t x, int16_t y, uint16_t *map); /* 2bd1:1729, med */
int32_t map_population_sum(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t *map); /* 2bd1:175f */
void stations_reach_update(int16_t player); /* 2bd1:1847, med */
void coverage_map_build(int16_t player); /* 2bd1:1906, med */
void building_map_door_pos(int16_t floor, int16_t slot, int16_t *x, int16_t *y); /* 2bd1:1af5, med */
char *door_name_text(int16_t floor, int16_t slot); /* 2bd1:1b88, med */
int16_t hour_index(void); /* 2bd1:1cef */
int16_t hour_index_after_min5(void); /* 2bd1:1d05 */
int16_t hour_index_after_min55(void); /* 2bd1:1d18 */
void coverage_stamp(int16_t station, int16_t player, uint16_t *src_map, uint16_t *dst_map); /* 2bd1:1d2b, med */
int32_t station_reach_gain(int16_t x, int16_t y); /* 2bd1:1e49, med */
int32_t mask_population_count(uint16_t *mask); /* 2bd1:1fdf */
int16_t show_role_count(int16_t show); /* 2bd1:208f, low */
void programme_buy(int16_t prog, int16_t player, int32_t price); /* 2bd1:20e4 */
int16_t episode_cast_complete(int16_t show, int16_t episode); /* 2bd1:22af, med */
int16_t actor_suitability(int16_t show, int16_t episode, int16_t role, int16_t actor); /* 2bd1:2316, low */
int32_t round_to_step(int32_t value, int32_t step); /* 2bd1:2404 */
int32_t rating_from_viewers(int16_t player, int32_t viewers); /* 2bd1:2431, low */
void film_nfl_tick(void); /* 2bd1:24a5, med */
void film_olympics_tick(void); /* 2bd1:24d9, med */
void event_npc_init(void); /* 2bd1:250c, med */
void random_events(void); /* 2bd1:266b, med */
void event_npc_strike(void); /* 2bd1:2a41, med */
void programme_lose(int16_t prog, int16_t msg_id, int16_t player); /* 2bd1:2e9e, med */
char *film_title_get(int16_t film); /* 2bd1:3024 */
char *film_descr_get(int16_t film); /* 2bd1:306e */
char *series_title_get(int16_t series); /* 2bd1:30b8 */
char *series_descr_get(int16_t series); /* 2bd1:3102 */
char *episode_title_get(int16_t series, int16_t episode); /* 2bd1:314c */
char *episode_descr_get(int16_t series, int16_t episode); /* 2bd1:31a0 */
char *show_text_title_get(int16_t id); /* 2bd1:31f4, med */
char *show_text_get(int16_t id, int16_t idx); /* 2bd1:322e, med */
int16_t room_entry_check(int16_t fig, int16_t query); /* 2bd1:3271, med */
void ratings_rank_update(void); /* 2bd1:363d, low */
void image_distribute(int16_t *deltas); /* 2bd1:3895, med */
int32_t price_step_next(int32_t value); /* 2bd1:3b5d, med */
int16_t genre_value_get(int16_t genre); /* 2bd1:3b96, low */
int32_t film_price(int16_t film); /* 2bd1:3ba7 */
int32_t series_price(int16_t series); /* 2bd1:3c6e */
char *fmt_money(int32_t value, int16_t mode); /* 2bd1:3d63 */
int16_t ad_genre_allowed(int16_t ad, int16_t genre); /* 2bd1:3e86, med */
int16_t news_count_by_category(int16_t category); /* 2bd1:3f0b, med */
int16_t news_find_next(int16_t category, int16_t from); /* 2bd1:3f4f, med */
int16_t image_at_least(int16_t player, int16_t min); /* 2bd1:3fa2, med */
void news_subscriptions_check(void); /* 2bd1:3fc7, med */
int16_t player_flag(int16_t bit, int16_t op); /* 2bd1:4048, med */
int16_t contest_tick(void); /* 2bd1:408e, med */
int16_t contest_score(int16_t player); /* 2bd1:427c, med */
int16_t contest_winner(void); /* 2bd1:432d */
int16_t contest_share(int16_t player); /* 2bd1:43b5 */

/* ---- Segment 300f ---- */
void finance_day_rotate(void); /* 300f:000b */
void stats_day_rotate(void); /* 300f:0052 */
void studio_slot_release(int16_t slot); /* 300f:0204, low */
void stations_settle(int16_t player); /* 300f:0286, med */
void production_clear_first(int16_t player); /* 300f:03a5, low */
void case_remove(int16_t idx); /* 300f:03c2, med */
void production_costs_update(int16_t player); /* 300f:03f8, low */
void case_store_to_archive(void); /* 300f:0599, med */

/* ---- Segment 3077 ---- */
void player_toggle_flag(int16_t player, int16_t idx); /* 3077:0049, med */
int16_t trade_item_toggle(uint16_t idx); /* 3077:0087, low */
int16_t schedule_place_ad(int16_t contract_idx, int16_t hour, int16_t player); /* 3077:0195 */
int16_t schedule_place_programme(int16_t archive_idx, int16_t genre, int16_t hour, int16_t player); /* 3077:01fd */
void schedule_clear_slot(int16_t player, int16_t hour); /* 3077:0415 */
void schedule_remove_programme(int16_t player, int16_t hour); /* 3077:0453 */
int16_t programme_check_sellable(int16_t archive_idx, int16_t genre); /* 3077:04c2, med */
int16_t broadcast_hour_tick(void); /* 3077:0781 */
int16_t money_check(int16_t player, int32_t amount); /* 3077:087b */
int16_t studio_prepare_episode(int16_t studio); /* 3077:08d7, med */
int16_t studio_produce_episode(int16_t studio); /* 3077:0a98, med */
void studio_rent_request(int16_t player, int16_t studio); /* 3077:0e92, med */
void studio_release_request(int16_t studio); /* 3077:0ed1, med */
void studio_rent(int16_t studio, int16_t player); /* 3077:0f08, med */
void studio_release(int16_t studio); /* 3077:10b8, med */
void savegame_load(int16_t hes_no); /* 3077:11be */
void savegame_serialize(int16_t hes_no); /* 3077:11d2 */
void savegame_save(int16_t hes_no); /* 3077:1d69 */
void savegame_io(void *data, int16_t len); /* 3077:1d7c */
int16_t archive_contains(int16_t player, int16_t genre, int16_t prog_id); /* 3077:1e75 */

/* ---- Segment 3264 ---- */
void ad_market_update(void); /* 3264:0001, med */
void production_market_update(void); /* 3264:0274, med */
void market_update_prices(void); /* 3264:0438, med */
void auction_day_end(void); /* 3264:0717, med */
void ratings_calc(int16_t phase); /* 3264:08ce */
void daily_topic_pick(void); /* 3264:1759, low */
int16_t schedule_get_programme(int16_t player, int16_t hour); /* 3264:17e2 */
char *name_table_get(int16_t id, int16_t part); /* 3264:1825, low */
void day_check_players(void); /* 3264:185a, low */

/* ---- Segment 340d ---- */
void show_message_text(char *text); /* 340d:0002 */
void show_message(int16_t text_id); /* 340d:0050 */
void show_info_overlay(char *text); /* 340d:006a, med */
void message_box_draw(int16_t mode, char *text); /* 340d:0089 */
void status_money_update(void); /* 340d:00d4 */
void status_text(char *text, int16_t y); /* 340d:0128 */
void status_meter_draw(void); /* 340d:01cb, med */
void schedule_panel_open(int16_t player); /* 340d:0287 */
void schedule_view(int16_t player); /* 340d:03a3 */
void schedule_draw_rows(int16_t player); /* 340d:06dd */
char *schedule_slot_title(int16_t hour, int16_t player); /* 340d:085f */
char *archive_item_title(int16_t genre, int16_t idx, int16_t player); /* 340d:09a2 */
char *archive_title_by_id(int16_t prog_id); /* 340d:09e4 */
void archive_list_dialog(int16_t xcol, int16_t yrow, int16_t mode, int16_t player); /* 340d:0a56 */
void station_map_panel(int16_t part); /* 340d:0c07, low */
void room8_items_panel(int16_t redraw); /* 340d:0f5b, low */
void dialog_frame(int16_t mode, int16_t frame_id); /* 340d:1022 */
void ad_offers_panel(void); /* 340d:144f, med */
void archive_shelf_panel(int16_t player); /* 340d:155a, med */
void series_episode_dialog(int16_t mode); /* 340d:167b, med */
void frame_draw(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t style); /* 340d:1911 */
void frame_draw_to(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t style, Bitmap *dst); /* 340d:1934 */
void cursor_attach_item(int16_t w, int16_t h, int16_t grab_x, int16_t grab_y, char *text, int16_t style); /* 340d:1aa6, med */
void cursor_drop_item(void); /* 340d:1d02 */
void ad_contract_list_dialog(int16_t xcol, int16_t yrow, int16_t mode, int16_t player); /* 340d:1d9a */
void building_directory_panel(void); /* 340d:1f43, med */
void player_flags_draw(int16_t player); /* 340d:21a4, low */
void news_slots_panel(int16_t player, int16_t mode); /* 340d:2246, med */
void player_tabs_panel(int16_t mode); /* 340d:24a1, low */
char *news_format_text(void *news_item, int16_t text_id); /* 340d:27e9, med */
void station_map_view(int16_t player, int16_t mode); /* 340d:2a3c, low */
void auction_panel(int16_t mode); /* 340d:2da7, med */
void production_offers_panel(void); /* 340d:3047, med */
void studio_booking_panel(void); /* 340d:313f, low */
void message_box2_draw(char *text, int16_t mode); /* 340d:3434, med */
void status_ratings_update(void); /* 340d:34b0 */
void studio_panel_draw(void); /* 340d:351e, low */
void studio_cast_preview(void); /* 340d:359d, low */
void studio_production_dialog(int16_t mode); /* 340d:372f, low */
void confirm_bubble_dialog(int16_t mode); /* 340d:386f, low */
void production_plan_panel(void); /* 340d:393b, low */
void people_select_dialog(int16_t mode); /* 340d:40c6, low */
void player_ranking_panel(void); /* 340d:41be, low */
void statistics_panel(void); /* 340d:440b, low */

/* ---- Segment 38b6 ---- */
void production_episode_panel(void); /* 38b6:0003, med */
void genre_menu_dialog(int16_t xcol, int16_t yrow, int16_t mode); /* 38b6:0253, med */
void bubble_text_overlay(int16_t mode, char *text); /* 38b6:03ca, med */
void boss_dialog(int16_t mode, int16_t text_sel); /* 38b6:041f, low */
void minigame_panel(int16_t mode); /* 38b6:066a, low */
void savegame_slots_draw(void); /* 38b6:09b3, med */
void info_bubble_dialog(int16_t mode); /* 38b6:0a2f, low */
void disk_request_dialog(int16_t mode, int16_t text_id); /* 38b6:0a88 */
void banner_two_lines(int16_t mode, char *rec); /* 38b6:0b96, med */
void bubble_text_dialog(int16_t mode, char *text); /* 38b6:0cb4 */
void bubble_text_dialog_small(int16_t mode, char *text); /* 38b6:0cfa */
void speech_bubble(int16_t mode, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t style); /* 38b6:0d45 */
void speech_bubble_text52(int16_t mode); /* 38b6:0dc5, low */
void options_panel(void); /* 38b6:0e0a, med */
void option_switch_draw(int16_t pos); /* 38b6:1059, med */
void bubble_dialog_8820(int16_t mode); /* 38b6:10f0, low */
void status_bar_draw(void); /* 38b6:1179, med */
void player_setup_screen(int16_t skip_player); /* 38b6:1269, med */
void station_bid_dialog(int16_t mode, int32_t *bids); /* 38b6:14e5, low */
void player_status_bubble(int16_t mode); /* 38b6:1721, low */
void player_message_dialog(uint16_t player); /* 38b6:17fd, med */
void player_info_popup(uint16_t player); /* 38b6:1d91, low */
void balance_sheet(void); /* 38b6:1e63, med */
void hline_tiles_draw(int16_t x0, int16_t x1, int16_t y); /* 38b6:2396 */
void yes_no_dialog(int16_t mode); /* 38b6:23e5, med */
void yes_no_dialog2(int16_t mode); /* 38b6:2473, med */

/* ---- Segment 3b06 ---- */
int16_t min_i16(int16_t a, int16_t b); /* 3b06:0000 */
int16_t max_i16(int16_t a, int16_t b); /* 3b06:0015 */
int32_t max_i32(int32_t a, int32_t b); /* 3b06:0027 */

/* ---- Segment 3b0a ---- */
void ai_tick(void); /* 3b0a:000c */
void ai_optimize_programmes(void); /* 3b0a:02cf, med */
void ai_optimize_ads(void); /* 3b0a:066c, med */
int16_t ai_find_better_ad(int16_t contract_idx, int16_t hour); /* 3b0a:081c, med */
void ai_fill_ad_slots(void); /* 3b0a:0a41, med */
void ai_fill_programme_slots(void); /* 3b0a:0cf3, med */
void ai_plan_schedule(void); /* 3b0a:138e, med */
void ai_place_ad_contract(int16_t contract_idx, int8_t spots); /* 3b0a:150e, med */
void ai_place_ad_range(int16_t contract_idx, int8_t from_hour, int8_t to_hour); /* 3b0a:1864 */
void ai_choose_films(void); /* 3b0a:190e, med */
void ai_buy_films(void); /* 3b0a:1c47, med */
void ai_auction_bid(int16_t limit); /* 3b0a:1ef1, low */
void ai_auction_overbid(void); /* 3b0a:2194, low */
void ai_budget_plan(int8_t player); /* 3b0a:24cc, med */
int8_t ai_eval_ratings(int8_t mode, int8_t player); /* 3b0a:2900, low */
void ai_sell_programme(int16_t genre, int16_t idx); /* 3b0a:2a7f */
void ai_sell_old_programmes(void); /* 3b0a:2bb9, med */
int16_t series_avg_quality(int16_t series_id); /* 3b0a:2dbc */
void ai_choose_ad_contracts(void); /* 3b0a:2e4d, med */
int8_t ai_can_take_contract(int16_t advertiser_id); /* 3b0a:30bd, med */
int8_t archive_total_length(int8_t player); /* 3b0a:323f, med */
int16_t ai_genre_total_length(int8_t genre); /* 3b0a:32f0, med */
int8_t ai_ad_spots_per_day(int16_t contract_idx, int8_t player); /* 3b0a:3410 */
int8_t ai_ad_target_ok(int16_t hour_or_prog, int16_t advertiser_id, int8_t is_prog); /* 3b0a:34c6 */
int8_t ai_check_arrival(void); /* 3b0a:361d, med */
void ai_goto_next(void); /* 3b0a:3854, med */
void ai_switch_player(void); /* 3b0a:3cb4 */
void ai_figure_goto(int16_t figure, int16_t room_type, int16_t owner, int16_t sub); /* 3b0a:3cc5 */
void ai_choose_studio_action(int8_t player); /* 3b0a:3d84, med */
int8_t production_avg_quality(int16_t prod_id); /* 3b0a:3ea3 */
int16_t ai_unfinished_avg_quality(int8_t player); /* 3b0a:3f21 */
void ai_day_plan(void); /* 3b0a:3f93 */
uint16_t ai_schedule_live(void); /* 3b0a:4219, low */
void ai_state_news_event(void); /* 3b0a:4344, low */
int8_t ai_check_can_act(void); /* 3b0a:462c */
void ai_state_show_event(void); /* 3b0a:46aa, low */
void ai_update_avg_income(int8_t player); /* 3b0a:4839 */

/* ---- Segment 3f95 ---- */
int32_t ai_calc_bid(int16_t ai_idx, int16_t x, int16_t y); /* 3f95:0000, med */
void ai_buy_news(void); /* 3f95:0104, low */
int16_t ai_news_quality_ok(int16_t player, int16_t threshold); /* 3f95:0332, low */
int32_t ai_rate_slot(int16_t hour, int16_t prog_id, int8_t mode); /* 3f95:037d */
int16_t ai_genre_share(int16_t genre, int16_t hour); /* 3f95:0a77, med */
void ai_schedule_ads(void); /* 3f95:0ab8, med */
void ai_place_ad_spots(int16_t player, int16_t advertiser_id, int16_t from_hour, int16_t to_hour, int16_t mode, int16_t contract_idx); /* 3f95:0f3d */
void ai_choose_gift(void); /* 3f95:1131, low */
void ai_give_gift(void); /* 3f95:12a1, low */
void ai_schedule_programmes(void); /* 3f95:1390, med */
void ai_pick_destination(void); /* 3f95:174b, low */
void find_event_door(int16_t *floor, int16_t *door); /* 3f95:19f7, low */
void building_find_door(uint16_t room_type, uint16_t owner, uint16_t sub, int16_t *floor, int16_t *door); /* 3f95:1b31 */

/* ---- Segment 4151 ---- */
int8_t ai_count_short_programmes(void); /* 4151:0002, med */
int8_t ai_find_free_slots(int8_t *out_hours, int8_t min_len); /* 4151:010c */
int8_t ai_ads_all_satisfied(void); /* 4151:01e0, med */
void ai_update_ad_contracts(void); /* 4151:0276, med */
void ai_adjust(int16_t player, int32_t amount, int16_t contract_idx); /* 4151:0480 */
void ai_init_new_game(void); /* 4151:0553 */
void ai_ad_spot_count(int16_t hour, int16_t contract_idx, int8_t add, int8_t player); /* 4151:06a8 */
void ai_ad_contract_init(int16_t contract_idx); /* 4151:0786 */
int16_t ai_archive_avg_quality(int8_t player); /* 4151:080a, med */
void ai_rent_studio(void); /* 4151:08e8, med */
void ai_buy_production(void); /* 4151:09fd, med */
void ai_produce_episode(void); /* 4151:0d1a, med */
void ai_prepare_episode(void); /* 4151:0e28, med */
void ai_cast_episode(void); /* 4151:0f75, low */

/* ---- Segment 42a0 ---- */
void bit_set(uint16_t *w, int16_t bit); /* 42a0:0001 */
void bit_clear(uint16_t *w, int16_t bit); /* 42a0:0016 */
int16_t bit_test(uint16_t w, int16_t bit); /* 42a0:002d */

/* ---- Segment 42a5 ---- */
void snd_init(void); /* 42a5:0002 */
void music_load_play(int16_t hes_no); /* 42a5:01a8 */
void music_free(void); /* 42a5:027f */
void music_start(void); /* 42a5:028f */
void music_stop(void); /* 42a5:02a8 */
void snd_shutdown(void); /* 42a5:02c0 */
void music_on_enter(int16_t room, int16_t owner_player); /* 42a5:02c6 */
void music_on_leave(int16_t room, int16_t owner_player); /* 42a5:04dd */
void music_switch_apply(void); /* PORT: Schalter Musik sofort anwenden */
void *para_align(void *p); /* 42a5:0582 */

/* ---- Segment 4301 ---- */
void *intro_load_frame(int16_t hes_no); /* 4301:0001 */
void intro_scene_setup(void); /* 4301:0090 */
void intro_frame_step(void); /* 4301:01f7 */
void intro_tick(void); /* 4301:0212 */
void intro_run_scene(void); /* 4301:0361 */
void intro_play(void); /* 4301:03ad */

#endif
