#include <pebble.h>

// ============================================================================
// COLORS
// ============================================================================

#ifdef PBL_COLOR
  #define COLOR_BG          GColorBlack
  #define COLOR_HOUR_GLOW   GColorPictonBlue
  #define COLOR_HOUR_CORE   GColorWhite
  #define COLOR_MIN_GLOW    GColorRed
  #define COLOR_MIN_CORE    GColorWhite
  #define COLOR_SEC_HAND    GColorLightGray
  #define COLOR_HILT        GColorLightGray
  #define COLOR_HILT_DARK   GColorDarkGray
  #define COLOR_CENTER      GColorWhite
  #define COLOR_DATE_BG     GColorWhite
  #define COLOR_DATE_TEXT   GColorBlack
  #define COLOR_DATE_BORDER GColorBlack
#else
  #define COLOR_BG          GColorBlack
  #define COLOR_HOUR_GLOW   GColorWhite
  #define COLOR_HOUR_CORE   GColorBlack
  #define COLOR_MIN_GLOW    GColorWhite
  #define COLOR_MIN_CORE    GColorBlack
  #define COLOR_SEC_HAND    GColorWhite
  #define COLOR_HILT        GColorWhite
  #define COLOR_HILT_DARK   GColorDarkGray
  #define COLOR_CENTER      GColorWhite
  #define COLOR_DATE_BG     GColorWhite
  #define COLOR_DATE_TEXT   GColorBlack
  #define COLOR_DATE_BORDER GColorBlack
#endif

// ============================================================================
// GEOMETRY
// Bitmap: 200x228 — exactly matches emery screen, drawn at y=0 (no offset)
// Clock center estimated from image center-hole position
// ============================================================================

#define CLOCK_CX     99
#define CLOCK_CY    110
#define DIAL_Y        0   // image is already 200x228, no vertical offset
#define HOUR_LEN     57
#define MIN_LEN      80
#define SEC_LEN      88
#define SEC_TAIL     12
// Date box: compact, centered in right triangular area (52x15px)
// x=114-166, y=102-117 — shifted 1px left with full face
#define DATE_BOX_X    114
#define DATE_BOX_Y    102
#define DATE_BOX_W     52
#define DATE_BOX_H     15
#define DATE_DIV_X    143  // divider: left cell 28px (115-142), right 22px (144-165)
// "PEBBLE" and "Emery" are both baked into the bitmap (no C overrides needed)

// ============================================================================
// GLOBAL STATE
// ============================================================================

static Window  *s_window;
static Layer   *s_canvas_layer;
static GPoint   s_center;
static struct tm s_now;
static char     s_day_buf[4];
static char     s_date_buf[3];

static GBitmap *s_dial_bmp = NULL;

static GPath *s_hour_blade = NULL;
static GPath *s_min_blade  = NULL;
static GPath *s_hour_hilt  = NULL;
static GPath *s_min_hilt   = NULL;

// Tapered blade: 6px wide at base, 2px at tip
static GPoint s_hour_blade_pts[] = { {-3, 5}, {3, 5}, {0, -HOUR_LEN} };
static GPathInfo s_hour_blade_info = { .num_points = 3, .points = s_hour_blade_pts };

static GPoint s_min_blade_pts[] = { {-3, 5}, {3, 5}, {0, -MIN_LEN} };
static GPathInfo s_min_blade_info = { .num_points = 3, .points = s_min_blade_pts };

static GPoint s_hour_hilt_pts[] = { {-4, 5}, {4, 5}, {4, 16}, {-4, 16} };
static GPathInfo s_hour_hilt_info = { .num_points = 4, .points = s_hour_hilt_pts };

static GPoint s_min_hilt_pts[] = { {-4, 5}, {4, 5}, {4, 16}, {-4, 16} };
static GPathInfo s_min_hilt_info = { .num_points = 4, .points = s_min_hilt_pts };

// ============================================================================
// BACKGROUND — dial face bitmap
// ============================================================================

static void draw_background(GContext *ctx) {
    if (s_dial_bmp) {
        graphics_context_set_compositing_mode(ctx, GCompOpAssign);
        graphics_draw_bitmap_in_rect(ctx, s_dial_bmp, GRect(0, 0, 200, 228));
    }
}

