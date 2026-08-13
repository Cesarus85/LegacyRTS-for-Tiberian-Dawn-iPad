#ifndef IPADOS_LAYOUT_H
#define IPADOS_LAYOUT_H

struct IPadLayoutInsets
{
    int left;
    int top;
    int right;
    int bottom;
};

struct IPadViewport
{
    int x;
    int y;
    int width;
    int height;
};

struct IPadLayout
{
    int output_width;
    int output_height;
    int window_width;
    int window_height;
    int game_width;
    int game_height;
    IPadLayoutInsets safe_area;
    IPadViewport viewport;
    bool compact;
};

IPadLayout Calculate_IPad_Layout(int output_width,
                                 int output_height,
                                 int window_width,
                                 int window_height,
                                 int game_width,
                                 int game_height,
                                 IPadLayoutInsets safe_area,
                                 bool integer_scaling);

void Map_IPad_Output_Point(const IPadLayout& layout, float output_x, float output_y, int& game_x, int& game_y);
void Map_IPad_Window_Point(const IPadLayout& layout, float window_x, float window_y, int& game_x, int& game_y);
void Map_IPad_Normalized_Point(const IPadLayout& layout,
                               float normalized_x,
                               float normalized_y,
                               int& game_x,
                               int& game_y);

#endif
