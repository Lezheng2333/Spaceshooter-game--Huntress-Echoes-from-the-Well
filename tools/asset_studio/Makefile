# Asset Studio — 无头素材渲染器 + BGM / SFX 试听导出
#
# 依赖：SDL2 (brew install sdl2) + zlib（macOS 自带）
# 注意：本目标每次都重新编译，避免游戏头文件或草稿改动后用到旧二进制。

GAME_DIR := ../../v2.0.0/multiple code files

CXX      := clang++
CXXFLAGS := -std=c++11 -O2 -Wall \
            -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -D_THREAD_SAFE \
            -I"$(GAME_DIR)" -Irenderer -Iregistry -Idrafts
LDFLAGS  := -L/opt/homebrew/lib -lSDL2 -lz

TARGET := assetstudio
AUDIO_TARGET := audiopreview

.PHONY: all clean check scan server

all:
	@python3 drafts/gen_drafts.py
	@echo "  [CXX] $(TARGET)  (含游戏头文件，约 2-5 秒)"
	@$(CXX) $(CXXFLAGS) renderer/assetstudio.cpp -o $(TARGET) $(LDFLAGS) && echo "  [OK ] 图片渲染器编译完成"
	@echo "  [CXX] $(AUDIO_TARGET)  (BGM / SFX 离线试听，约 1-2 秒)"
	@$(CXX) $(CXXFLAGS) -fno-access-control renderer/audiopreview.cpp -o $(AUDIO_TARGET) $(LDFLAGS) && echo "  [OK ] 音频试听器编译完成"

# 一键启动工坊
server: all
	@python3 server.py --open

# 列出素材 + 渲染试点素材 + 全部音频自检
check: all
	@./$(TARGET) --list --pretty | head -50
	@echo
	@./$(TARGET) --render ch2.player.nightelf --ascii --analysis
	@echo
	@./$(AUDIO_TARGET) --verify-all

# 交叉检查：游戏源码里有哪些绘制 / 音效入口还没登记
scan: all
	@python3 registry/scanner.py

# 无头浏览器 UI 冒烟（需要本机 Edge；服务未启动时先 python3 server.py）
ui:
	@python3 tests/ui_smoke.py

clean:
	@rm -f $(TARGET) $(AUDIO_TARGET)
	@rm -rf cache
	@echo "  [OK ] 已清理（含渲染缓存）"
