#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <math.h>

#include "nano-X.h"

#define SCREEN_W 320
#define SCREEN_H 240
#define CENTER_X 160
#define CENTER_Y 120
#define SEGMENTS 60
#define FACE_STACK 0
#define FACE_FLOW 1
#define FACE_FLUX 2
#define FACE_INFO 3
#define FACE_COUNT 4

static const int ring_radius[] = { 108, 96, 84 };
static const GR_COLOR flow_colors[] = {
	GR_RGB(255, 76, 105), GR_RGB(91, 235, 151), GR_RGB(73, 190, 255)
};
static const GR_COLOR flux_colors[] = {
	GR_RGB(255, 102, 56), GR_RGB(211, 81, 255), GR_RGB(62, 195, 255)
};
static const GR_COLOR track_colors[] = {
	GR_RGB(35, 24, 34), GR_RGB(20, 36, 31), GR_RGB(19, 31, 43)
};
static const GR_COLOR face_colors[] = {
	GR_RGB(3, 5, 10), GR_RGB(4, 6, 12),
	GR_RGB(5, 5, 13), GR_RGB(4, 6, 12)
};
static const GR_COLOR center_colors[] = {
	GR_RGB(3, 5, 10), GR_RGB(9, 12, 20),
	GR_RGB(11, 9, 22), GR_RGB(4, 6, 12)
};

static GR_WINDOW_ID window;
static GR_GC_ID gc;
static GR_FONT_ID detail_font;
static volatile sig_atomic_t face_change_requested;
static int face_index = FACE_STACK;
static int last_ring_index[3] = { -1, -1, -1 };
static int last_second = -1;
static int last_minute = -1;
static int last_hour = -1;
static int last_day = -1;
static int last_month = -1;
static int last_year = -1;
static int last_stack_hour = -1;
static int last_stack_minute = -1;
static int last_stack_second = -1;
static GR_SIZE screen_width = SCREEN_W;
static GR_SIZE screen_height = SCREEN_H;

static const unsigned char digit_segments[] = {
	0x3f, 0x06, 0x5b, 0x4f, 0x66,
	0x6d, 0x7d, 0x07, 0x7f, 0x6f
};

static void draw_text_field(const char *text, GR_COORD left, GR_COORD top,
			    GR_SIZE width, GR_SIZE height, GR_COORD baseline,
			    GR_FONT_ID font, GR_COLOR color);

static void
request_next_face(int signal_number)
{
	(void)signal_number;
	face_change_requested = 1;
}

static GR_COLOR
face_color(int ring)
{
	if (face_index == FACE_FLUX)
		return flux_colors[ring];
	return flow_colors[ring];
}

static int
format_uptime(char *text, size_t text_size)
{
	FILE *file;
	double uptime;
	int days, hours, minutes, parsed, written;

	file = fopen("/proc/uptime", "r");
	if (file == NULL)
		return -1;
	parsed = fscanf(file, "%lf", &uptime);
	if (fclose(file) != 0 || parsed != 1)
		return -1;

	days = (int)uptime / 86400;
	hours = ((int)uptime / 3600) % 24;
	minutes = ((int)uptime / 60) % 60;
	written = snprintf(text, text_size, "UPTIME  %dd %02dh %02dm",
			   days, hours, minutes);
	if (written < 0 || (size_t)written >= text_size)
		return -1;
	return 0;
}

static void
draw_uptime(GR_COORD top, GR_COORD baseline)
{
	char text[64];

	if (format_uptime(text, sizeof(text)) != 0)
		sprintf(text, "UPTIME  unavailable");
	draw_text_field(text, 50, top, 220, 20, baseline,
			detail_font, GR_RGB(110, 220, 177));
}

static void
draw_centered(const char *text, GR_COORD baseline, GR_FONT_ID font,
	      GR_COLOR color)
{
	GR_SIZE width, height, text_baseline;

	GrSetGCForeground(gc, color);
	GrSetGCFont(gc, font);
	GrGetGCTextSize(gc, (void *)text, -1, GR_TFBASELINE,
			&width, &height, &text_baseline);
	GrText(window, gc, (screen_width - width) / 2, baseline, (void *)text, -1,
	       GR_TFBASELINE);
}

