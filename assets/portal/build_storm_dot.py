#!/usr/bin/env python3
"""Build Storm Dot, an original 5×7 round-dot display face. Requires fontTools.
Original geometric artwork for stormDASH; released under CC0-1.0.
"""
from pathlib import Path
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

# Each row is a five-bit drawing, top first; lowercase shares uppercase glyphs.
PATTERNS = {
'A':['01110','10001','10001','11111','10001','10001','10001'],
'B':['11110','10001','10001','11110','10001','10001','11110'],
'C':['01111','10000','10000','10000','10000','10000','01111'],
'D':['11110','10001','10001','10001','10001','10001','11110'],
'E':['11111','10000','10000','11110','10000','10000','11111'],
'F':['11111','10000','10000','11110','10000','10000','10000'],
'G':['01110','10001','10000','10111','10001','10001','01110'],
'H':['10001','10001','10001','11111','10001','10001','10001'],
'I':['11111','00100','00100','00100','00100','00100','11111'],
'J':['00111','00010','00010','00010','10010','10010','01100'],
'K':['10001','10010','10100','11000','10100','10010','10001'],
'L':['10000','10000','10000','10000','10000','10000','11111'],
'M':['10001','11011','10101','10101','10001','10001','10001'],
'N':['10001','11001','11001','10101','10011','10011','10001'],
'O':['01110','10001','10001','10001','10001','10001','01110'],
'P':['11110','10001','10001','11110','10000','10000','10000'],
'Q':['01110','10001','10001','10001','10101','10010','01101'],
'R':['11110','10001','10001','11110','10100','10010','10001'],
'S':['01111','10000','10000','01110','00001','00001','11110'],
'T':['11111','00100','00100','00100','00100','00100','00100'],
'U':['10001','10001','10001','10001','10001','10001','01110'],
'V':['10001','10001','10001','10001','10001','01010','00100'],
'W':['10001','10001','10001','10101','10101','11011','10001'],
'X':['10001','10001','01010','00100','01010','10001','10001'],
'Y':['10001','10001','01010','00100','00100','00100','00100'],
'Z':['11111','00001','00010','00100','01000','10000','11111'],
'0':['01110','10001','10011','10101','11001','10001','01110'],
'1':['00100','01100','00100','00100','00100','00100','01110'],
'2':['01110','10001','00001','00010','00100','01000','11111'],
'3':['11110','00001','00001','01110','00001','00001','11110'],
'4':['00010','00110','01010','10010','11111','00010','00010'],
'5':['11111','10000','10000','11110','00001','00001','11110'],
'6':['01110','10000','10000','11110','10001','10001','01110'],
'7':['11111','00001','00010','00100','01000','01000','01000'],
'8':['01110','10001','10001','01110','10001','10001','01110'],
'9':['01110','10001','10001','01111','00001','00001','01110'],
'.':['00000','00000','00000','00000','00000','00100','00100'],
':':['00000','00100','00100','00000','00100','00100','00000'],
'/':['00001','00001','00010','00100','01000','10000','10000'],
'-':['00000','00000','00000','11111','00000','00000','00000'],
' ':['00000']*7,
}

def dot(pen,x,y,r=34):
    pen.moveTo((x+r,y))
    pen.qCurveTo((x+r,y+r),(x,y+r))
    pen.qCurveTo((x-r,y+r),(x-r,y))
    pen.qCurveTo((x-r,y-r),(x,y-r))
    pen.qCurveTo((x+r,y-r),(x+r,y))
    pen.closePath()

glyphs={}
charmap={}
for char,rows in PATTERNS.items():
    name='uni%04X'%ord(char)
    pen=TTGlyphPen(None)
    for row,bits in enumerate(rows):
        for col,bit in enumerate(bits):
            if bit=='1': dot(pen,100+100*col,700-row*100)
    glyphs[name]=pen.glyph()
    charmap[ord(char)]=name
    if char.isalpha(): charmap[ord(char.lower())]=name
pen=TTGlyphPen(None)
glyphs['.notdef']=pen.glyph()
font=FontBuilder(1000,isTTF=True)
font.setupGlyphOrder(['.notdef']+[n for n in glyphs if n!='.notdef'])
font.setupCharacterMap(charmap)
font.setupGlyf(glyphs)
font.setupHorizontalMetrics({name:(600,66 if name!='uni0020' else 0) for name in glyphs})
font.setupHorizontalHeader(ascent=800,descent=-200,lineGap=0)
font.setupNameTable({'familyName':'Storm Dot','styleName':'Regular','uniqueFontIdentifier':'StormDot-Original-1.0','fullName':'Storm Dot Regular','psName':'StormDot-Regular','version':'Version 1.0','copyright':'Original geometric design for stormDASH. CC0-1.0.'})
font.setupOS2(sTypoAscender=800,sTypoDescender=-200,sTypoLineGap=0,usWinAscent=800,usWinDescent=200,sxHeight=700,sCapHeight=734)
font.setupPost()
font.setupMaxp()
# WOFF uses built-in zlib; no external network or font dependency at runtime.
font.font.flavor='woff'
out=Path(__file__).with_name('storm-dot.woff')
font.save(out)
print(out, out.stat().st_size)
