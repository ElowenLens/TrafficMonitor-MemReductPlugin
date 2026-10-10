# -*- coding: utf-8 -*-
"""生成插件图标 plugin.ico（16/32/48 三种尺寸，纯 Python 无第三方依赖）
设计：蓝绿色渐变圆角方块 + 白色向下箭头（表示释放内存）
"""
import struct

def draw_master(size=48):
    """在 size x size 画布上绘制图标，返回 (r,g,b,a) 像素列表"""
    px = [[(0, 0, 0, 0)] * size for _ in range(size)]

    def lerp(a, b, t):
        return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(4))

    c_top = (46, 134, 222, 255)     # 蓝色
    c_bot = (16, 172, 132, 255)     # 绿色

    # 圆角方块背景（垂直渐变）
    radius = size // 5
    for y in range(size):
        t = y / (size - 1)
        row_color = lerp(c_top, c_bot, t)
        for x in range(size):
            # 圆角判断
            dx = min(x, size - 1 - x)
            dy = min(y, size - 1 - y)
            if dx < radius and dy < radius:
                ox, oy = radius - dx, radius - dy
                if ox * ox + oy * oy > radius * radius:
                    continue
            px[y][x] = row_color

    def fill(x0, y0, x1, y1, color):
        for y in range(max(0, y0), min(size, y1)):
            for x in range(max(0, x0), min(size, x1)):
                px[y][x] = color

    white = (255, 255, 255, 255)
    cx = size // 2

    # 箭头杆
    shaft_w = max(2, size // 8)
    shaft_top = int(size * 0.18)
    head_top = int(size * 0.52)
    fill(cx - shaft_w // 2, shaft_top, cx - shaft_w // 2 + shaft_w, head_top, white)

    # 箭头头部（三角）
    head_h = size - int(size * 0.18) - head_top
    for i in range(head_h):
        half = int((i + 1) * (size * 0.30) / head_h)
        y = head_top + i
        fill(cx - half, y, cx + half + 1, y + 1, white)

    return px

def downscale(px, new_size):
    old = len(px)
    step = old / new_size
    out = []
    for y in range(new_size):
        row = []
        for x in range(new_size):
            x0, x1 = int(x * step), max(int(x * step) + 1, int((x + 1) * step))
            y0, y1 = int(y * step), max(int(y * step) + 1, int((y + 1) * step))
            r = g = b = a = cnt = 0
            for yy in range(y0, min(y1, old)):
                for xx in range(x0, min(x1, old)):
                    c = px[yy][xx]
                    r += c[0]; g += c[1]; b += c[2]; a += c[3]; cnt += 1
            row.append((r // cnt, g // cnt, b // cnt, a // cnt))
        out.append(row)
    return out

def bmp_data(px):
    """把像素转成 ICO 内的 BMP 数据（32bpp BGRA 自底向上 + AND 掩码）"""
    h = len(px)
    w = len(px[0])
    header = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0,
                         w * h * 4, 0, 0, 0, 0)
    pixels = b''
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b, a = px[y][x]
            pixels += struct.pack('<BBBB', b, g, r, a)
    and_stride = ((w + 31) // 32) * 4
    mask = b'\x00' * (and_stride * h)
    return header + pixels + mask

def build_ico(path):
    master = draw_master(48)
    images = []
    for s in (16, 32, 48):
        img = downscale(master, s)
        images.append((s, bmp_data(img)))
    ico = struct.pack('<HHH', 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b''
    for s, data in images:
        entries += struct.pack('<BBBBHHII', s if s < 256 else 0, s, 0, 0,
                               1, 32, len(data), offset)
        offset += len(data)
    with open(path, 'wb') as f:
        f.write(ico + entries + b''.join(d for _, d in images))
    print('icon written:', path)

if __name__ == '__main__':
    build_ico('plugin.ico')