static void
draw_text_field(const char *text, GR_COORD left, GR_COORD top,
		GR_SIZE width, GR_SIZE height, GR_COORD baseline,
		GR_FONT_ID font, GR_COLOR color)
{
	GrSetGCForeground(gc, center_colors[face_index]);
	GrFillRect(window, gc, left, top, width, height);
	draw_centered(text, baseline, font, color);
}

static void
draw_ring_dot(int ring, int segment, int active)
{
	double angle;
	GR_COORD x, y;

	segment %= SEGMENTS;
	if (segment < 0)
		segment += SEGMENTS;
	angle = (2.0 * 3.14159265358979323846 * segment / SEGMENTS) -
		3.14159265358979323846 / 2.0;
	x = CENTER_X + (GR_COORD)(cos(angle) * ring_radius[ring]);
	y = CENTER_Y + (GR_COORD)(sin(angle) * ring_radius[ring]);

	GrSetGCForeground(gc, active ? face_color(ring) : track_colors[ring]);
	GrFillEllipse(window, gc, x, y, 2, 2);
}

static void
draw_digit(int x, int y, int digit)
{
	int unit = (screen_height - 48) / 90;
	int segment_x[] = { 5, 32, 32, 5, 0, 0, 5 };
	int segment_y[] = { 0, 4, 20, 32, 20, 4, 16 };
	int segment_w[] = { 25, 4, 4, 25, 4, 4, 25 };
	int segment_h[] = { 4, 12, 12, 4, 12, 12, 4 };
	GR_COLOR accent = face_index == FACE_FLUX ?
		GR_RGB(255, 144, 75) : GR_RGB(86, 208, 255);
	int segment;

	for (segment = 0; segment < 7; segment++) {
		int sx = x + segment_x[segment] * unit;
		int sy = y + segment_y[segment] * unit;
		int sw = segment_w[segment] * unit;
		int sh = segment_h[segment] * unit;

		GrSetGCForeground(gc, GR_RGB(23, 28, 38));
		GrFillRect(window, gc, sx, sy, sw, sh);
		GrSetGCForeground(gc, accent);
		GrRect(window, gc, sx, sy, sw, sh);
		if (digit_segments[digit] & (1 << segment)) {
			GrSetGCForeground(gc, GR_RGB(242, 248, 255));
			GrFillRect(window, gc, sx + unit, sy + unit,
				   sw - 2 * unit, sh - 2 * unit);
		}
	}
}

