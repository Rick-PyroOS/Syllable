#ifndef __GRAPHICS_THEME_STYLE_BASE_H__
#define __GRAPHICS_THEME_STYLE_BASE_H__

#include <graphics/fillstyle.h>
#include <graphics/penstyle.h>
#include "border.h"

#include <gui/rect.h>
#include <storage/file.h>


namespace graphics{
    namespace theme{
        namespace style{
            class Style {
                public:
                        Style();

                        void SetFillStyle(const graphics::FillStyle&) const;
                        graphics::FillStyle GetFillStyle() const;

                        void SetPenStyle(const graphics::PenStyle&) const;
                        graphics::PenStyle GetPenStyle();

                        void SetInnerBorderStyle(const graphics::theme::style::BorderStyle& );
                        graphics::theme::style::BorderStyle GetInnerBorderStyle() const;

                        void SetOuterBorderStyle(const graphics::theme::style::BorderStyle& );
                        graphics::theme::style::BorderStyle GetOuterBorderStyle() const;

                        void SetTextPenStyle(const graphics::PenStyle& ) const;
                        graphics::PenStyle GetTextPenStyle();

                        void SetBackgroundFillStyle(const graphics::FillStyle&);
                        graphics::FillStyle SetBackgroundFillStyle(const graphics::FillStyle);

                        os::Rect GetMargin() const;
                        void SetMargin(const os::Rect&);

                         os::Rect GetPadding() const;
                         void SetPadding(const os::Rect&);
                private:
                    class Private;
                    Private* m;
                private:
                        void __ReadFromFile__(const os::File&);
                        void __Init__();
                };
        }
    }
}

#endif
