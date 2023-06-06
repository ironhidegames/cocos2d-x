/****************************************************************************
 Copyright (c) 2013      Zynga Inc.
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

 http://www.cocos2d-x.org

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "2d/CCLabel.h"
#include <vector>
#include "base/ccUTF8.h"
#include "base/CCDirector.h"
#include "2d/CCFontAtlas.h"
#include "2d/CCFontFNT.h"
#include "fribidi.h"

NS_CC_BEGIN

void Label::computeAlignmentOffset()
{
    _linesOffsetX.clear();
    switch (_hAlignment)
    {
    case cocos2d::TextHAlignment::LEFT:
        _linesOffsetX.assign(_numberOfLines, 0);
        break;
    case cocos2d::TextHAlignment::CENTER:
        for (auto lineWidth : _linesWidth)
        {
            _linesOffsetX.push_back((_contentSize.width - lineWidth) / 2.f);
        }
        break;
    case cocos2d::TextHAlignment::RIGHT:
        for (auto lineWidth : _linesWidth)
        {
            _linesOffsetX.push_back(_contentSize.width - lineWidth);
        }
        break;
    default:
        break;
    }

    switch (_vAlignment)
    {
    case cocos2d::TextVAlignment::TOP:
        _letterOffsetY = _contentSize.height;
        break;
    case cocos2d::TextVAlignment::CENTER:
        _letterOffsetY = (_contentSize.height + _textDesiredHeight) / 2.f;
        break;
    case cocos2d::TextVAlignment::BOTTOM:
        _letterOffsetY = _textDesiredHeight;
        break;
    default:
        break;
    }
}

int Label::getFirstCharLen(const std::u32string& /*utf32Text*/, int /*startIndex*/, int /*textLen*/) const
{
    return 1;
}

int Label::getFirstWordLen(const std::u32string& utf32Text, int startIndex, int textLen) const
{
    int len = 0;
    auto nextLetterX = 0;
    FontLetterDefinition letterDef;
    auto contentScaleFactor = CC_CONTENT_SCALE_FACTOR();

    for (int index = startIndex; index < textLen; ++index)
    {
        char32_t character = utf32Text[index];

        if (character == StringUtils::UnicodeCharacters::NewLine
            || (!StringUtils::isUnicodeNonBreaking(character)
                && (StringUtils::isUnicodeSpace(character)
                    || StringUtils::isCJKUnicode(character))))
        {
            break;
        }

        if (!getFontLetterDef(character, letterDef))
        {
            break;
        }

        if (_maxLineWidth > 0.f)
        {
            auto letterX = (nextLetterX + letterDef.offsetX * _bmfontScale) / contentScaleFactor;

            if (letterX + letterDef.width * _bmfontScale > _maxLineWidth)
                break;
        }

        nextLetterX += letterDef.xAdvance * _bmfontScale + _additionalKerning;

        len++;
    }

    if (len == 0 && textLen)
        len = 1;

    return len;
}

bool Label::getFontLetterDef(char32_t character, FontLetterDefinition& letterDef) const
{
    if (character == StringUtils::UnicodeCharacters::NoBreakSpace)
    {
        // change no-break space to regular space
        // reason: some fonts have issue with no-break space:
        //   * no letter definition
        //   * not normal big width
        character = StringUtils::UnicodeCharacters::Space;
    }

    return _fontAtlas->getLetterDefinitionForChar(character, letterDef);
}

void Label::updateBMFontScale()
{
    auto font = _fontAtlas->getFont();
    if (_currentLabelType == LabelType::BMFONT) {
        FontFNT *bmFont = (FontFNT*)font;
        float originalFontSize = bmFont->getOriginalFontSize();
        _bmfontScale = _bmFontSize * CC_CONTENT_SCALE_FACTOR() / originalFontSize;
    }else{
        _bmfontScale = 1.0f;
    }
}