// ============================================================================
// DATE — overrides static "MON 7" in bitmap with live day/date
// ============================================================================

static void draw_date(GContext *ctx) {
    GFont font_day = fonts_get_system_font(FONT_KEY_GOTHIC_09);
    GFont font_num = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);

    // Fill box white to cover any hand that crossed through
    graphics_context_set_fill_color(ctx, COLOR_DATE_BG);
    graphics_fill_rect(ctx, GRect(DATE_BOX_X, DATE_BOX_Y, DATE_BOX_W, DATE_BOX_H), 0, GCornerNone);

    // Redraw border and divider (bitmap box is the reference; C keeps it fresh)
    graphics_context_set_stroke_color(ctx, COLOR_DATE_BORDER);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_rect(ctx, GRect(DATE_BOX_X, DATE_BOX_Y, DATE_BOX_W, DATE_BOX_H));
    graphics_draw_line(ctx,
        GPoint(DATE_DIV_X, DATE_BOX_Y + 1),
        GPoint(DATE_DIV_X, DATE_BOX_Y + DATE_BOX_H - 1));

    // Day in left cell (28px wide), nudged up 1px
    graphics_context_set_text_color(ctx, COLOR_DATE_TEXT);
    graphics_draw_text(ctx, s_day_buf, font_day,
        GRect(DATE_BOX_X + 1, DATE_BOX_Y + 2, 28, 9),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    // Date number in right cell (22px wide), nudged up 1px
    graphics_draw_text(ctx, s_date_buf, font_num,
        GRect(DATE_DIV_X + 1, DATE_BOX_Y - 1, 22, DATE_BOX_H),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

// ============================================================================
// HANDS — lightsaber glow + white core, hilts (no second hand)
// ============================================================================

static void draw_hands(GContext *ctx) {
    int32_t hour_ang = ((s_now.tm_hour % 12) * TRIG_MAX_ANGLE / 12)
                     + (s_now.tm_min * TRIG_MAX_ANGLE / 12 / 60);
    int32_t min_ang  = (s_now.tm_min * TRIG_MAX_ANGLE / 60);

    int16_t htx = (int16_t)(s_center.x + (sin_lookup(hour_ang) * HOUR_LEN) / TRIG_MAX_RATIO);
    int16_t hty = (int16_t)(s_center.y - (cos_lookup(hour_ang) * HOUR_LEN) / TRIG_MAX_RATIO);
    int16_t mtx = (int16_t)(s_center.x + (sin_lookup(min_ang)  * MIN_LEN)  / TRIG_MAX_RATIO);
    int16_t mty = (int16_t)(s_center.y - (cos_lookup(min_ang)  * MIN_LEN)  / TRIG_MAX_RATIO);

    // Hour — glow line → tapered blade → white core
    graphics_context_set_stroke_color(ctx, COLOR_HOUR_GLOW);
    graphics_context_set_stroke_width(ctx, 7);
    graphics_context_set_antialiased(ctx, true);
    graphics_draw_line(ctx, s_center, GPoint(htx, hty));

    gpath_rotate_to(s_hour_blade, hour_ang);
    gpath_move_to(s_hour_blade, s_center);
    graphics_context_set_fill_color(ctx, COLOR_HOUR_GLOW);
    gpath_draw_filled(ctx, s_hour_blade);
    // Thick white core drawn last so it fully covers the colored interior
    graphics_context_set_stroke_color(ctx, COLOR_HOUR_CORE);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, s_center, GPoint(htx, hty));

    // Minute — glow line → tapered blade → white core
    graphics_context_set_stroke_color(ctx, COLOR_MIN_GLOW);
    graphics_context_set_stroke_width(ctx, 7);
    graphics_draw_line(ctx, s_center, GPoint(mtx, mty));

    gpath_rotate_to(s_min_blade, min_ang);
    gpath_move_to(s_min_blade, s_center);
    graphics_context_set_fill_color(ctx, COLOR_MIN_GLOW);
    gpath_draw_filled(ctx, s_min_blade);
    // Thick white core drawn last so it fully covers the colored interior
    graphics_context_set_stroke_color(ctx, COLOR_MIN_CORE);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, s_center, GPoint(mtx, mty));

    // Hilts
    gpath_rotate_to(s_hour_hilt, hour_ang);
    gpath_move_to(s_hour_hilt, s_center);
    graphics_context_set_fill_color(ctx, COLOR_HILT);
    gpath_draw_filled(ctx, s_hour_hilt);
    graphics_context_set_stroke_color(ctx, COLOR_HILT_DARK);
    graphics_context_set_stroke_width(ctx, 1);
    gpath_draw_outline(ctx, s_hour_hilt);

    gpath_rotate_to(s_min_hilt, min_ang);
    gpath_move_to(s_min_hilt, s_center);
    graphics_context_set_fill_color(ctx, COLOR_HILT);
    gpath_draw_filled(ctx, s_min_hilt);
    graphics_context_set_stroke_color(ctx, COLOR_HILT_DARK);
    gpath_draw_outline(ctx, s_min_hilt);

    // Center dot
    graphics_context_set_fill_color(ctx, COLOR_CENTER);
    graphics_fill_circle(ctx, s_center, 4);
    graphics_context_set_fill_color(ctx, COLOR_BG);
    graphics_fill_circle(ctx, s_center, 2);
}

