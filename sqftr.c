#include <float.h>
#include <raylib.h>
#include <raymath.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#include "nob.h"
#define JIMP_IMPLEMENTATION
#include "jimp.h"

Color BG_COLOR = CLITERAL(Color){18, 18, 18, 255};
Color FG_COLOR = SKYBLUE;
Color FONT_COLOR = RAYWHITE;
const char *FONT_PATH = "./VinaSans-Regular.ttf";
int WIDTH = 1600;
int HEIGHT = 1000;
float FONT_SIZE = 18.0;
int PAD = 25;
int BUFFER = 400;
float SCALE = 15;
Vector2 UNINITIALIZED = (Vector2) { FLT_MIN, FLT_MIN };

typedef struct {
    char *name;
    float price;
} Flooring;

typedef struct {
    Flooring *items;
    size_t count;
    size_t capacity;
} FlooringOptions;

typedef enum { RIGHT, LEFT, UP, DOWN } Direction;

typedef struct {
    Direction dir;
    Vector2 start;
    Vector2 end;
} Segment;

typedef struct {
    Segment *items;
    size_t count;
    size_t capacity;
} Segments;

typedef struct {
    const char *name;
    Segments segments;
    Segments scaled;
    float area;
    float waste;
    char *flooring;
    Rectangle bb;
    Vector2 position;;
    Vector2 label_position;
} Room;

typedef struct {
    Room *items;
    size_t count;
    size_t capacity;
    float total_area;
    float total_waste;
    float total_price;
} Rooms;

Vector2 pt(float x, float y) { return (Vector2){.x = x, .y = y}; }

Direction parse_dir(char dir) {
    switch (dir) {
    case 'R':
        return RIGHT;
    case 'L':
        return LEFT;
    case 'U':
        return UP;
    case 'D':
        return DOWN;
    default:
        printf("Invalid Direction: %c", dir);
        exit(1);
    }
}

bool load_flooring(Jimp *jimp, FlooringOptions *flooring_opts) {
    if (!jimp_object_begin(jimp)) return false;

    while (jimp_object_member(jimp)) {
        Flooring fl = {0};
        fl.name = jimp->string;
        if (!jimp_number(jimp)) return false;
        fl.price = jimp->number;
        da_append(flooring_opts, fl);
    }

    if (!jimp_object_end(jimp)) return false;

    return true;
}

bool load_segments(Jimp *jimp, Segments *segments) {
    Vector2 start = {.x = 0, .y = 0};

    if (!jimp_array_begin(jimp)) return false;
    while (jimp_array_item(jimp)) {
        if (!jimp_string(jimp)) return false;
        char *dir_and_length = strdup(jimp->string);
        Direction dir = parse_dir(dir_and_length[0]);
        char *e;
        float length = strtof(dir_and_length + 1, &e);

        Segment segment = {.start = start};
        switch (dir) {
        case RIGHT:
            segment.end = pt(start.x + length, start.y);
            break;
        case LEFT:
            segment.end = pt(start.x - length, start.y);
            break;
        case UP:
            segment.end = pt(start.x, start.y - length);
            break;
        case DOWN:
            segment.end = pt(start.x, start.y + length);
            break;
        }
        start = segment.end;

        da_append(segments, segment);
    }

    if (!jimp_array_end(jimp)) return false;

    return true;
}

float calculate_segments_area(const Segments *segments) {
    if (segments == NULL || segments->count < 4 || segments->items == NULL) {
        return 0.0f;
    }

    float area = 0.0f;
    size_t n = segments->count;

    for (size_t i = 0; i < n; i++) {
        Vector2 current = segments->items[i].start;
        size_t nextIndex = (i + 1) % n;
        Vector2 next = segments->items[nextIndex].start;

        area += (current.x * next.y) - (next.x * current.y);
    }

    return fabsf(area) * 0.5f;
}

