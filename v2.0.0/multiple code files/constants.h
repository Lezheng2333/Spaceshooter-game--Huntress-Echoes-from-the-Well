#pragma once

// ============== 游戏标题 ==============
static const char* GAME_TITLE = "Huntress: Echoes from the Well";
static const char* GAME_TITLE_MAIN = "HUNTRESS";
static const char* GAME_TITLE_SUBTITLE = "ECHOES FROM THE WELL";

// ============== 窗口 & 透视常量 ==============
const int WIN_WIDTH = 800;
const int WIN_HEIGHT = 600;
const int CENTER_X = WIN_WIDTH / 2;
const int HORIZON_Y = 200;
const double HORIZON_LEFT  = WIN_WIDTH * 0.25;
const double HORIZON_RIGHT = WIN_WIDTH * 0.75;

inline double perspLeft(double y) {
    return HORIZON_LEFT  * (WIN_HEIGHT - y) / (WIN_HEIGHT - HORIZON_Y);
}
inline double perspRight(double y) {
    return WIN_WIDTH - HORIZON_LEFT * (WIN_HEIGHT - y) / (WIN_HEIGHT - HORIZON_Y);
}
inline double perspWidth(double y) {
    return perspRight(y) - perspLeft(y);
}