// ============================================================================
// MAIN CANVAS
// ============================================================================

static void canvas_update_proc(Layer *layer, GContext *ctx) {
    draw_background(ctx);   // dial face bitmap (PEBBLE + Emery baked in)
    draw_date(ctx);         // date box drawn before hands
    draw_hands(ctx);        // hands drawn last — always on top of date box
}

// ============================================================================
// TIME HANDLING  (SECOND_UNIT for second hand)
// ============================================================================

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    s_now = *tick_time;
    if (units_changed & MINUTE_UNIT) {
        strftime(s_day_buf, sizeof(s_day_buf), "%a", tick_time);
        for (int i = 0; s_day_buf[i]; i++) {
            if (s_day_buf[i] >= 'a' && s_day_buf[i] <= 'z') s_day_buf[i] -= 32;
        }
        snprintf(s_date_buf, sizeof(s_date_buf), "%d", tick_time->tm_mday);
    }
    if (s_canvas_layer) layer_mark_dirty(s_canvas_layer);
}

// ============================================================================
// WINDOW HANDLERS
// ============================================================================

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    s_center = GPoint(CLOCK_CX, CLOCK_CY);

    s_canvas_layer = layer_create(bounds);
    layer_set_update_proc(s_canvas_layer, canvas_update_proc);
    layer_add_child(window_layer, s_canvas_layer);

    s_dial_bmp   = gbitmap_create_with_resource(RESOURCE_ID_DIAL_FACE);
    s_hour_blade = gpath_create(&s_hour_blade_info);
    s_min_blade  = gpath_create(&s_min_blade_info);
    s_hour_hilt  = gpath_create(&s_hour_hilt_info);
    s_min_hilt   = gpath_create(&s_min_hilt_info);

    time_t t = time(NULL);
    struct tm *now = localtime(&t);
    if (now) tick_handler(now, MINUTE_UNIT);
}

static void window_unload(Window *window) {
    layer_destroy(s_canvas_layer);
    s_canvas_layer = NULL;
    if (s_dial_bmp)   { gbitmap_destroy(s_dial_bmp);   s_dial_bmp   = NULL; }
    if (s_hour_blade) { gpath_destroy(s_hour_blade);  s_hour_blade = NULL; }
    if (s_min_blade)  { gpath_destroy(s_min_blade);   s_min_blade  = NULL; }
    if (s_hour_hilt)  { gpath_destroy(s_hour_hilt);   s_hour_hilt  = NULL; }
    if (s_min_hilt)   { gpath_destroy(s_min_hilt);    s_min_hilt   = NULL; }
}

// ============================================================================
// INIT / DEINIT / MAIN
// ============================================================================

static void init(void) {
    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);
    window_set_window_handlers(s_window, (WindowHandlers){
        .load   = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_window, true);
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}
