#ifndef __GRAPHICS_THEME_STYLE_BORDER_H__
#define __GRAPHICS_THEME_STYLE_BORDER_H__

#include <graphics/penstyle.h>
#include <graphics/shape.h>

namespace graphics
{
    namespace theme
    {
        namespace style
        {
            class BorderStyle {
                public:
                    enum LineStyle {
                        SOLID=1,
                        DASH,
                        DOTTED,
                        DASHDOTTED,
                        DOUBLE,
                        DIVIDE,
                        CENTER,
                        BORDER,
                    };
                public:
                  BorderStyle(){
                        nLineStyle = SOLID;
                        nRadius = 0;
                        cMargin = os::Rect(2,2,-2,-2);
                        cPenStyleColour = "black";
                        vPenStyleWidth = 1;
                    }


                    int GetRadius() const {
                        return nRadius;
                    }
                    void SetRadius(const int& radius){
                        nRadius = radius;
                    }

                    const os::String GetPenStyleColour(){
                        return cPenStyleColour;
                    }
                    void SetPenStyleColour(const os::String& colour){
                        cPenStyleColour = colour;
                    }

                    double GetPenStyleWidth() const{
                        return vPenStyleWidth;
                    }
                    void SetPenStyleWidth(const double& width){
                        vPenStyleWidth = width;
                    }

                    graphics::PenStyle GetPenStyle(){
                        graphics::PenStyle style = graphics::PenStyle(GetPenStyleColour(),GetPenStyleWidth());
                        style.SetDashes(GetLineStyle());
                        return style;
                    }

                    void SetLineStyle(LineStyle lineStyle){
                        nLineStyle = lineStyle;
                    }
                    os::String GetLineStyle() const{
                        switch(nLineStyle){
                            case SOLID:
                                return "solid";
                            case DASH:
                                return "dashed";
                            case DOTTED:
                                return "dotted";
                            case DASHDOTTED:
                                return "dashdotted";
                            case DIVIDE:
                                return "divide";
                            case CENTER:
                                return "center";
                            case BORDER:
                                return "border";
                            default:
                                return "solid";
                        }
                        return "solid";
                    }

                    os::Rect GetMargin() const{
                        return cMargin;
                    }

                    void SetMargin(const os::Rect& margin){
                        cMargin = margin;
                    }


                    graphics::Shape GetShape(const os::Rect& cRect) const{
                       graphics::Shape shape;
                       int radius =  GetRadius();
                       os::Rect margin = GetMargin();
                       os::Rect rect = cRect;
                       rect.Resize(margin.left,margin.top,margin.right,margin.bottom);
                       if (radius == 0){
                            shape.Rectangle(rect);
                       }
                       else{
                            shape.RoundRectangle(rect,radius);
                       }
                       return shape;
                    }

            private:
                double                  vPenStyleWidth;
                os::String              cPenStyleColour;
                int                     nRadius;
                LineStyle               nLineStyle;
                os::Rect                cMargin;
            };
        }
    }
}
#endif
