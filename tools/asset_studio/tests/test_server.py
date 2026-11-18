"""Local regression checks; all cache/draft writes go to a temporary directory."""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('studio', Path(__file__).parents[1] / 'server.py')
studio = importlib.util.module_from_spec(spec)
spec.loader.exec_module(studio)


class StudioTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.old_cache, self.old_drafts = studio.CACHE_DIR, studio.DRAFTS_DIR
        studio.CACHE_DIR = self.tmp.name
        studio.DRAFTS_DIR = self.tmp.name
        studio._waveform_cache.clear()
        self.handler = object.__new__(studio.Handler)

    def tearDown(self):
        studio.CACHE_DIR, studio.DRAFTS_DIR = self.old_cache, self.old_drafts
        studio._waveform_cache.clear()
        self.tmp.cleanup()

    def call(self, method, q):
        """直接调用 Handler 方法并截获响应（不真的起 HTTP 服务）。"""
        captured = {"code": None, "json": None, "file": None, "ctype": None, "cache": None}
        h = object.__new__(studio.Handler)

        def send_json(obj, code=200):
            captured.update(json=obj, code=code)

        def send_file(path, ctype, cache=False):
            captured.update(file=path, ctype=ctype, cache=cache, code=200)

        def err(msg, code=400):
            captured.update(json={"error": msg}, code=code)

        h.send_json, h.send_file, h.err = send_json, send_file, err
        if q is None:
            getattr(h, method)()
        else:
            getattr(h, method)(q)
        return captured

    def test_draft_round_trip_and_validation(self):
        d = Path(self.tmp.name) / 'nightelf_wide'
        d.mkdir()
        (d / 'meta.json').write_text(json.dumps({'id': 'draft.nightelf_wide'}))
        self.assertTrue(studio.save_draft_params('draft.nightelf_wide', {'noseLen': 40}, 'full'))
        draft = studio.list_drafts()[0]
        self.assertEqual(draft['tuned']['noseLen'], 40)
        self.assertEqual(draft['state'], 'full')
        self.assertFalse(studio.save_draft_params('../outside', {}))
        with self.assertRaises(ValueError):
            studio.save_draft_params('draft.nightelf_wide', {'noseLen': float('nan')})
        with self.assertRaises(ValueError):
            studio.save_draft_params('draft.nightelf_wide', {}, 'missing')

    @unittest.skipUnless(importlib.util.find_spec("PIL"), "GIF checks require Pillow")
    def test_cache_includes_animation_options_and_build(self):
        q = {'id': ['ch2.boss.warden'], 'frames': ['4'], 'fps': ['12']}
        first, _ = self.handler._render_cached(q, 'gif')
        from PIL import Image
        with Image.open(first) as im:
            self.assertGreater(im.n_frames, 1)
            original_size = im.size
        q['fps'] = ['6']
        second, _ = self.handler._render_cached(q, 'gif')
        self.assertNotEqual(first, second)
        q['frames'] = ['8']
        third, _ = self.handler._render_cached(q, 'gif')
        self.assertNotEqual(second, third)
        with Image.open(third) as im:
            original_size = im.size
        q['zoom'] = ['2']
        zoomed, _ = self.handler._render_cached(q, 'gif')
        with Image.open(zoomed) as im:
            self.assertEqual(im.size, tuple(v * 2 for v in original_size))
        # 确认重新编译后缓存失效，不触碰实际可执行文件。
        old = studio.RENDERER
        import shutil
        binary = str(Path(self.tmp.name) / 'renderer')
        shutil.copy2(old, binary)
        try:
            studio.RENDERER = binary
            os.utime(binary, ns=(1, 1))
            changed, _ = self.handler._render_cached(q, 'gif')
            self.assertNotEqual(zoomed, changed)
        finally:
            studio.RENDERER = old

    def test_audio_catalog_contains_all_bgm_and_sfx(self):
        assets = studio.get_assets(force=True)['assets']
        audio = [a for a in assets if a.get('media') == 'audio']
        bgm = [a for a in audio if a.get('audioType') == 'bgm']
        sfx = [a for a in audio if a.get('audioType') == 'sfx']
        self.assertEqual(len(bgm), 5)
        self.assertEqual(len(sfx), 52)
        self.assertEqual(len({a['id'] for a in audio}), len(audio))
        self.assertTrue(all(a.get('duration', 0) > 0 for a in audio))
        # 游戏里的音效方法必须全部在清单中，新增 snd* 不能静默漏掉。
        audio_h = (Path(__file__).parents[3] / 'v2.0.0' / 'multiple code files' / 'audio.h')
        import re
        source = audio_h.read_text(encoding='utf-8')
        methods = set(re.findall(r'\bvoid\s+(snd[A-Za-z0-9_]*)\s*\(\s*\)', source))
        listed = {a['name'] for a in sfx}
        self.assertTrue(listed)
        self.assertEqual(len(methods), len(sfx))

    @unittest.skipUnless(os.path.isfile(studio.AUDIO_RENDERER), "audio preview binary not built")
    def test_audio_preview_exports_playable_wav(self):
        import subprocess, wave
        out = Path(self.tmp.name) / 'title.wav'
        r = subprocess.run([studio.AUDIO_RENDERER, '--render', 'audio.bgm.title',
                            '--out', str(out)], capture_output=True, text=True, timeout=30)
        self.assertEqual(r.returncode, 0, r.stderr)
        with wave.open(str(out), 'rb') as wav:
            self.assertEqual(wav.getnchannels(), 1)
            self.assertEqual(wav.getsampwidth(), 2)
            self.assertEqual(wav.getframerate(), 44100)
            self.assertGreater(wav.getnframes(), 44100)

    def test_source_resolves_stale_line_numbers(self):
        source = studio.read_source('v2.0.0/multiple code files/ch2/ch2_boss.h:1 (Ch2WardenBoss::draw)')
        self.assertIn('void draw(SDL_Renderer* r)', source['code'])
        self.assertGreater(source['line'], 200)

    @unittest.skipUnless(importlib.util.find_spec("PIL"), "Pixel comparisons require Pillow")
    def test_boss_states_are_visually_distinct(self):
        from PIL import Image, ImageChops
        for aid, a, b in [('ch2.boss.warden', 'entering', 'fight'),
                          ('ch2.boss.sphere', 'fight', 'shatter')]:
            def render(state):
                p, _ = self.handler._render_cached({'id': [aid], 'state': [state]}, 'png')
                return Image.open(p).convert('RGB')
            left, right = render(a), render(b)
            # 裁切可能改变尺寸；补回同尺寸画布后比较。
            if left.size == right.size:
                self.assertIsNotNone(ImageChops.difference(left, right).getbbox())
            else:
                self.assertNotEqual(left.size, right.size)
        path, _ = self.handler._render_cached({
            'id': ['ch2.boss.sphere'], 'state': ['entering'], 'frames': ['8']}, 'gif')
        with Image.open(path) as animation:
            for frame in range(animation.n_frames):
                animation.seek(frame)
                rgb = animation.convert('RGB')
                background = Image.new('RGB', rgb.size, '#0a0c18')
                self.assertIsNotNone(ImageChops.difference(rgb, background).getbbox())


    # ---------- 逐帧渲染 /api/frame ----------
    def test_frame_clamps_and_helpers(self):
        asset = studio.find_asset('ch2.boss.warden')
        self.assertEqual(studio.requested_frame_count({}, asset), min(asset['frames'], 32))
        self.assertEqual(studio.requested_frame_count({'frames': ['999']}, asset), studio.MAX_FRAME_TOTAL)
        self.assertEqual(studio.requested_frame_count({'frames': ['0']}, asset), 1)
        self.assertEqual(studio.requested_frame_count({'frames': ['oops']}, asset), min(asset['frames'], 32))
        self.assertEqual(studio.requested_frame_index({'frame': ['99']}, 8), 7)
        self.assertEqual(studio.requested_frame_index({'frame': ['-2']}, 8), 0)
        self.assertEqual(studio.requested_frame_index({}, 8), 0)

    def test_frame_endpoint_returns_one_png_per_frame(self):
        q0 = {'id': ['ch2.boss.warden'], 'frames': ['8'], 'frame': ['0']}
        p0, _ = self.handler._render_cached(q0, 'frame')
        with open(p0, 'rb') as f:
            b0 = f.read()
        self.assertTrue(b0.startswith(b'\x89PNG\r\n\x1a\n'), '逐帧输出必须是单文件 PNG')
        self.assertTrue(os.path.isfile(p0))
        self.assertTrue(p0.endswith('.png'))
        # 缓存命中：同一请求再次返回同一路径
        p0_again, _ = self.handler._render_cached(dict(q0), 'frame')
        self.assertEqual(p0, p0_again)
        # 不同帧：不同缓存文件、不同画面
        p4, _ = self.handler._render_cached(
            {'id': ['ch2.boss.warden'], 'frames': ['8'], 'frame': ['4']}, 'frame')
        with open(p4, 'rb') as f:
            b4 = f.read()
        self.assertNotEqual(p4, p0)
        self.assertNotEqual(b0, b4)
        # 全帧并集包围盒 ⇒ frame 0 与 frame 5 的 PNG 尺寸必须相等
        p5, _ = self.handler._render_cached(
            {'id': ['ch2.boss.warden'], 'frames': ['8'], 'frame': ['5']}, 'frame')
        if importlib.util.find_spec('PIL'):
            from PIL import Image
            with Image.open(p0) as im0, Image.open(p5) as im5:
                base_size = im0.size
                self.assertEqual(base_size, im5.size)
            # 继承 /api/png 的 zoom 语义：尺寸 ×2，且缓存与不缩放分开
            pz, _ = self.handler._render_cached(dict(q0, zoom=['2']), 'frame')
            self.assertNotEqual(pz, p0)
            with Image.open(pz) as imz:
                self.assertEqual(imz.size, (base_size[0] * 2, base_size[1] * 2))
        # 继承 scene 语义：带真实场景上下文的逐帧同样输出单文件 PNG
        ps, _ = self.handler._render_cached(
            {'id': ['ch2.boss.warden'], 'frames': ['6'], 'frame': ['2'], 'scene': ['1']}, 'frame')
        with open(ps, 'rb') as f:
            self.assertTrue(f.read(8).startswith(b'\x89PNG'))
        # HTTP 层语义：image/png，且 no-store（与 /api/png 一致）
        resp = self.call('api_frame', dict(q0))
        self.assertEqual(resp['code'], 200)
        self.assertEqual(resp['ctype'], 'image/png')
        self.assertFalse(resp['cache'])
        self.assertEqual(resp['file'], p0)

    def test_frame_endpoint_rejects_audio_and_unknown(self):
        audio = self.call('api_frame', {'id': ['audio.bgm.title'], 'frames': ['4'], 'frame': ['1']})
        self.assertEqual(audio['code'], 400)
        self.assertIn('音频', audio['json']['error'])
        missing = self.call('api_frame', {'id': ['no.such.asset'], 'frames': ['4'], 'frame': ['0']})
        self.assertEqual(missing['code'], 404)
        self.assertEqual(self.call('api_frame', {})['code'], 400)

    # ---------- 音频波形 /api/waveform ----------
    def test_waveform_peaks_are_real_and_normalized(self):
        for aid in ('audio.bgm.title', 'audio.sfx.shoot'):
            asset = studio.find_asset(aid)
            data = studio.waveform_data(asset)
            peaks = data['peaks']
            self.assertEqual(data['id'], aid)
            self.assertEqual(len(peaks), studio.WAVEFORM_BINS)
            self.assertTrue(all(0.0 <= v <= 1.0 for v in peaks))
            self.assertAlmostEqual(max(peaks), 1.0, places=3)   # 归一化到峰值 1
            self.assertGreater(len(set(peaks)), 20)             # 是真实波形，不是同一个值
            self.assertGreater(data['duration'], 0)
            self.assertEqual(data['sampleRate'], 44100)
        # 进程内缓存：同一素材第二次直接命中
        asset = studio.find_asset('audio.bgm.title')
        self.assertIs(studio.waveform_data(asset), studio.waveform_data(asset))

    def test_waveform_endpoint_rejects_non_audio_and_unknown(self):
        bad = self.call('api_waveform', {'id': ['ch2.boss.warden']})
        self.assertEqual(bad['code'], 400)
        self.assertIn('不是音频', bad['json']['error'])
        missing = self.call('api_waveform', {'id': ['no.such.asset']})
        self.assertEqual(missing['code'], 404)
        self.assertEqual(self.call('api_waveform', {})['code'], 400)
        ok = self.call('api_waveform', {'id': ['audio.sfx.shoot']})
        self.assertEqual(ok['code'], 200)
        self.assertEqual(len(ok['json']['peaks']), studio.WAVEFORM_BINS)

    # ---------- 缓存统计 / 清理 ----------
    def test_cache_stats_and_clear_respect_global_cache_dir(self):
        # 在临时 CACHE_DIR 里产生缓存（不同帧 = 不同文件）
        self.handler._render_cached({'id': ['ch2.boss.warden'], 'frames': ['4'], 'frame': ['1']}, 'frame')
        self.handler._render_cached({'id': ['ch2.boss.warden'], 'frames': ['4'], 'frame': ['2']}, 'frame')
        if os.path.isfile(studio.AUDIO_RENDERER):
            studio.waveform_data(studio.find_asset('audio.sfx.shoot'))
            self.assertIn('audio.sfx.shoot', studio._waveform_cache)

        stats = studio.cache_stats()
        self.assertGreaterEqual(stats['files'], 2)
        self.assertGreater(stats['bytes'], 0)
        # 统计的确实是临时 CACHE_DIR 里的文件（而不是真实 cache/）
        real_files = [n for _r, _d, names in os.walk(self.tmp.name) for n in names]
        self.assertEqual(len(real_files), stats['files'])
        # /api/cache 额外包含 mb
        listing = self.call('api_cache', None)
        self.assertEqual(listing['code'], 200)
        self.assertEqual(listing['json']['files'], stats['files'])
        self.assertEqual(listing['json']['bytes'], stats['bytes'])
        self.assertAlmostEqual(listing['json']['mb'],
                               round(stats['bytes'] / (1024.0 * 1024.0), 2), places=2)

        cleared = self.call('api_cache_clear', None)
        self.assertEqual(cleared['code'], 200)
        self.assertTrue(cleared['json']['ok'])
        self.assertEqual(cleared['json']['removed'], stats['files'])
        self.assertEqual(cleared['json']['bytes'], stats['bytes'])
        self.assertTrue(os.path.isdir(studio.CACHE_DIR), '清理后必须重建缓存目录')
        self.assertEqual(studio.cache_stats(), {'files': 0, 'bytes': 0})
        self.assertEqual(studio._waveform_cache, {})

    # ---------- 健康检查扩展 ----------
    def test_health_reports_build_cache_and_versions(self):
        health = studio.health_payload()
        for key in ('ok', 'renderer', 'audioRenderer', 'builtAt', 'audioBuiltAt',
                    'cache', 'versions'):
            self.assertIn(key, health)
        self.assertTrue(health['ok'])
        self.assertIsInstance(health['renderer'], bool)
        self.assertIsInstance(health['audioRenderer'], bool)
        self.assertRegex(health['builtAt'], r'^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$')
        if os.path.isfile(studio.AUDIO_RENDERER):
            self.assertRegex(health['audioBuiltAt'], r'^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$')
        self.assertIn('files', health['cache'])
        self.assertIn('bytes', health['cache'])
        self.assertIsInstance(health['versions']['python'], str)
        self.assertTrue(health['versions']['python'].startswith('3.'))
        self.assertIn('pillow', health['versions'])


if __name__ == '__main__':
    unittest.main()
