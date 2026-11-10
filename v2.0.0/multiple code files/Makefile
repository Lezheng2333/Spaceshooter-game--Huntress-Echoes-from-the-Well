CXX = clang++
CXXFLAGS = -std=c++11 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -D_THREAD_SAFE
LDFLAGS = -L/opt/homebrew/lib -lSDL2
TARGET = shooter

all: $(TARGET)

$(TARGET): main.cpp types.h constants.h art_fx.h demo_stroke_font.h font.h renderer.h audio.h floating_text.h \
		dialogue.h narration.h particles.h aim_assist.h player.h bullets.h \
		ch1/ch1_shockwave.h ch1/ch1_aliens.h ch1/ch1_boss.h ch1/ch1_background.h \
		ch2/ch2_background.h ch2/ch2_shooter_base.h ch2/ch2_hud.h \
		ch2/ch2_danmaku.h ch2/ch2_aliens.h ch2/ch2_sphere_boss.h \
		ch2/ch2_skill_orb.h ch2/ch2_pulse.h \
		ch2/ch2_gate.h ch2/ch2_airlock.h ch2/ch2_lab.h ch2/ch2_boss.h \
		chapter_manager.h ui.h save_system.h save_menu.h game.h
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
