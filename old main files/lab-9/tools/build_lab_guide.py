"""Render the editable Markdown guide. Requires reportlab; run from project root."""
from pathlib import Path
from html import escape
import re
from reportlab.pdfgen import canvas
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, Preformatted
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

fontdir = Path('/System/Library/Fonts/Supplemental')
for name, file in [('Body', 'Arial.ttf'), ('Bold', 'Arial Bold.ttf'), ('Mono', 'Courier New.ttf')]:
    pdfmetrics.registerFont(TTFont(name, str(fontdir / file)))
styles = {
    'body': ParagraphStyle('body', fontName='Body', fontSize=9.3, leading=12, spaceAfter=5),
    'title': ParagraphStyle('title', fontName='Bold', fontSize=19, leading=23, spaceAfter=12),
    'h2': ParagraphStyle('h2', fontName='Bold', fontSize=11.5, leading=15, spaceBefore=6, spaceAfter=5, keepWithNext=True),
    'cell': ParagraphStyle('cell', fontName='Body', fontSize=8.7, leading=10.8),
    'head': ParagraphStyle('head', fontName='Bold', fontSize=8.7, leading=10.8, textColor=colors.white),
    'code': ParagraphStyle('code', fontName='Mono', fontSize=8.0, leading=10.5, spaceAfter=8, backColor=colors.HexColor('#f1f4f7'), borderPadding=7),
}
W,H=A4
width=W-88

def inline(s):
    s=escape(s)
    if s.startswith('https://'):
        s=f'<link href="{s}" color="#165f88">{s}</link>'
    return s

def footer(c, doc):
    c.saveState()
    c.setFont('Body',8)
    c.setFillColor(colors.HexColor('#526170'))
    c.drawString(44,H-28,'48033 IoT 2026  |  Week 8 practice and Week 9 lab')
    c.setStrokeColor(colors.HexColor('#d9e2e9'))
    c.line(44,36,W-44,36)
    c.drawString(44,23,'ESP32 MQTT fire alarm  |  Bench and assessment guide')
    c.drawRightString(W-44,23,str(doc.page))
    c.restoreState()

story=[]
source=Path('LAB9_GUIDE.md').read_text()
for page_index, chunk in enumerate(source.split('---PAGE---')):
    if page_index: story.append(PageBreak())
    lines=chunk.strip().splitlines(); i=0
    while i<len(lines):
        line=lines[i]
        if not line.strip(): i+=1; continue
        if line.startswith('```'):
            i+=1; block=[]
            while i<len(lines) and not lines[i].startswith('```'):
                block.append(lines[i]); i+=1
            story.append(Preformatted('\n'.join(block),styles['code']))
            i+=1; continue
        if line.startswith('|'):
            rows=[]
            while i<len(lines) and lines[i].startswith('|'):
                cells=[s.strip() for s in lines[i].strip('|').split('|')]
                if not all(re.fullmatch(r'[-: ]+',s) for s in cells): rows.append(cells)
                i+=1
            data=[[Paragraph(inline(c), styles['head' if n==0 else 'cell']) for c in r] for n,r in enumerate(rows)]
            table=Table(data,colWidths=[width*.36,width*.64],hAlign='LEFT',repeatRows=1)
            table.setStyle(TableStyle([
                ('BACKGROUND',(0,0),(-1,0),colors.HexColor('#244c63')),
                ('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.HexColor('#f1f5f7'),colors.white]),
                ('VALIGN',(0,0),(-1,-1),'TOP'),
                ('LEFTPADDING',(0,0),(-1,-1),7),('RIGHTPADDING',(0,0),(-1,-1),7),
                ('TOPPADDING',(0,0),(-1,-1),3),('BOTTOMPADDING',(0,0),(-1,-1),3),
                ('LINEBELOW',(0,-1),(-1,-1),.4,colors.HexColor('#ccd9e0')),
            ]))
            story.extend([table,Spacer(1,8)]); continue
        if line.startswith('# '): style='title'; line=line[2:]
        elif line.startswith('## '): style='h2'; line=line[3:]
        else: style='body'
        story.append(Paragraph(inline(line),styles[style])); i+=1

out=Path('output/pdf/Lab9_ESP32_MQTT_Setup_Guide.pdf')
doc=SimpleDocTemplate(str(out),pagesize=A4,rightMargin=44,leftMargin=44,topMargin=47,bottomMargin=48,
    title='ESP32 MQTT fire alarm lab guide',author='Lab project guide')
doc.build(story,onFirstPage=footer,onLaterPages=footer)
print(out)