static void
draw_stacked_time(const struct tm *local, int full)
{
	int hour = local->tm_hour;
	int minute = local->tm_min;
	GR_COLOR accent = face_index == FACE_FLUX ?
		GR_RGB(255, 144, 75) : GR_RGB(86, 208, 255);
	int unit = (screen_height - 48) / 90;
	int digit_width = 37 * unit;
	int digit_height = 36 * unit;
	int gap = 3 * unit;
	int x1 = (screen_width - (2 * digit_width + gap)) / 2;
	int hour_y = (screen_height - 2 * digit_height - 18) / 2;
	int minute_y = hour_y + digit_height + 18;
	int second;

	if (full) {
		GrSetGCForeground(gc, face_colors[face_index]);
		GrFillRect(window, gc, 0, 0, screen_width, screen_height);
		draw_centered("LOCAL TIME", hour_y - 5, detail_font,
			      GR_RGB(169, 182, 204));
		last_stack_hour = -1;
		last_stack_minute = -1;
		last_stack_second = -1;
	}
	if (full || hour != last_stack_hour) {
		GrSetGCForeground(gc, face_colors[face_index]);
		GrFillRect(window, gc, x1, hour_y, 2 * digit_width + gap,
			   digit_height);
		draw_digit(x1, hour_y, hour / 10);
		draw_digit(x1 + digit_width + gap, hour_y, hour % 10);
		last_stack_hour = hour;
	}
	if (full || minute != last_stack_minute) {
		GrSetGCForeground(gc, face_colors[face_index]);
		GrFillRect(window, gc, x1, minute_y, 2 * digit_width + gap,
			   digit_height);
		draw_digit(x1, minute_y, minute / 10);
		draw_digit(x1 + digit_width + gap, minute_y, minute % 10);
		last_stack_minute = minute;
	}

	second = local->tm_sec;
	if (full) {
		int ticks = screen_width * 24 / 320;
		int tick_gap = 2;
		int tick_width = (ticks - 11 * tick_gap) / 12;
		int tick_x = (screen_width - ticks) / 2;
		int tick_y = screen_height - 18;
		for (minute = 0; minute < 12; minute++) {
			GrSetGCForeground(gc, minute <= second / 5 ?
					  accent : GR_RGB(35, 38, 48));
			GrFillRect(window, gc,
				   tick_x + minute * (tick_width + tick_gap),
				   tick_y, tick_width, 5);
		}
	} else if (second / 5 != last_stack_second / 5) {
		int ticks = screen_width * 24 / 320;
		int tick_gap = 2;
		int tick_width = (ticks - 11 * tick_gap) / 12;
		int tick_x = (screen_width - ticks) / 2;
		int tick_y = screen_height - 18;
		if (second < last_stack_second) {
			for (minute = 0; minute < 12; minute++) {
				GrSetGCForeground(gc, GR_RGB(35, 38, 48));
				GrFillRect(window, gc,
					   tick_x + minute * (tick_width + tick_gap),
					   tick_y, tick_width, 5);
			}
			minute = 0;
		} else {
			minute = last_stack_second / 5 + 1;
		}
		for (; minute <= second / 5; minute++) {
			GrSetGCForeground(gc, accent);
			GrFillRect(window, gc,
				   tick_x + minute * (tick_width + tick_gap),
				   tick_y, tick_width, 5);
		}
	}
	last_stack_second = second;
}

static void
draw_info_page(const struct tm *local)
{
	char value[64];
	char date_text[32];

	GrSetGCForeground(gc, face_colors[FACE_INFO]);
	GrFillRect(window, gc, 0, 0, screen_width, screen_height);
	draw_centered("DEVICE INFO", 33, detail_font,
		      GR_RGB(91, 190, 255));
	draw_centered("TOMTOM ONE  /  V6", 60, detail_font,
		      GR_RGB(225, 234, 244));
	strftime(value, sizeof(value), "%H:%M", local);
	draw_centered(value, 104, detail_font, GR_RGB(245, 249, 255));
	strftime(date_text, sizeof(date_text), "%a  %d %b %Y", local);
	draw_centered(date_text, 130, detail_font,
		      GR_RGB(173, 190, 211));
	draw_uptime(151, 166);
	draw_centered("DEVICE STATUS", 210,
		      detail_font, GR_RGB(126, 142, 163));
}

static void
draw_ring_progress(int ring, int index)
{
	int segment;

	for (segment = 0; segment < SEGMENTS; segment++)
		draw_ring_dot(ring, segment, segment <= index);
	last_ring_index[ring] = index;
}

static void
advance_ring(int ring, int index)
{
	int segment;
	int previous = last_ring_index[ring];

	if (previous == index)
		return;
	if (previous < 0 || index < previous) {
		draw_ring_progress(ring, index);
		return;
	}
	for (segment = previous + 1; segment <= index; segment++)
		draw_ring_dot(ring, segment, 1);
	last_ring_index[ring] = index;
}

static int
hour_progress(const struct tm *local)
{
	return ((local->tm_hour % 12) * 60 + local->tm_min) / 12;
}

static void
draw_tracks(void)
{
	int ring, segment;

	for (ring = 0; ring < 3; ring++) {
		for (segment = 0; segment < SEGMENTS; segment++)
			draw_ring_dot(ring, segment, 0);
	}
}