void scale_segments(Room *room)
{
    da_foreach(Segment, s, &room->segments) {
        Vector2 start = Vector2Scale(s->start, SCALE);
        Vector2 end = Vector2Scale(s->end, SCALE);
        Segment scaled =(Segment) { .start = start, .end = end, .dir = s->dir };
        da_append(&room->scaled, scaled);
    }
}

void calculate_bounding_box(Room *room) {
    float min_x = FLT_MAX;
    float max_x = FLT_MIN;
    float min_y = FLT_MAX;
    float max_y = FLT_MIN;
    scale_segments(room);
    da_foreach(Segment, s, &room->scaled) {
        if (s->start.x < min_x) min_x = s->start.x;
        if (s->start.x > max_x) max_x = s->start.x;
        if (s->start.y < min_y) min_y = s->start.y;
        if (s->start.y > max_y) max_y = s->start.y;
        if (s->end.x < min_x) min_x = s->end.x;
        if (s->end.x > max_x) max_x = s->end.x;
        if (s->end.y < min_y) min_y = s->end.y;
        if (s->end.y > max_y) max_y = s->end.y;
    }
    room->bb = (Rectangle){.x = min_x, .y = min_y, .width = max_x - min_x, .height = max_y - min_y};
}

bool load_room(Jimp *jimp, Room *rm) {
    if (!jimp_object_begin(jimp)) return false;

    while (jimp_object_member(jimp)) {
        if (strcmp(jimp->string, "name") == 0) {
            if (!jimp_string(jimp)) return false;
            rm->name = strdup(jimp->string);
            continue;
        } else if (strcmp(jimp->string, "segments") == 0) {
            if (!jimp_is_array_ahead(jimp)) return false;
            Segments segments = {0};
            load_segments(jimp, &segments);
            rm->segments = segments;
            rm->area = calculate_segments_area(&rm->segments);
            rm->waste = rm->area * 0.1f;
            calculate_bounding_box(rm);
            continue;
        } else if (strcmp(jimp->string, "flooring") == 0) {
            if (!jimp_string(jimp)) return false;
            // TODO: validate
            rm->flooring = jimp->string;
        }
    }

    if (!jimp_object_end(jimp)) return false;

    rm->position = UNINITIALIZED;
    rm->label_position = UNINITIALIZED;

    return true;
}

float calculate_price(FlooringOptions *fl_opts, Room *room) {
    da_foreach(Flooring, f, fl_opts) {
        if (strcmp(f->name, room->flooring) == 0) {
            return f->price * (room->area + room->waste);
        }
    }

    printf("Could not find flooring option for %s", room->flooring);
    exit(1);
}

bool load_rooms(Jimp *jimp, Rooms *rms, FlooringOptions *fl_opts) {
    if (!jimp_array_begin(jimp)) return false;

    while (jimp_array_item(jimp)) {
        Room room = {0};
        if (!load_room(jimp, &room)) return false;
        rms->total_area += room.area;
        rms->total_waste += room.waste;
        rms->total_price += calculate_price(fl_opts, &room);
        da_append(rms, room);
    }

    if (!jimp_array_end(jimp)) return false;

    return true;
}

bool init_sqftr(Jimp *jimp, FlooringOptions *fl_opts, Rooms *rms) {
    if (!jimp_object_begin(jimp)) return false;
    while (jimp_object_member(jimp)) {
        if (strcmp(jimp->string, "flooring") == 0) {
            if (!load_flooring(jimp, fl_opts)) return false;
        } else if (strcmp(jimp->string, "rooms") == 0) {
            if (!load_rooms(jimp, rms, fl_opts)) return false;
        } else {
            printf("Unsupported config option %s\n", jimp->string);
            return false;
        }
    }

    return true;
}

Vector2 get_centroid(const Segments *segments) {
    if (segments == NULL || segments->count == 0 || segments->items == NULL) {
        return (Vector2){0.0f, 0.0f};
    }

    float min_x = segments->items[0].start.x;
    float max_x = min_x;
    float min_y = segments->items[0].start.y;
    float max_y = min_y;

    for (size_t i = 1; i < segments->count; i++) {
        Vector2 p = segments->items[i].start;
        if (p.x < min_x) min_x = p.x;
        if (p.x > max_x) max_x = p.x;
        if (p.y < min_y) min_y = p.y;
        if (p.y > max_y) max_y = p.y;
    }

    Vector2 c = {.x = min_x + (max_x - min_x) * 0.5f, .y = min_y + (max_y - min_y) * 0.5f};

    return c;
}