bool Label::multilineTextWrap(const std::function<int(const std::u32string&, int, int)>& nextTokenLen)
{
    int textLen = getStringLength();
    int lineIndex = 0;
    float nextTokenX = 0.f;
    float nextTokenY = 0.f;
    float longestLine = 0.f;
    float letterRight = 0.f;
    float nextWhitespaceWidth = 0.f;

    auto contentScaleFactor = CC_CONTENT_SCALE_FACTOR();
    float lineSpacing = _lineSpacing * contentScaleFactor;
    float highestY = 0.f;
    float lowestY = 0.f;
    FontLetterDefinition letterDef;
    Vec2 letterPosition;
    bool nextChangeSize = true;

    this->updateBMFontScale();

    for (int index = 0; index < textLen; )
    {
        char32_t character = _utf32Text[index];
        if (character == StringUtils::UnicodeCharacters::NewLine)
        {
            //CCLOG(" ACTUAL breaking at char:%i nextTokenX:%f", index, nextTokenX);
            _linesWidth.push_back(letterRight);
            letterRight = 0.f;
            lineIndex++;
            nextTokenX = 0.f;
            nextTokenY -= _lineHeight*_bmfontScale + lineSpacing;
            recordPlaceholderInfo(index, character);
            index++;
            continue;
        }

        auto tokenLen = nextTokenLen(_utf32Text, index, textLen);
        float tokenHighestY = highestY;
        float tokenLowestY = lowestY;
        float tokenRight = letterRight;
        float nextLetterX = nextTokenX;
        float whitespaceWidth = nextWhitespaceWidth;
        bool newLine = false;
        for (int tmp = 0; tmp < tokenLen;++tmp)
        {
            int letterIndex = index + tmp;
            character = _utf32Text[letterIndex];
            if (character == StringUtils::UnicodeCharacters::CarriageReturn)
            {
                recordPlaceholderInfo(letterIndex, character);
                continue;
            }

            // \b - Next char not change x position
            if (character == StringUtils::UnicodeCharacters::NextCharNoChangeX)
            {
                nextChangeSize = false;
                recordPlaceholderInfo(letterIndex, character);
                continue;
            }

            if (!getFontLetterDef(character, letterDef))
            {
                recordPlaceholderInfo(letterIndex, character);
                //CCLOG("LabelTextFormatter error: can't find letter definition in font file for letter: 0x%x", character);
                continue;
            }

            auto letterX = (nextLetterX + letterDef.offsetX * _bmfontScale) / contentScaleFactor;
            //CCLOG("    ACTUAL char:%x x:%f", character, letterX);
            if (_enableWrap && _maxLineWidth > 0.f && nextTokenX > 0.f && letterX + letterDef.width * _bmfontScale > _maxLineWidth
                && !StringUtils::isUnicodeSpace(character) && nextChangeSize)
            {
                _linesWidth.push_back(letterRight - whitespaceWidth);
                nextWhitespaceWidth = 0.f;
                letterRight = 0.f;
                lineIndex++;
                nextTokenX = 0.f;
                nextTokenY -= (_lineHeight*_bmfontScale + lineSpacing);
                newLine = true;
                break;
            }
            else
            {
                letterPosition.x = letterX;
            }
            letterPosition.y = (nextTokenY - letterDef.offsetY * _bmfontScale) / contentScaleFactor;
            recordLetterInfo(letterPosition, character, letterIndex, lineIndex);

            if (nextChangeSize)
            {
                float newLetterWidth = 0.f;
                if (_horizontalKernings && letterIndex < textLen - 1)
                    newLetterWidth = _horizontalKernings[letterIndex + 1];
                newLetterWidth += letterDef.xAdvance * _bmfontScale + _additionalKerning;

                nextLetterX += newLetterWidth;
                tokenRight = nextLetterX / contentScaleFactor;

                if (StringUtils::isUnicodeSpace(character))
                {
                    nextWhitespaceWidth += newLetterWidth / contentScaleFactor;
                }
                else
                {
                    nextWhitespaceWidth = 0;
                }
            }
            nextChangeSize = true;

            if (tokenHighestY < letterPosition.y)
                tokenHighestY = letterPosition.y;
            if (tokenLowestY > letterPosition.y - letterDef.height * _bmfontScale)
                tokenLowestY = letterPosition.y - letterDef.height * _bmfontScale;
        }

        if (newLine)
        {
            continue;
        }

        nextTokenX = nextLetterX;
        letterRight = tokenRight;
        if (highestY < tokenHighestY)
            highestY = tokenHighestY;
        if (lowestY > tokenLowestY)
            lowestY = tokenLowestY;

        index += tokenLen;
    }

    if (_linesWidth.empty())
    {
        _linesWidth.push_back(letterRight);
        longestLine = letterRight;
    }
    else
    {
        _linesWidth.push_back(letterRight - nextWhitespaceWidth);
        for (auto && lineWidth : _linesWidth)
        {
            if (longestLine < lineWidth)
                longestLine = lineWidth;
        }
    }

    _numberOfLines = lineIndex + 1;
    _textDesiredHeight = (_numberOfLines * _lineHeight * _bmfontScale) / contentScaleFactor;
    if (_numberOfLines > 1)
        _textDesiredHeight += (_numberOfLines - 1) * _lineSpacing;
    Size contentSize(_labelWidth, _labelHeight);
    if (_labelWidth <= 0.f)
        contentSize.width = longestLine;
    if (_labelHeight <= 0.f)
        contentSize.height = _textDesiredHeight;
    setContentSize(contentSize);

    _tailoredTopY = contentSize.height;
    _tailoredBottomY = 0.f;
    if (highestY > 0.f)
        _tailoredTopY = contentSize.height + highestY;
    if (lowestY < -_textDesiredHeight)
        _tailoredBottomY = _textDesiredHeight + lowestY;

    return true;
}