static void
draw_face(const struct tm *local)
{
	char time_text[8];
	char seconds_text[4];
	char date_text[32];
	int progress[3];

	GrSetGCForeground(gc, face_colors[face_index]);
	GrFillRect(window, gc, 0, 0, screen_width, screen_height);
	last_ring_index[0] = last_ring_index[1] = last_ring_index[2] = -1;
	last_stack_hour = last_stack_minute = last_stack_second = -1;

	if (face_index == FACE_STACK) {
		draw_stacked_time(local, 1);
	} else if (face_index == FACE_INFO) {
		draw_info_page(local);
	} else {
		draw_tracks();
		if (face_index == FACE_FLOW) {
			GrSetGCForeground(gc, GR_RGB(9, 12, 20));
			GrFillEllipse(window, gc, CENTER_X, CENTER_Y, 73, 73);
			draw_centered("FLOW", 70, detail_font,
				      GR_RGB(154, 165, 185));
		} else {
			GrSetGCForeground(gc, GR_RGB(11, 9, 22));
			GrFillEllipse(window, gc, CENTER_X, CENTER_Y, 73, 73);
			draw_centered("FLUX", 70, detail_font,
				      GR_RGB(195, 166, 220));
		}
	}

	progress[0] = local->tm_sec;
	progress[1] = local->tm_min;
	progress[2] = hour_progress(local);
	if (face_index == FACE_FLOW || face_index == FACE_FLUX) {
		draw_ring_progress(0, progress[0]);
		draw_ring_progress(1, progress[1]);
		draw_ring_progress(2, progress[2]);
	}

	strftime(time_text, sizeof(time_text), "%H:%M", local);
	strftime(seconds_text, sizeof(seconds_text), "%S", local);
	strftime(date_text, sizeof(date_text), "%a  %d %b", local);
	if (face_index == FACE_FLOW || face_index == FACE_FLUX) {
		draw_centered(date_text, 94, detail_font,
			      GR_RGB(205, 213, 228));
		draw_centered(time_text, 137, detail_font,
			      GR_RGB(249, 250, 255));
		if (face_index == FACE_FLOW)
			draw_centered(seconds_text, 161, detail_font,
				      GR_RGB(169, 181, 202));
	}

	last_second = local->tm_sec;
	last_minute = local->tm_min;
	last_hour = local->tm_hour;
	last_day = local->tm_mday;
	last_month = local->tm_mon;
	last_year = local->tm_year;
}

static void
update_face(const struct tm *local)
{
	char time_text[8];
	char seconds_text[4];
	char date_text[32];

	if (face_index == FACE_STACK) {
		draw_stacked_time(local, 0);
	} else if (face_index == FACE_INFO) {
		if (local->tm_min != last_minute ||
		    local->tm_hour != last_hour ||
		    local->tm_mday != last_day ||
		    local->tm_mon != last_month ||
		    local->tm_year != last_year) {
			char value[64];
			char date_text[32];

			if (local->tm_hour != last_hour ||
			    local->tm_min != last_minute) {
				strftime(value, sizeof(value), "%H:%M", local);
				draw_text_field(value, 83, 83, 154, 22, 104,
						detail_font, GR_RGB(245, 249, 255));
				draw_uptime(151, 166);
			}
			if (local->tm_mday != last_day ||
			    local->tm_mon != last_month ||
			    local->tm_year != last_year) {
				strftime(date_text, sizeof(date_text),
					 "%a  %d %b %Y", local);
				draw_text_field(date_text, 72, 108, 176, 27,
						130, detail_font,
						GR_RGB(173, 190, 211));
			}
		}
	} else {
		advance_ring(0, local->tm_sec);
		advance_ring(1, local->tm_min);
		advance_ring(2, hour_progress(local));
	}

	if ((face_index == FACE_FLOW || face_index == FACE_FLUX) &&
	    (local->tm_hour != last_hour || local->tm_min != last_minute)) {
		strftime(time_text, sizeof(time_text), "%H:%M", local);
		draw_text_field(time_text, 97, 121, 126, 24, 137,
				detail_font, GR_RGB(249, 250, 255));
	}

	if (face_index == FACE_FLOW && local->tm_sec != last_second) {
		strftime(seconds_text, sizeof(seconds_text), "%S", local);
		draw_text_field(seconds_text, 130, 146, 60, 20, 161,
				detail_font, GR_RGB(169, 181, 202));
	}

	if ((face_index == FACE_FLOW || face_index == FACE_FLUX) &&
	    (local->tm_mday != last_day || local->tm_mon != last_month ||
	     local->tm_year != last_year)) {
		strftime(date_text, sizeof(date_text), "%a  %d %b", local);
		draw_text_field(date_text, 91, 77, 138, 23, 94,
				detail_font, GR_RGB(205, 213, 228));
	}

	last_second = local->tm_sec;
	last_minute = local->tm_min;
	last_hour = local->tm_hour;
	last_day = local->tm_mday;
	last_month = local->tm_mon;
	last_year = local->tm_year;
}

