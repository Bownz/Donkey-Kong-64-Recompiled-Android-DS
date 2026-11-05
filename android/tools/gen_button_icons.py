# Generates the in-game button icons (original artwork) as raw 64x64 RGBA files.
import os, sys
from PIL import Image, ImageDraw, ImageFont
OUT = sys.argv[1]; FONT = sys.argv[2]
S = 64; SS = 4  # draw at 4x, downsample
os.makedirs(OUT, exist_ok=True)
DARK = (34, 30, 28, 255); RIM = (12, 10, 9, 255); WHITE = (250, 246, 236, 255); YEL = (249, 214, 72, 255)
def canvas(): return Image.new('RGBA', (S*SS, S*SS), (0,0,0,0))
def font(px): return ImageFont.truetype(FONT, px*SS)
def text(d, s, px, col, cx=S/2, cy=S/2):
    f = font(px); b = d.textbbox((0,0), s, font=f)
    w, h = b[2]-b[0], b[3]-b[1]
    d.text((cx*SS - w/2 - b[0], cy*SS - h/2 - b[1]), s, font=f, fill=col)
def circle(d, fill=DARK, r=29):
    c = S*SS/2
    d.ellipse([c-(r+2)*SS, c-(r+2)*SS, c+(r+2)*SS, c+(r+2)*SS], fill=RIM)
    d.ellipse([c-r*SS, c-r*SS, c+r*SS, c+r*SS], fill=fill)
def pill(d, w=60, h=40, rad=12):
    x0=(S-w)/2*SS; y0=(S-h)/2*SS
    d.rounded_rectangle([x0-2*SS, y0-2*SS, x0+(w+2)*SS, y0+(h+2)*SS], rad*SS+2*SS, fill=RIM)
    d.rounded_rectangle([x0, y0, x0+w*SS, y0+h*SS], rad*SS, fill=DARK)
def save(img, name):
    img = img.resize((S, S), Image.LANCZOS)
    open(os.path.join(OUT, name + '.rgba'), 'wb').write(img.tobytes())
    img.save(os.path.join(OUT, '..', 'preview_' + name + '.png'))
def face(name, letter, col):
    im = canvas(); d = ImageDraw.Draw(im); circle(d); text(d, letter, 40, col); save(im, name)
for l, c in (('a',(96,200,72,255)),('b',(232,72,60,255)),('x',(64,140,240,255)),('y',(249,200,40,255))):
    face('xbox_' + l, l.upper(), c)
for l in 'abxy': face('nin_' + l, l.upper(), WHITE)
def shape(name, kind, col):
    im = canvas(); d = ImageDraw.Draw(im); circle(d); c = S*SS/2; r = 15*SS; w = 5*SS
    if kind == 'cross':
        d.line([c-r, c-r, c+r, c+r], fill=col, width=w); d.line([c-r, c+r, c+r, c-r], fill=col, width=w)
    elif kind == 'circle':
        d.ellipse([c-r, c-r, c+r, c+r], outline=col, width=w)
    elif kind == 'square':
        d.rectangle([c-r*0.85, c-r*0.85, c+r*0.85, c+r*0.85], outline=col, width=w)
    else:
        d.polygon([(c, c-r*1.05), (c+r*1.05, c+r*0.75), (c-r*1.05, c+r*0.75)], outline=col, width=w)
    save(im, name)
shape('ps_cross','cross',(120,170,240,255)); shape('ps_circle','circle',(240,100,100,255))
shape('ps_square','square',(230,130,200,255)); shape('ps_triangle','triangle',(90,210,170,255))
def shoulder(name, label, px=30):
    im = canvas(); d = ImageDraw.Draw(im); pill(d); text(d, label, px, WHITE); save(im, name)
for n in ('lb','rb','lt','rt','l','r','zl','zr','l1','r1','l2','r2'): shoulder(n, n.upper(), 30 if len(n) == 2 else 34)
def round_label(name, label, px=40):
    im = canvas(); d = ImageDraw.Draw(im); circle(d); text(d, label, px, WHITE); save(im, name)
def start_icon():
    im = canvas(); d = ImageDraw.Draw(im); circle(d); c = S*SS/2
    for dy in (-9, 0, 9):
        d.rounded_rectangle([c-14*SS, c+(dy-2)*SS, c+14*SS, c+(dy+2)*SS], 2*SS, fill=WHITE)
    save(im, 'start')
def back_icon():
    im = canvas(); d = ImageDraw.Draw(im); circle(d); c = S*SS/2
    d.rectangle([c-13*SS, c-11*SS, c+5*SS, c+5*SS], outline=WHITE, width=3*SS)
    d.rectangle([c-5*SS, c-3*SS, c+13*SS, c+13*SS], fill=DARK, outline=WHITE, width=3*SS)
    save(im, 'back')
start_icon(); back_icon()
round_label('plus', '+', 46); round_label('minus', '−', 46)
def stick(name, side, direction):
    im = canvas(); d = ImageDraw.Draw(im); circle(d); c = S*SS/2
    d.ellipse([c-17*SS, c-17*SS, c+17*SS, c+17*SS], outline=(90,84,78,255), width=3*SS)
    text(d, side, 24, WHITE)
    if direction:
        a = 25*SS; t = 8*SS
        pts = {'up': [(c, c-a-2*SS), (c-t, c-a+t), (c+t, c-a+t)], 'down': [(c, c+a+2*SS), (c-t, c+a-t), (c+t, c+a-t)],
               'left': [(c-a-2*SS, c), (c-a+t, c-t), (c-a+t, c+t)], 'right': [(c+a+2*SS, c), (c+a-t, c-t), (c+a-t, c+t)]}[direction]
        d.polygon(pts, fill=YEL)
    save(im, name)
for side in ('l', 'r'):
    for dr in ('up', 'down', 'left', 'right'): stick(side + 's_' + dr, side.upper(), dr)
    stick(side + 's_click', side.upper() + '3' if False else side.upper(), None)
def dpad(name, direction):
    im = canvas(); d = ImageDraw.Draw(im); c = S*SS/2; a = 28*SS; t = 9*SS
    for col, grow in ((RIM, 2*SS), (DARK, 0)):
        d.rectangle([c-t-grow, c-a-grow, c+t+grow, c+a+grow], fill=col); d.rectangle([c-a-grow, c-t-grow, c+a+grow, c+t+grow], fill=col)
    box = {'up': [c-t, c-a, c+t, c-t], 'down': [c-t, c+t, c+t, c+a], 'left': [c-a, c-t, c-t, c+t], 'right': [c+t, c-t, c+a, c+t]}[direction]
    d.rectangle(box, fill=YEL); save(im, name)
for dr in ('up','down','left','right'): dpad('dpad_' + dr, dr)
