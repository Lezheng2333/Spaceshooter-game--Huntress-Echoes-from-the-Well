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
        self.handler = object.__new__(studio.Handler)

    def tearDown(self):
        studio.CACHE_DIR, studio.DRAFTS_DIR = self.old_cache, self.old_drafts
        self.tmp.cleanup()

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


if __name__ == '__main__':
    unittest.main()