int
main(void)
{
	GR_EVENT event;

	if (GrOpen() < 0) {
		fprintf(stderr, "cannot open Nano-X graphics\n");
		return 1;
	}

	signal(SIGUSR1, request_next_face);
	window = GrNewWindowEx(GR_WM_PROPS_NODECORATE |
			       GR_WM_PROPS_NOAUTOMOVE |
			       GR_WM_PROPS_NOAUTORESIZE |
			       GR_WM_PROPS_NORESIZE,
			       "Flow Watch Faces", GR_ROOT_WINDOW_ID,
			       0, 0, SCREEN_W, SCREEN_H, face_colors[0]);
	gc = GrNewGC();
	detail_font = GrCreateFontEx(GR_FONT_SYSTEM_FIXED, 14, 0, NULL);
	if (window == 0 || gc == 0 || detail_font == 0) {
		fprintf(stderr, "cannot create watch face resources\n");
		GrClose();
		return 1;
	}

	{
		GR_WINDOW_INFO info;
		GrGetWindowInfo(window, &info);
		if (info.width > 0 && info.height > 0) {
			screen_width = info.width;
			screen_height = info.height;
		}
	}
	GrSelectEvents(window, GR_EVENT_MASK_EXPOSURE |
		       GR_EVENT_MASK_BUTTON_DOWN |
		       GR_EVENT_MASK_CLOSE_REQ);
	GrMapWindow(window);

	for (;;) {
		time_t now;
		struct tm *local;
		GR_WINDOW_INFO info;

		GrGetNextEventTimeout(&event, 1000L);
		if (event.type == GR_EVENT_TYPE_CLOSE_REQ) {
			GrClose();
			return 0;
		}
		if (event.type == GR_EVENT_TYPE_BUTTON_DOWN) {
			face_index = (face_index + 1) % FACE_COUNT;
			last_second = -1;
		}
		if (event.type == GR_EVENT_TYPE_EXPOSURE) {
			GrGetWindowInfo(window, &info);
			if (info.width > 0 && info.height > 0) {
				screen_width = info.width;
				screen_height = info.height;
			}
			last_second = -1;
		}
		if (face_change_requested) {
			face_change_requested = 0;
			face_index = (face_index + 1) % FACE_COUNT;
			last_second = -1;
		}
		if (event.type == GR_EVENT_TYPE_EXPOSURE)
			continue;
		now = time(NULL);
		local = localtime(&now);
		if (local == NULL)
			continue;
		if (last_second < 0)
			draw_face(local);
		else if (local->tm_sec != last_second ||
			 local->tm_min != last_minute ||
			 local->tm_hour != last_hour ||
			 local->tm_mday != last_day ||
			 local->tm_mon != last_month ||
			 local->tm_year != last_year)
			update_face(local);
	}
}
