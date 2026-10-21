// =====================================================================
//  PDFForge  -  cards.h : the card scaffolding used by every view
//  Author : Ragul S
// =====================================================================
#pragma once
#include "app.h"

struct Cards { std::vector<RECT> rects; int idx = 0; };
struct CardScope { int x = 0, y = 0, w = 0; };

int  cardHeaderH();
void drawCardChrome(Frame& f, RECT r, const std::wstring& title, int step, const std::wstring& hint);
CardScope cardBegin(Frame& f, Cards& c, const std::wstring& title, int step,
                    const std::wstring& hint = std::wstring(), int gap = 0);
void cardEnd(Frame& f, Cards& c, CardScope cs);