void render_text(const char *text, Vector2 center, int font_size, Color font_color) {
    int textWidth = MeasureText(text, FONT_SIZE);
    float offsetX = textWidth * 0.5f;
    float offsetY = FONT_SIZE * 0.5f;

    Font font = LoadFont(FONT_PATH);
    DrawTextEx(font, text, (Vector2){(int)(center.x - offsetX), (int)(center.y - offsetY)}, font_size, 1, font_color);
}

void render_area(Room *room, Vector2 position) {
    if (room == NULL || room->segments.count == 0) return;

    Vector2 center = Vector2Add(Vector2Scale(get_centroid(&room->segments), SCALE), position);
    render_text(TextFormat("%s: %.0f", room->name, room->area), center, FONT_SIZE, FONT_COLOR);
}

void render_bb(Room *room) {
    DrawRectangleLines(room->position.x, room->position.y, room->bb.width, room->bb.height, YELLOW);
}

void draw_room(Room *room) {
    da_foreach(Segment, s, &room->scaled) {
        Vector2 start = Vector2Add(s->start, room->position);
        Vector2 end = Vector2Add(s->end, room->position);
        DrawLineEx(start, end, 3.0, FG_COLOR);
    }

    render_bb(room);
}

void render_areas(Rooms *rooms, size_t start, size_t end, Vector2 *tracking) {
    float y = tracking->y + PAD;
    for (size_t i = start; i < end; i++) {
        printf("drawing area for %s [%zu] at (%.2f,%.2f)\n", rooms->items[i].name, i,
               tracking->x + rooms->items[i].bb.height, y);
        DrawRectangleLines(1, y, PAD, PAD, RED);
    }
}

void draw_rooms(Rooms *rooms) {
    da_foreach(Room, r, rooms) {
        draw_room(r);
    }
}

Vector2 room_max(Room *room)
{
    if (Vector2Equals(room->position, UNINITIALIZED)) {
        printf("Room has not been laid out: %s", room->name);
        exit(1);
    }

    return (Vector2) { room->position.x + room->bb.width, room->position.y + room->bb.height };
}

void layout_rooms(Rooms *rooms)
{
    size_t room_idx = 0;
    float max_y = FLT_MIN, row_y = PAD, next_x = PAD;
    da_foreach(Room, r, rooms) {
        if (room_idx != 0) {
            next_x = room_max(&rooms->items[room_idx - 1]).x + PAD;
        }
        if (next_x + r->bb.width + PAD > WIDTH) {
            next_x = PAD;
            row_y += max_y + PAD;
        }
        r->position.x = next_x;
        r->position.y = row_y;
        float y = room_max(r).y;
        if (max_y < y) max_y = y;
        room_idx++;
    }
}

int main(void) {
    const char *file_path = "data/sqftr.json";

    String_Builder sb = {0};
    if (!read_entire_file(file_path, &sb)) return false;

    Jimp jimp = {0};
    jimp_begin(&jimp, file_path, sb.items, sb.count);
    Rooms rooms = {0};
    FlooringOptions fl_opts = {0};

    init_sqftr(&jimp, &fl_opts, &rooms);

    InitWindow(WIDTH, HEIGHT, "sqftr");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BG_COLOR);

        layout_rooms(&rooms);
        draw_rooms(&rooms);
        render_text(TextFormat("%d Rooms. Total sqf: %.2f (%.2f w/ waste)\nTotal Price: %.2f", rooms.count,
                               rooms.total_area, rooms.total_area + rooms.total_waste, rooms.total_price),
                    (Vector2){.x = 250, .y = HEIGHT - 250}, 48, LIME);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