bool Label::multilineTextWrapByWord()
{
    return multilineTextWrap(CC_CALLBACK_3(Label::getFirstWordLen, this));
}

bool Label::multilineTextWrapByChar()
{
    return multilineTextWrap(CC_CALLBACK_3(Label::getFirstCharLen, this));
}

bool Label::isVerticalClamp()
{
    if (_textDesiredHeight > _contentSize.height)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Label::isHorizontalClamp()
{
    bool letterClamp = false;
    for (int ctr = 0; ctr < _lengthOfString; ++ctr)
    {
        if (_lettersInfo[ctr].valid)
        {
            auto& letterDef = _fontAtlas->_letterDefinitions[_lettersInfo[ctr].utf32Char];

            auto px = _lettersInfo[ctr].positionX + letterDef.width/2 * _bmfontScale;
            auto lineIndex = _lettersInfo[ctr].lineIndex;

            if(_labelWidth > 0.f){
                if(!this->_enableWrap){
                    if (px > _contentSize.width) {
                        letterClamp = true;
                        break;
                    }
                }
                else{
                    auto wordWidth = this->_linesWidth[lineIndex];
                    if(wordWidth > this->_contentSize.width && (px > _contentSize.width)){
                        letterClamp = true;
                        break;
                    }

                }

            }
        }
    }

    return letterClamp;
}

void Label::shrinkLabelToContentSize(const std::function<bool(void)>& lambda)
{
    float fontSize = this->getRenderingFontSize();

    int i = 0;
    auto letterDefinition = _fontAtlas->_letterDefinitions;
    auto tempLetterDefinition = letterDefinition;
    float originalLineHeight = _lineHeight;
    bool flag = true;
    while (lambda()) {
        ++i;
        float newFontSize = fontSize - i;
        flag = false;
        if (newFontSize <= 0) {
            break;
        }
        float scale = newFontSize / fontSize;
        std::swap(_fontAtlas->_letterDefinitions, tempLetterDefinition);
        _fontAtlas->scaleFontLetterDefinition(scale);
        this->setLineHeight(originalLineHeight * scale);

        if (getTextDirection() == TextDirection::RTL) {
            // replace string with a pre-wrapped and normalized one, with newlines
            auto nText = rtlWrapAndNormalize(_utf8Text);
            std::u32string utf32String;
            if (StringUtils::UTF8ToUTF32(nText, utf32String)) {
                _utf32Text  = utf32String;
            }
        }

        if (_maxLineWidth > 0.f && !_lineBreakWithoutSpaces)
        {
            multilineTextWrapByWord();
        }
        else
        {
            multilineTextWrapByChar();
        }
        computeAlignmentOffset();
        tempLetterDefinition = letterDefinition;
    }
    this->setLineHeight(originalLineHeight);
    std::swap(_fontAtlas->_letterDefinitions, letterDefinition);

    if (!flag) {
        if (fontSize - i >= 0) {
            this->scaleFontSizeDown(fontSize - i);
        }
    }
}

void Label::recordLetterInfo(const cocos2d::Vec2& point, char32_t utf32Char, int letterIndex, int lineIndex)
{
    if (static_cast<std::size_t>(letterIndex) >= _lettersInfo.size())
    {
        LetterInfo tmpInfo;
        _lettersInfo.push_back(tmpInfo);
    }
    _lettersInfo[letterIndex].lineIndex = lineIndex;
    _lettersInfo[letterIndex].utf32Char = utf32Char;
    _lettersInfo[letterIndex].valid = _fontAtlas->_letterDefinitions[utf32Char].validDefinition;
    _lettersInfo[letterIndex].positionX = point.x;
    _lettersInfo[letterIndex].positionY = point.y;
    _lettersInfo[letterIndex].atlasIndex = -1;
}

void Label::recordPlaceholderInfo(int letterIndex, char32_t utf32Char)
{
    if (static_cast<std::size_t>(letterIndex) >= _lettersInfo.size())
    {
        LetterInfo tmpInfo;
        _lettersInfo.push_back(tmpInfo);
    }
    _lettersInfo[letterIndex].utf32Char = utf32Char;
    _lettersInfo[letterIndex].valid = false;
}

// ------------------------------------------------------------
// -- RTL functions
// 

std::string Label::rtlWrapAndNormalize(std::string input)
{
    bool wordWrap = (_maxLineWidth > 0.f && !_lineBreakWithoutSpaces);

    this->updateBMFontScale();

    std::u32string input32;
    StringUtils::UTF8ToUTF32(input, input32);
    
    FriBidiChar* shaped_str = NULL;
    FriBidiCharType* bidi_types = NULL;
    FriBidiBracketType* bracket_types = NULL;
    FriBidiLevel* embedding_levels = NULL;
    FriBidiArabicProp* ar_props = NULL;
    FriBidiParType pbase_dir = FRIBIDI_PAR_RTL;
    FriBidiFlags flags = FRIBIDI_FLAGS_DEFAULT | FRIBIDI_FLAGS_ARABIC;

    FriBidiChar* str = (FriBidiChar*)input32.data();
    FriBidiStrIndex len = input32.size();

    shaped_str = new FriBidiChar[len];
    bidi_types = new FriBidiCharType[len];
    bracket_types = new FriBidiBracketType[len];
    embedding_levels = new FriBidiLevel[len];

    // shapes the arabic string without reordering it, so we can
    // calculate the width of the lines in the original character order
    fribidi_get_bidi_types (str, len, bidi_types);
    fribidi_get_bracket_types(str, len, bidi_types, bracket_types);
    FriBidiLevel max_level = fribidi_get_par_embedding_levels_ex(bidi_types, bracket_types, len, &pbase_dir, embedding_levels) - 1;

    memcpy(shaped_str, str, len * sizeof (*shaped_str));
    ar_props = new FriBidiArabicProp[len];

    fribidi_get_joining_types(str, len, ar_props);
    fribidi_join_arabic(bidi_types, len, embedding_levels, ar_props);
    fribidi_shape(flags, embedding_levels, len, ar_props, shaped_str);

    std::u32string shaped((char32_t*)shaped_str);
    _fontAtlas->prepareLetterDefinitions(shaped);  // load with the contextual forms

    // walk the shaped string and break it into lines, reversting each line in the process
    std::vector<std::u32string> lines32;
    auto contentScaleFactor = CC_CONTENT_SCALE_FACTOR();
    float curWidth = 0;
    int lineStart = 0;
    FontLetterDefinition letterDef;

    // ---------------------------------------- 
    auto PUSH = [=,&lines32](int lstart, int llen) {
        fribidi_reorder_line(flags, bidi_types, llen, lstart, pbase_dir, embedding_levels, shaped_str, NULL);
        std::u32string str32 = std::u32string(shaped_str + lstart, shaped_str + lstart + llen);
        lines32.push_back(str32);
    };

    auto WORD_LEN = [=](int startIndex) {
        int wlen = 0;
        auto nextLetterX = 0;
        FontLetterDefinition letterDef;
        auto contentScaleFactor = CC_CONTENT_SCALE_FACTOR();
        for (int index = startIndex; index < len; ++index) {
                char32_t character = shaped_str[index];
                if (character == StringUtils::UnicodeCharacters::NewLine
                    || (!StringUtils::isUnicodeNonBreaking(character)
                        && (StringUtils::isUnicodeSpace(character)
                            || StringUtils::isCJKUnicode(character)))) {
                    break;
                }
                if (!getFontLetterDef(character, letterDef)) {
                    break;
                }
                if (_maxLineWidth > 0.f) {
                    auto letterX = (nextLetterX + letterDef.offsetX * _bmfontScale) / contentScaleFactor;
                    if (letterX + letterDef.width * _bmfontScale > _maxLineWidth)
                        break;
                }
                nextLetterX += letterDef.xAdvance * _bmfontScale + _additionalKerning;
                wlen++;
        }
        if (wlen == 0 && len)
            wlen = 1;
        return wlen;
    };
    // ---------------------------------------- 

    CCLOG("Break starts for %s", shaped.c_str());
    for (int idx=0; idx<len;) {
        char32_t character = shaped_str[idx];
        if (character == StringUtils::UnicodeCharacters::NewLine) {
            PUSH(lineStart, idx-lineStart);
            curWidth = 0;
            idx++; // skip the newline
            lineStart = idx;
            continue;
        }

        auto tokenWidth = 0.f;
        auto tokenLen = wordWrap ? WORD_LEN(idx) : 1;
        for (int tmp = 0; tmp < tokenLen; ++tmp)
        {
            int letterIndex = idx + tmp;
            character = shaped_str[letterIndex];

            if (character == StringUtils::UnicodeCharacters::CarriageReturn) {
                continue;
            }
            if (character == StringUtils::UnicodeCharacters::NextCharNoChangeX) {
                continue;
            }
            if (character == 0xFEFF) {
                continue;
            }
            
            if (!getFontLetterDef(character, letterDef)) {
                CCLOG("rtlWrapAndNormalize error: can't find letter definition in font file for letter: 0x%x", character);
                continue;
            }
        
            auto letterWidth = (letterDef.xAdvance * _bmfontScale + _additionalKerning) / contentScaleFactor;
            if (letterIndex < len - 1)
                letterWidth += _fontAtlas->getFont()->getHorizontalKerningForChars(shaped_str[letterIndex],shaped_str[letterIndex+1]) / contentScaleFactor;
        
            if (_enableWrap 
                && curWidth > 0  // prevents case where maxLineWidth is < character width
                && _maxLineWidth > 0.f 
                && curWidth + tokenWidth + letterWidth >= _maxLineWidth) {

                CCLOG(" -break at idx:%i tmp:%i  x:%f actual-x:%f", idx, tmp, curWidth+tokenWidth+letterWidth, curWidth);
                PUSH(lineStart, idx-lineStart); // push up to the prev token
                curWidth = 0;
                lineStart = idx;
                goto NEXT_TOKEN;

            } else {
                CCLOG("  adding char:%x at idx:%i tmp:%i x:%f", character, idx, tmp, curWidth+tokenWidth+letterWidth);
                tokenWidth += letterWidth;
            }
        }
        idx += tokenLen;
        curWidth += tokenWidth;

        NEXT_TOKEN:
        {}
    }    
    // add the last line
    if (curWidth > 0) {
        PUSH(lineStart, len-lineStart);
    }

    delete[] shaped_str;        
    delete[] bidi_types;
    delete[] bracket_types;
    delete[] embedding_levels;
    delete[] ar_props;

    // concatenate the lines with \n
    std::string out;
    for (const auto& line32 : lines32) {
        // convert to utf8
        std::string line;
        StringUtils::UTF32ToUTF8(line32, line);        
        out += line;
        if (line32 != lines32.back())
            out += StringUtils::UnicodeCharacters::NewLine;
    }

    // remove ZWNBSP char 0xFEFF
    auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
        if (from.empty ())
            return;
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length (); // for the case where 'to' contains 'from'
        }
    };
    replace_all(out, "\uFEFF", "");

    return out;
}

NS_CC_END
